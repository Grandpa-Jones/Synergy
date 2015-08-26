#include "walletmodel.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "turboaddresstablemodel.h"
#include "transactiontablemodel.h"

#include "ui_interface.h"
#include "wallet.h"
#include "walletdb.h" // for BackupWallet
#include "base58.h"

#include <QSet>
#include <QTimer>

extern bool fRescanLock;

WalletModel::WalletModel(CWallet *wallet, OptionsModel *optionsModel, QObject *parent) :
    QObject(parent), wallet(wallet), optionsModel(optionsModel), addressTableModel(0),
    turboAddressTableModel(0), transactionTableModel(0),
    cachedBalance(0), cachedStake(0), cachedUnconfirmedBalance(0), cachedImmatureBalance(0),
    cachedNumTransactions(0),
    cachedEncryptionStatus(Unencrypted),
    cachedNumBlocks(0)
{
    addressTableModel = new AddressTableModel(wallet, this);
    turboAddressTableModel = new TurboAddressTableModel(wallet, this);
    transactionTableModel = new TransactionTableModel(wallet, this);

    // This timer will be fired repeatedly to update the balance
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(pollBalanceChanged()));
    pollTimer->start(MODEL_UPDATE_DELAY);

    subscribeToCoreSignals();
}

WalletModel::~WalletModel()
{
    unsubscribeFromCoreSignals();
}

qint64 WalletModel::getBalance() const
{
    return wallet->GetBalance();
}

qint64 WalletModel::getUnconfirmedBalance() const
{
    return wallet->GetUnconfirmedBalance();
}

qint64 WalletModel::getStake() const
{
    return wallet->GetStake();
}

qint64 WalletModel::getImmatureBalance() const
{
    return wallet->GetImmatureBalance();
}

int WalletModel::getNumTransactions() const
{
    int numTransactions = 0;
    {
        LOCK(wallet->cs_wallet);
        numTransactions = wallet->mapWallet.size();
    }
    return numTransactions;
}

void WalletModel::updateStatus()
{
    EncryptionStatus newEncryptionStatus = getEncryptionStatus();

    if(cachedEncryptionStatus != newEncryptionStatus)
        emit encryptionStatusChanged(newEncryptionStatus);
}

void WalletModel::pollBalanceChanged()
{
    // Get required locks upfront. This avoids the GUI from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    TRY_LOCK(cs_main, lockMain);
    if(!lockMain)
        return;
    TRY_LOCK(wallet->cs_wallet, lockWallet);
    if(!lockWallet)
        return;

    if(nBestHeight != cachedNumBlocks)
    {
        // Balance and number of transactions might have changed
        cachedNumBlocks = nBestHeight;

        checkBalanceChanged();
        if(transactionTableModel)
            transactionTableModel->updateConfirmations();
    }
}

void WalletModel::checkBalanceChanged()
{
    qint64 newBalance = getBalance();
    qint64 newStake = getStake();
    qint64 newUnconfirmedBalance = getUnconfirmedBalance();
    qint64 newImmatureBalance = getImmatureBalance();

    if(cachedBalance != newBalance || cachedStake != newStake || cachedUnconfirmedBalance != newUnconfirmedBalance || cachedImmatureBalance != newImmatureBalance)
    {
        cachedBalance = newBalance;
        cachedStake = newStake;
        cachedUnconfirmedBalance = newUnconfirmedBalance;
        cachedImmatureBalance = newImmatureBalance;
        emit balanceChanged(newBalance, newStake, newUnconfirmedBalance, newImmatureBalance);
    }
}

void WalletModel::updateTransaction(const QString &hash, int status)
{

    if (fRescanLock) {
          return;
    }

    if(transactionTableModel)
        transactionTableModel->updateTransaction(hash, status);

    // Balance and number of transactions might have changed
    checkBalanceChanged();

    int newNumTransactions = getNumTransactions();
    if(cachedNumTransactions != newNumTransactions)
    {
        cachedNumTransactions = newNumTransactions;
        emit numTransactionsChanged(newNumTransactions);
    }
}


void WalletModel::updateAddressBook(const QString &address, const QString &label, bool isMine, int status)
{
    if(addressTableModel)
        addressTableModel->updateEntry(address, label, isMine, status);
}

void WalletModel::updateTurboAddresses()
{
    turboAddressTableModel->update();
}

void WalletModel::updateTurbo()
{
    this->updateTurboAddresses();
}

bool WalletModel::validateAddress(const QString &address)
{
    std::string sAddr = address.toStdString();

    if (sAddr.length() > 75)
    {
        if (IsStealthAddress(sAddr))
            return true;
    };

    CBitcoinAddress addressParsed(sAddr);
    return addressParsed.IsValid();
}

WalletModel::SendCoinsReturn WalletModel::sendCoins(const QString &txcomment, const QList<SendCoinsRecipient> &recipients, unsigned int nProdTypeID, const CCoinControl *coinControl)
{
    qint64 total = 0;
    QSet<QString> setAddress;
    QString hex;

    if(recipients.empty())
    {
        return OK;
    }

    // Pre-check input data for validity
    foreach(const SendCoinsRecipient &rcp, recipients)
    {
        if(!validateAddress(rcp.address))
        {
            return InvalidAddress;
        }
        setAddress.insert(rcp.address);

        if(rcp.amount <= 0)
        {
            return InvalidAmount;
        }
        total += rcp.amount;
    }

    if(recipients.size() > setAddress.size())
    {
        return DuplicateAddress;
    }

    int64_t nBalance = 0;
    std::vector<COutput> vCoins;
    wallet->AvailableCoins(vCoins, true, coinControl);

    BOOST_FOREACH(const COutput& out, vCoins)
        nBalance += out.tx->vout[out.i].nValue;

    if(total > nBalance)
    {
        return AmountExceedsBalance;
    }

    if((total + nTransactionFee) > nBalance)
    {
        return SendCoinsReturn(AmountWithFeeExceedsBalance, nTransactionFee);
    }

    std::map<int, std::string> mapStealthNarr;

    {
        LOCK2(cs_main, wallet->cs_wallet);

        CWalletTx wtx;

        // Sendmany
        std::vector<std::pair<CScript, int64_t> > vecSend;
        foreach(const SendCoinsRecipient &rcp, recipients)
        {
            std::string sAddr = rcp.address.toStdString();

            if (rcp.typeInd == AddressTableModel::AT_Stealth)
            {
                CStealthAddress sxAddr;
                if (sxAddr.SetEncoded(sAddr))
                {
                    ec_secret ephem_secret;
                    ec_secret secretShared;
                    ec_point pkSendTo;
                    ec_point ephem_pubkey;


                    if (GenerateRandomSecret(ephem_secret) != 0)
                    {
                        printf("GenerateRandomSecret failed.\n");
                        return Aborted;
                    };

                    if (StealthSecret(ephem_secret, sxAddr.scan_pubkey, sxAddr.spend_pubkey, secretShared, pkSendTo) != 0)
                    {
                        printf("Could not generate receiving public key.\n");
                        return Aborted;
                    };

                    CPubKey cpkTo(pkSendTo);
                    if (!cpkTo.IsValid())
                    {
                        printf("Invalid public key generated.\n");
                        return Aborted;
                    };

                    CKeyID ckidTo = cpkTo.GetID();

                    CBitcoinAddress addrTo(ckidTo);

                    if (SecretToPublicKey(ephem_secret, ephem_pubkey) != 0)
                    {
                        printf("Could not generate ephem public key.\n");
                        return Aborted;
                    };

                    if (fDebug)
                    {
                        printf("Stealth send to generated pubkey %"PRIszu": %s\n", pkSendTo.size(), HexStr(pkSendTo).c_str());
                        printf("hash %s\n", addrTo.ToString().c_str());
                        printf("ephem_pubkey %"PRIszu": %s\n", ephem_pubkey.size(), HexStr(ephem_pubkey).c_str());
                    };

                    CScript scriptPubKey;
                    scriptPubKey.SetDestination(addrTo.Get());
                    vecSend.push_back(make_pair(scriptPubKey, rcp.amount));

                    CScript scriptP = CScript() << OP_RETURN << ephem_pubkey;

                    if (rcp.narration.length() > 0)
                    {
                        std::string sNarr = rcp.narration.toStdString();

                        if (sNarr.length() > 24)
                        {
                            printf("Narration is too long.\n");
                            return NarrationTooLong;
                        };

                        std::vector<unsigned char> vchNarr;
                        
                        SecMsgCrypter crypter;
                        crypter.SetKey(&secretShared.e[0], &ephem_pubkey[0]);
                        
                        if (!crypter.Encrypt((uint8_t*)&sNarr[0], sNarr.length(), vchNarr))
                        {
                            printf("Narration encryption failed.\n");
                            return Aborted;
                        };
                        
                        if (vchNarr.size() > 48)
                        {
                            printf("Encrypted narration is too long.\n");
                            return Aborted;
                        };
                        
                        if (vchNarr.size() > 0)
                            scriptP = scriptP << OP_RETURN << vchNarr;
                        
                        int pos = vecSend.size()-1;
                        mapStealthNarr[pos] = sNarr;
                    };
                    
                    vecSend.push_back(make_pair(scriptP, 0));
                    
                    continue;
                }; // else drop through to normal
            }
            
            CScript scriptPubKey;
            scriptPubKey.SetDestination(CBitcoinAddress(sAddr).Get());
            vecSend.push_back(make_pair(scriptPubKey, rcp.amount));
            
            
            
            
            if (rcp.narration.length() > 0)
            {
                std::string sNarr = rcp.narration.toStdString();
                
                if (sNarr.length() > 24)
                {
                    printf("Narration is too long.\n");
                    return NarrationTooLong;
                };
                
                std::vector<uint8_t> vNarr(sNarr.c_str(), sNarr.c_str() + sNarr.length());
                std::vector<uint8_t> vNDesc;
                
                vNDesc.resize(2);
                vNDesc[0] = 'n';
                vNDesc[1] = 'p';
                
                CScript scriptN = CScript() << OP_RETURN << vNDesc << OP_RETURN << vNarr;
                
                vecSend.push_back(make_pair(scriptN, 0));
            }
        }


        CReserveKey keyChange(wallet);
        int64_t nFeeRequired = 0;
	std::string strTxComment = txcomment.toStdString();

        int nChangePos = -1;
        bool fCreated = wallet->CreateTransaction(vecSend, wtx, keyChange, nFeeRequired, nChangePos, strTxComment, nProdTypeID, coinControl);

        std::map<int, std::string>::iterator it;
        for (it = mapStealthNarr.begin(); it != mapStealthNarr.end(); ++it)
        {
            int pos = it->first;
            if (nChangePos > -1 && it->first >= nChangePos)
                pos++;

            char key[64];
            if (snprintf(key, sizeof(key), "n_%u", pos) < 1)
            {
                printf("CreateStealthTransaction(): Error creating narration key.");
                continue;
            };
            wtx.mapValue[key] = it->second;
        };

        // in some cases the fee required is less than the minimum fee
        if(!fCreated)
        {
            if((total + nFeeRequired) > nBalance) // FIXME: could cause collisions in the future
            {
                return SendCoinsReturn(AmountWithFeeExceedsBalance, nFeeRequired);
            }
            return TransactionCreationFailed;
        }
        if(!uiInterface.ThreadSafeAskFee(nFeeRequired, tr("Sending...").toStdString()))
        {
            return Aborted;
        }
        if(!wallet->CommitTransaction(wtx, keyChange))
        {
            return TransactionCommitFailed;
        }
        hex = QString::fromStdString(wtx.GetHash().GetHex());
    }

    // Add addresses / update labels that we've sent to to the address book
    foreach(const SendCoinsRecipient &rcp, recipients)
    {
        std::string strAddress = rcp.address.toStdString();
        CTxDestination dest = CBitcoinAddress(strAddress).Get();
        std::string strLabel = rcp.label.toStdString(); {
            LOCK(wallet->cs_wallet);
            if (rcp.typeInd == AddressTableModel::AT_Stealth) {
                  wallet->UpdateStealthAddress(strAddress, strLabel, true);
            } else {
                  std::map<CTxDestination, std::string>::iterator mi =
                                              wallet->mapAddressBook.find(dest);
                  // Check if we have a new address or an updated label
                  if (mi == wallet->mapAddressBook.end() || mi->second != strLabel) {
                       wallet->SetAddressBookName(dest, strLabel);
                  }
            }
        }
    }

    return SendCoinsReturn(OK, 0, hex);
}

OptionsModel *WalletModel::getOptionsModel()
{
    return optionsModel;
}

AddressTableModel *WalletModel::getAddressTableModel()
{
    return addressTableModel;
}

TurboAddressTableModel *WalletModel::getTurboAddressTableModel()
{
    return turboAddressTableModel;
}

TransactionTableModel *WalletModel::getTransactionTableModel()
{
    return transactionTableModel;
}

WalletModel::EncryptionStatus WalletModel::getEncryptionStatus() const
{
    if(!wallet->IsCrypted())
    {
        return Unencrypted;
    }
    else if(wallet->IsLocked())
    {
        return Locked;
    }
    else
    {
        return Unlocked;
    }
}

bool WalletModel::setWalletEncrypted(bool encrypted, const SecureString &passphrase)
{
    if(encrypted)
    {
        // Encrypt
        return wallet->EncryptWallet(passphrase);
    }
    else
    {
        // Decrypt -- TODO; not supported yet
        return false;
    }
}

bool WalletModel::setWalletLocked(bool locked, const SecureString &passPhrase)
{
    if(locked)
    {
        // Lock
        return wallet->Lock();
    }
    else
    {
        // Unlock
        return wallet->Unlock(passPhrase);
    }
}

bool WalletModel::changePassphrase(const SecureString &oldPass, const SecureString &newPass)
{
    bool retval;
    {
        LOCK(wallet->cs_wallet);
        wallet->Lock(); // Make sure wallet is locked before attempting pass change
        retval = wallet->ChangeWalletPassphrase(oldPass, newPass);
    }
    return retval;
}

bool WalletModel::backupWallet(const QString &filename)
{
    return BackupWallet(*wallet, filename.toLocal8Bit().data());
}

// Handlers for core signals
static void NotifyKeyStoreStatusChanged(WalletModel *walletmodel, CCryptoKeyStore *wallet)
{
    OutputDebugStringF("NotifyKeyStoreStatusChanged\n");
    QMetaObject::invokeMethod(walletmodel, "updateStatus", Qt::QueuedConnection);
}

static void NotifyAddressBookChanged(WalletModel *walletmodel, CWallet *wallet, const CTxDestination &address, const std::string &label, bool isMine, ChangeType status)
{

    if (address.type() == typeid(CStealthAddress))
    {
        CStealthAddress sxAddr = boost::get<CStealthAddress>(address);
        std::string enc = sxAddr.Encoded();
        OutputDebugStringF("NotifyAddressBookChanged %s %s isMine=%i status=%i\n", enc.c_str(), label.c_str(), isMine, status);
        QMetaObject::invokeMethod(walletmodel, "updateAddressBook", Qt::QueuedConnection,
                                  Q_ARG(QString, QString::fromStdString(enc)),
                                  Q_ARG(QString, QString::fromStdString(label)),
                                  Q_ARG(bool, isMine),
                                  Q_ARG(int, status));
    } else
    {
    OutputDebugStringF("NotifyAddressBookChanged %s %s isMine=%i status=%i\n", CBitcoinAddress(address).ToString().c_str(), label.c_str(), isMine, status);
    QMetaObject::invokeMethod(walletmodel, "updateAddressBook", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(CBitcoinAddress(address).ToString())),
                              Q_ARG(QString, QString::fromStdString(label)),
                              Q_ARG(bool, isMine),
                              Q_ARG(int, status));
    }
}

static void NotifyTransactionChanged(WalletModel *walletmodel, CWallet *wallet, const uint256 &hash, ChangeType status)
{
    OutputDebugStringF("NotifyTransactionChanged %s status=%i\n", hash.GetHex().c_str(), status);
    QMetaObject::invokeMethod(walletmodel, "updateTransaction", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(hash.GetHex())),
                              Q_ARG(int, status));
}

static void NotifyBlocksChanged(WalletModel *walletmodel)
{
    if (!IsInitialBlockDownload() && !fRescanLock) {
          walletmodel->updateTurbo();
    }
}


void WalletModel::subscribeToCoreSignals()
{
    // Connect signals to wallet
    wallet->NotifyStatusChanged.connect(boost::bind(&NotifyKeyStoreStatusChanged, this, _1));
    wallet->NotifyAddressBookChanged.connect(boost::bind(NotifyAddressBookChanged, this, _1, _2, _3, _4, _5));
    wallet->NotifyTransactionChanged.connect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
    uiInterface.NotifyBlocksChanged.connect(boost::bind(NotifyBlocksChanged, this));
}

void WalletModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from wallet
    wallet->NotifyStatusChanged.disconnect(boost::bind(&NotifyKeyStoreStatusChanged, this, _1));
    wallet->NotifyAddressBookChanged.disconnect(boost::bind(NotifyAddressBookChanged, this, _1, _2, _3, _4, _5));
    wallet->NotifyTransactionChanged.disconnect(boost::bind(NotifyTransactionChanged, this, _1, _2, _3));
    uiInterface.NotifyBlocksChanged.disconnect(boost::bind(NotifyBlocksChanged, this));
}

// WalletModel::UnlockContext implementation
WalletModel::UnlockContext WalletModel::requestUnlock()
{
    bool was_locked = getEncryptionStatus() == Locked;
    
    if ((!was_locked) && fWalletUnlockStakingOnly)
    {
       setWalletLocked(true);
       was_locked = getEncryptionStatus() == Locked;
    }
    if(was_locked)
    {
        // Request UI to unlock wallet
        emit requireUnlock();
    }
    // If wallet is still locked, unlock was failed or cancelled, mark context as invalid
    bool valid = getEncryptionStatus() != Locked;

    return UnlockContext(this, valid, was_locked && !fWalletUnlockStakingOnly);
}

WalletModel::UnlockContext::UnlockContext(WalletModel *wallet, bool valid, bool relock):
        wallet(wallet),
        valid(valid),
        relock(relock)
{
}

WalletModel::UnlockContext::~UnlockContext()
{
    if(valid && relock)
    {
        wallet->setWalletLocked(true);
    }
}

void WalletModel::UnlockContext::CopyFrom(const UnlockContext& rhs)
{
    // Transfer context; old object no longer relocks wallet
    *this = rhs;
    rhs.relock = false;
}

bool WalletModel::getPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const
{
    return wallet->GetPubKey(address, vchPubKeyOut);   
}

// returns a list of COutputs from COutPoints
void WalletModel::getOutputs(const std::vector<COutPoint>& vOutpoints, std::vector<COutput>& vOutputs)
{
    LOCK2(cs_main, wallet->cs_wallet);
    BOOST_FOREACH(const COutPoint& outpoint, vOutpoints)
    {
        if (!wallet->mapWallet.count(outpoint.hash)) continue;
        int nDepth = wallet->mapWallet[outpoint.hash].GetDepthInMainChain();
        if (nDepth < 0) continue;
        COutput out(&wallet->mapWallet[outpoint.hash], outpoint.n, nDepth);
        vOutputs.push_back(out);
    }
}

// AvailableCoins + LockedCoins grouped by wallet address (put change in one group with wallet address) 
void WalletModel::listCoins(std::map<QString, std::vector<COutput> >& mapCoins) const
{
    std::vector<COutput> vCoins;
    wallet->AvailableCoins(vCoins);

    LOCK2(cs_main, wallet->cs_wallet); // ListLockedCoins, mapWallet
    std::vector<COutPoint> vLockedCoins;

    // add locked coins
    BOOST_FOREACH(const COutPoint& outpoint, vLockedCoins)
    {
        if (!wallet->mapWallet.count(outpoint.hash)) continue;
        int nDepth = wallet->mapWallet[outpoint.hash].GetDepthInMainChain();
        if (nDepth < 0) continue;
        COutput out(&wallet->mapWallet[outpoint.hash], outpoint.n, nDepth);
        vCoins.push_back(out);
    }

    BOOST_FOREACH(const COutput& out, vCoins)
    {
        COutput cout = out;

        while (wallet->IsChange(cout.tx->vout[cout.i]) && cout.tx->vin.size() > 0 && wallet->IsMine(cout.tx->vin[0]))
        {
            if (!wallet->mapWallet.count(cout.tx->vin[0].prevout.hash)) break;
            cout = COutput(&wallet->mapWallet[cout.tx->vin[0].prevout.hash], cout.tx->vin[0].prevout.n, 0);
        }

        CTxDestination address;
        if(!ExtractDestination(cout.tx->vout[cout.i].scriptPubKey, address)) continue;
        mapCoins[CBitcoinAddress(address).ToString().c_str()].push_back(out);
    }
}

// Available + LockedCoins assigned to each address
void WalletModel::listAddresses(std::map<QString, int64_t>& mapAddrs) const
{
    std::map<QString, std::vector<COutput> > mapCoins;
    // not perfectly efficient, but it should do
    this->listCoins(mapCoins);

    std::map<QString, std::vector<COutput> >::iterator it;
    for(it = mapCoins.begin(); it != mapCoins.end(); ++it)
    {
       BOOST_FOREACH(const COutput &out, it->second)
       {
           COutput cout = out;
           CTxDestination destAddr;
           if(!ExtractDestination(cout.tx->vout[cout.i].scriptPubKey, destAddr))
           {
              // should never fail because it just succeeded
              continue;
           }
           // need to get address because they may be change
           QString qAddr(CBitcoinAddress(destAddr).ToString().c_str());
           std::map<QString, int64_t>::iterator qit;
           qit = mapAddrs.find(qAddr);
           if (qit == mapAddrs.end())
           {
               mapAddrs[qAddr] = cout.tx->vout[cout.i].nValue;
           }
           else
           {
               qit->second += cout.tx->vout[cout.i].nValue;
           }
       }
    }
}

// [TODO] refactor with listCoins 
// group coins by address, don't group change addresses with predecessor
void WalletModel::groupAddresses(std::map<QString, std::vector<COutput> >& mapCoins) const
{
    std::vector<COutput> vCoins;
    wallet->AvailableCoins(vCoins);

    LOCK2(cs_main, wallet->cs_wallet); // ListLockedCoins, mapWallet
    std::vector<COutPoint> vLockedCoins;

    // add locked coins (seems like it never does anything???)
    BOOST_FOREACH(const COutPoint& outpoint, vLockedCoins)
    {
        if (!wallet->mapWallet.count(outpoint.hash)) continue;
        int nDepth = wallet->mapWallet[outpoint.hash].GetDepthInMainChain();
        if (nDepth < 0) continue;
        COutput out(&wallet->mapWallet[outpoint.hash], outpoint.n, nDepth);
        vCoins.push_back(out);
    }

    BOOST_FOREACH(const COutput& out, vCoins)
    {
        CTxDestination address;
        if(!ExtractDestination(out.tx->vout[out.i].scriptPubKey, address)) continue;
        mapCoins[CBitcoinAddress(address).ToString().c_str()].push_back(out);
    }
}

// find all coins sent to the address addr
void WalletModel::spentCoinsToAddress(std::vector<COutput>& vCoins, QString& qsAddr) const
{
    std::string sAddr = qsAddr.toStdString();

    std::vector<COutput> vcns;
    this->wallet->SpentCoins(vcns);

    vCoins.clear();
    std::vector<COutput>::const_iterator it;
    for (it = vcns.begin(); it != vcns.end(); ++it)
    {
        CTxDestination destaddr;
        if (!ExtractDestination(it->tx->vout[it->i].scriptPubKey, destaddr)) continue;
        if (sAddr == CBitcoinAddress(destaddr).ToString())
        {
              vCoins.push_back(COutput(it->tx, it->i, it->nDepth));
        }
    }
}


// these will be in reverse chronological order
bool WalletModel::listCoinsFromAddress(QString &qsAddr, int64_t lookback, std::vector<COutput>& vOut) const
{
    std::string sAddr = qsAddr.toStdString();
    std::map<QString, std::vector<COutput> > mapCoins;
    this->groupAddresses(mapCoins);
    std::map<QString, std::vector<COutput> >::iterator it;
    for (it = mapCoins.begin(); it != mapCoins.end(); ++it)
    {
        if (fDebug)
        {
               printf("listCoinsFromAddress(): adress group %s\n", it->first.toStdString().c_str());
        }
        std::vector<COutput>::const_iterator oit;
        for (oit = it->second.begin(); oit != it->second.end(); ++oit)
        {
            if (oit->tx->nTime < lookback)
            {
                   break;
            }
            if (oit->tx->IsCoinBase() || oit->tx->IsCoinStake())
            {
                   continue;
            }
            if (fDebug)
            {
                   printf("listCoinsFromAddress(): txid %s\n", oit->tx->GetHash().ToString().c_str());
            }
            std::vector<CTxIn> vin = oit->tx->vin;
            std::vector<CTxIn>::const_iterator iit;
            bool fFoundMatchingInput = false;
            for (iit = vin.begin(); iit != vin.end(); ++iit)
            {
                 // wallet tx with the previous output
                 CWalletTx *pcoin = &(wallet->mapWallet[iit->prevout.hash]);
                 if (!pcoin->IsTrusted())
                 {
                      if (fDebug)
                      {
                            printf("listCoinsFromAddress(): prevout %s has untrusted input\n",
                                         pcoin->GetHash().ToString().c_str());
                      }
                      continue;
                 }
                 int nDepth = pcoin->GetDepthInMainChain();
                 int nReq;
                 txnouttype txType;
                 std::vector<CTxDestination> vDest;

                 if (ExtractDestinations(pcoin->vout[iit->prevout.n].scriptPubKey, txType, vDest, nReq))
                 {
                      BOOST_FOREACH(const CTxDestination& sigaddr, vDest)
                      {
                            if (fDebug)
                            {
                                    printf("listCoinsFromAddress(): checking\n   my %s =\n       %s\n",
                                               sAddr.c_str(), CBitcoinAddress(sigaddr).ToString().c_str());
                            }
                            if (sAddr == CBitcoinAddress(sigaddr).ToString())
                            {
                                    // vOut.push_back(COutput(pcoin, iit->prevout.n, nDepth));
                                    fFoundMatchingInput = true;
                                    break; // at least one prevout dest matched, no need to continue
                            }
                      }
                 }
                 else
                 {
                      if (fDebug) {
                           printf("listCoinsFromAddress(): Could not extract from destination.\n");
                      }
                      continue;
                 }
                 if (fFoundMatchingInput)
                 {
                       break;  // at least one prevout matched no need to continue
                 }
            }
            if (fFoundMatchingInput)
            {
                    if (fDebug) {
                          printf("listCoinsFromAddress(): adding tx %s.\n",
                                        oit->tx->GetHash().ToString().c_str());
                    }
                    vOut.push_back(*oit);
            }
        }
    }
    return (vOut.size() > 0);
}

bool WalletModel::findStealthTransactions(const CTransaction& tx, mapValue_t& mapNarr)
{
    CWalletTx wtx;
    // assume that the the tx pointed to by the prevout (input) to an output to me
    // is not necessarily the same object as the object as the output in my wallet
    //    me [outputA]--> other [outputB]--> me
    //           | outputA in my wallet as input to outputB
    //    me [outputA']--> other
    //           | outputA' in my wallet as output
    // so even though outputA = outputA', they are represented as
    // two different objects in wallet(?)
    std::map<uint256, CWalletTx>::const_iterator it = this->wallet->mapWallet.find(tx.GetHash());
    if (it == this->wallet->mapWallet.end())
    {
         printf("WalletModel::findStealthTransactions(): tx %s not in wallet\n",
                                                          tx.GetHash().ToString().c_str());
         return false;
    }
    return this->wallet->FindStealthTransactions(it->second, mapNarr);
}

// returns false if vin is empty, prevout tx isn't in wallet or is not trusted
bool WalletModel::getFirstPrevoutForTx(const CWalletTx &wtx, COutput &inOut)
{
    if (wtx.vin.empty())
    {
         printf("getFirstPrevOutForTx(): tx has no inputs\n");
         return false;
    }
    COutPoint inPt = wtx.vin[0].prevout;
    std::map<uint256, CWalletTx>::const_iterator it = this->wallet->mapWallet.find(inPt.hash);
    if (it == this->wallet->mapWallet.end())
    {
         printf("getFirstPrevOutForTx(): tx not in wallet\n");
         return false;
    }
    if (!it->second.IsTrusted())
    {
         return false;
    }
    inOut.tx = &it->second;
    inOut.i = inPt.n;
    inOut.nDepth = it->second.GetDepthInMainChain();
    return true;
}

bool WalletModel::getBlocksInInterval(int64_t start, int64_t end,
                                      std::pair<CBlockIndex*,CBlockIndex*> &blocks)
{
    if (start > end)
    {
          int64_t tmp;
          tmp = start;
          start = end;
          end = tmp;
    }

    if ((start == end) || (end < pindexGenesisBlock->nTime) || (start > pindexBest->nTime))
    {
          printf("getBlocksInInterval(): no overlap\n");
          return false;
    }

    bool fStartFound = false;
    bool fEndFound = false;

    if (start < pindexGenesisBlock->nTime)
    {
          blocks.first = pindexGenesisBlock;
          fStartFound = true;
    }

    if (end > pindexBest->nTime)
    {
          blocks.second = pindexBest;
          fEndFound = true;
    }


    if (!(fStartFound && fEndFound))
    {
          // work our way backwards
          CBlockIndex* pindex = pindexBest;
          while (pindex->pprev != NULL)
          {
               if (!fEndFound && (pindex->nTime <= end))
               {
                      blocks.second = pindex;
                      fEndFound = true;
               }
               if (!fStartFound && (pindex->pprev->nTime < start))
               {
                      fStartFound = true;
                      blocks.first = pindex;
                      break;
               }
               pindex = pindex->pprev;
          }
    }

    // this should never test true
    if (!(fStartFound && fEndFound))
    {
          printf("getBlocksInInterval(): blocks not found\n");
          return false;
    }
 
    return true;
}


bool WalletModel::GetAddressBalancesInInterval(std::string &sAddress,
                                               int64_t start, int64_t end,
                                               std::vector<std::pair<uint256, int64_t> > &balancesRet,
                                               int64_t &minBalanceRet, int64_t &maxBalanceRet)
{
    if (start == end)
    {
          printf("GetAddressBalancesInInterval(): 0 length interval\n");
          return false;
    }
    if (start > end)
    {
          int64_t tmp;
          tmp = start;
          start = end;
          end = tmp;
    }
    if ((start > pindexBest->nTime) || (end < pindexGenesisBlock->nTime))
    {
          printf("GetAddressBalancesInInterval(): no overlap\n");
          return false;
    }

    std::vector<std::pair<uint256, CWalletTx*> > vpWTx;
    for (std::map<uint256, CWalletTx>::iterator it = this->wallet->mapWallet.begin();
                                                it != this->wallet->mapWallet.end(); ++it)
    {
           if(it->second.IsTrusted() && (it->second.nTime <= end))
           {
                 CWalletTx *pcoin = &(*it).second;
                 vpWTx.push_back(std::make_pair(it->first, pcoin));
           }
    }

    StructPairpCWTxTimeSorter pairpCWTxTimeSorter;
    std::sort(vpWTx.begin(), vpWTx.end(), pairpCWTxTimeSorter);

    // minimum balance during interval
    minBalanceRet = 0;
    maxBalanceRet = 0;

    // running balance
    int64_t balance = 0;
    // turns true when in interval, better than starting min balance at -1
    bool fHitInterval = false;
    std::vector<std::pair<uint256, CWalletTx*> >::iterator it;
    for (it = vpWTx.begin(); it != vpWTx.end(); ++it)
    {
         CWalletTx *pcoin = it->second;
         int64_t outtx = 0;
         int64_t intx = 0;
         std::vector<CTxOut>::const_iterator oit;
         for (oit = pcoin->vout.begin(); oit != pcoin->vout.end(); ++oit)
         {
             CTxDestination destaddr;
             if (!ExtractDestination(oit->scriptPubKey, destaddr))
             {
                  continue;
             }
             if (sAddress != CBitcoinAddress(destaddr).ToString())
             {
                  continue;
             }
             outtx += oit->nValue;
         }
         std::vector<CTxIn>::const_iterator iit;
         for (iit = pcoin->vin.begin(); iit != pcoin->vin.end(); ++iit)
         {
             CTxOut inout;
             if (this->wallet->mapWallet.count(iit->prevout.hash))
             {
                   CWalletTx tmpwtx = this->wallet->mapWallet[iit->prevout.hash];
                   if (tmpwtx.vout.size() > iit->prevout.n)
                   {
                       inout = tmpwtx.vout[iit->prevout.n];
                   }
                   else
                   {
                       printf("GetAddressBalancesInInterval(): prevout %d not in wallet transaction %s\n",
                                                  iit->prevout.n, iit->prevout.hash.ToString().c_str());
                       // try to read from block index
                       CTransaction tmptx;
                       uint256 hashBlock = 0;
                       if (GetTransaction(iit->prevout.hash, tmptx, hashBlock))
                       {

                           if (tmptx.vout.size() > iit->prevout.n)
                           {
                               inout = tmptx.vout[iit->prevout.n];
                           }
                           else
                           {
                               printf("GetAddressBalancesInInterval(): prevout %d also not in C transaction %s\n",
                                                      iit->prevout.n, iit->prevout.hash.ToString().c_str());
                               continue;
                           }
                       }
                   }
             }
             else
             {
                   CTransaction tmptx;
                   uint256 hashBlock = 0;
                   if (GetTransaction(iit->prevout.hash, tmptx, hashBlock))
                   {

                       if (tmptx.vout.size() > iit->prevout.n)
                       {
                           inout = tmptx.vout[iit->prevout.n];
                       }
                       else
                       {
                           printf("GetAddressBalancesInInterval(): prevout %d not in C transaction %s\n",
                                                  iit->prevout.n, iit->prevout.hash.ToString().c_str());
                           continue;
                       }

                   }
                   else
                   {
                      if (iit->prevout.hash != 0)
                      {
                          printf("GetAddressBalancesInInterval(): no info for tx %s\n",
                                                      iit->prevout.hash.ToString().c_str());
                      }
                      continue;
                   }
             }
             CTxDestination destaddr; 
             if (!ExtractDestination(inout.scriptPubKey, destaddr))
             { 
                  continue;
             }
             if (sAddress != CBitcoinAddress(destaddr).ToString())
             {
                  continue;
             }
             intx += inout.nValue;
         }
         if ((intx != 0) || (outtx != 0))
         {
               int64_t time = pcoin->nTime;
               balance = balance + outtx - intx;
               balancesRet.push_back(std::make_pair(time, balance));
               if ((pcoin->nTime >= start) && (pcoin->nTime <= end))
               {
                     if (fHitInterval)
                     {
                           if (balance < minBalanceRet)
                           {
                                  minBalanceRet = balance;
                           }
                           if (balance > maxBalanceRet)
                           {
                                  maxBalanceRet = balance;
                           }
                     }
                     else  // entered interval
                     {
                           minBalanceRet = balance;
                           maxBalanceRet = balance;
                           fHitInterval = true;
                     }
               }
         }
    }
    return true;

}


bool WalletModel::isLockedCoin(uint256 hash, unsigned int n) const
{
    return false;
}

void WalletModel::lockCoin(COutPoint& output)
{
    return;
}

void WalletModel::unlockCoin(COutPoint& output)
{
    return;
}

void WalletModel::listLockedCoins(std::vector<COutPoint>& vOutpts)
{
    return;
}
