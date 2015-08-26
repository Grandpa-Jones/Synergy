#include "pumppage.h"
#include "ui_pumppage.h"
#include "main.h"
#include "wallet.h"
#include "init.h"
#include "base58.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "bitcoinrpc.h"
#include <sstream>
#include <string>
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "bitcoinunits.h"
#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "optionsmodel.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "pumppage.moc"
#include "addresscontrol.h"
#include "findaddressdialog.h"
#include "stealth.h"
#include "util.h"
#include "coincontroldialog.h"
#include "coincontrol.h"
#include "json_spirit.h"

#include <boost/lexical_cast.hpp>

#include <QMessageBox>


#define PUMP_LOOKBACK 2592000 // 30 days

#define PUMP_MIN_BALANCE_TIME 1209600  // 2 weeks

extern bool fRescanLock;

using namespace json_spirit;



PumpPage::PumpPage(QWidget *parent) :
    QWidget(parent),
    pumpInfo(),
    ui(new Ui::PumpPage)
{
    ui->setupUi(this);

    QPixmap banner_pix(":images/pump_banner");
    ui->label_banner->setPixmap(banner_pix);

    connect(ui->btnFindAddresses, SIGNAL(pressed()), this, SLOT(findAddress()));
    connect(ui->btnSetStealthAddress, SIGNAL(pressed()), this, SLOT(selectStealthAddress()));
    connect(ui->btnJoinPump, SIGNAL(pressed()), this, SLOT(joinPump()));

    ui->btcamtBalanceOngoing->setReadOnly(true);
    ui->btcamtBalanceNext->setReadOnly(true);
    ui->btcamtAmount->setReadOnly(true);
    ui->btcamtRequired->setReadOnly(true);

    subscribeToCoreSignals();
}

PumpPage::~PumpPage()
{
    unsubscribeFromCoreSignals();
    delete ui;
}

void PumpPage::setModel(WalletModel *model)
{
    // does anything need to be updated here?
    this->model = model;
}

// updates info taken from block chain
void PumpPage::updateCurrentPump()
{
    if (fRescanLock) {
          return;
    }
    if (IsInitialBlockDownload()) {
          if (fDebug)
          {
                printf("PumpPage::updateCurrentPump(): is initial block download\n");
          }
          return;
    }

    StructCOutTimeRevSorter coutTimeRevSorter;

    // find the most recent registration for current pump, not necessarily best
    // [TODO] move to model class
    std::vector<COutput> vCoins;
    this->model->spentCoinsToAddress(vCoins, this->pumpInfo.currAddr);
    std::sort(vCoins.begin(), vCoins.end(), coutTimeRevSorter);
    bool fFoundReg = false;
    std::string sRegister, sStealth, sPump;
    std::vector<COutput>::const_iterator it;
    for (it = vCoins.begin(); it != vCoins.end(); ++it)
    {
        if (fDebug) {
           printf("updateCurrentPump(): reg txid: %s\n", it->tx->GetHash().GetHex().c_str());
        }
        std::string strJson = it->tx->strTxComment;
        json_spirit::Value value;
        if (json_spirit::read(strJson, value))
        {
            if (value.type() != json_spirit::obj_type)
            {
               if (fDebug)
               {
                    printf("updateCurrentPump(): wrong type for pump registration\n");
               }
               continue;
            }
            json_spirit::Object obj = value.get_obj();
            if ((obj.size() < 2) || (obj[0].name_ != "stealth") ||
                                    (obj[1].name_ != "pump"))
            {
               if (fDebug)
               {
                    printf("updateCurrentPump(): malformed pump registration\n");
               }
               continue;
            }
            json_spirit::Value valStealth = obj[0].value_;
            json_spirit::Value valPump = obj[1].value_;
            if ((valStealth.type() != str_type) || (valPump.type() != str_type))
            {
               if (fDebug)
               {
                    printf("updateCurrentPump(): stealth address or pump id is wrong type\n");
               }
               continue;
            }
            sStealth = valStealth.get_str();
            if (!(sStealth.length() > 75 && IsStealthAddress(sStealth)))
            {
               if (fDebug)
               {
                    printf("updateCurrentPump(): stealth address is malformed\n");
               }
               continue;
            }
            char hexDate[17];
            sprintf(hexDate, "%"PRIx64, this->pumpInfo.currDate);
            sPump = valPump.get_str();
            if (sPump != std::string(hexDate))
            {
               if (fDebug)
               {
                    printf("updateCurrentPump(): mismatched pump id\n");
               }
               continue;
            }
            // assume all from same address as should be
            CWalletTx wtx = *it->tx;
            // seems like the only way is to init as null
            COutput inOut(NULL, 0, 0);
            if (!this->model->getFirstPrevoutForTx(wtx, inOut))
            {
                  printf("updateCurrentPump(): could not get prevout\n");
                  continue;
            }
            CTxDestination destaddr;
            if (ExtractDestination(inOut.tx->vout[inOut.i].scriptPubKey, destaddr))
            {
                  sRegister = CBitcoinAddress(destaddr).ToString();
                  fFoundReg = true;
                  break;
            }
            else
            {
                  if (fDebug)
                  {
                        printf("updateCurrentPump(): could not extract reg address\n");
                  }
                  continue;
            }
        }
        else
        {
            if (fDebug)
            {
                 printf("updateCurrentPump(): could not parse json\n");
            }
            continue;
        }
    }
    if (!fFoundReg)
    {
        if (fDebug)
        {
            printf("updateCurrentPump(): could not find registration transaction\n");
        }
        return;
    }
    this->ui->lineEditAddressOngoing->setText(QString(sRegister.c_str()));

    // find minimum balance for address 2 weeks prior to current pump
    std::vector<std::pair<uint256, int64_t> > balancesRet;
    int64_t minBalanceRet, maxBalanceRet;
    // find minimum balance for 2 weeks prior
    if (model->GetAddressBalancesInInterval(sRegister,
                     pumpInfo.currDate - PUMP_MIN_BALANCE_TIME, pumpInfo.currDate,
                     balancesRet, minBalanceRet, maxBalanceRet))
    {
           this->ui->btcamtBalanceOngoing->setValue(minBalanceRet);
    }
    else
    {
           printf("updateCurrentPump(): problem with pumpInfo.currDate\n");
    }

    // find the pick transaction coming from pumpInfo.currAddr
    std::vector<COutput> vOutFrom;
    if (!this->model->listCoinsFromAddress(this->pumpInfo.currAddr,
                                            this->pumpInfo.currDate - PUMP_LOOKBACK, vOutFrom))
    {
           return;
    }
    std::sort(vOutFrom.begin(), vOutFrom.end(), coutTimeRevSorter);
    CTransaction pickTx = *vOutFrom[0].tx;
    if (fDebug) {
        printf("updateCurrentPump(): pick txid: %s\n", pickTx.GetHash().ToString().c_str());
    }
    mapValue_t mapNarr;
    // [beta TODO] check to see if in wallet first!
    // if not, alert that wallet must be unlocked for pick.
    // hell, prompt for unlock right here
    if (model->findStealthTransactions(pickTx, mapNarr))
    {
            // [beta TODO] make sure the narrations get into wallet
            if (fDebug) {
                 printf("updateCurrentPump(): found %d narrations\n", mapNarr.size());
            }

            mapValue_t::const_iterator it;
            if (fDebug) {
                  for (it = mapNarr.begin(); it != mapNarr.end(); ++it)
                  {
                       printf("updateCurrentPump(): Narration: %s - %s\n", it->first.c_str(), it->second.c_str());
                  }
            }
            // should only be one narration in the pick transaction
            if (mapNarr.size() >= 1)
            {
                this->ui->lineEditPickOngoing->setText(QString(mapNarr.begin()->second.c_str()));
            }
    }
    else
    {
            printf("updateCurrentPump(): found no narrations for tx %s\n",
                                                 pickTx.GetHash().ToString().c_str());
    }
}

// updates info transmitted through alert system
void PumpPage::updatePumpInfo(const QString &qsCurrent, const QString &qsNext)
{
    this->ui->lineEditWeekOfOngoing->setText(qsCurrent);
    this->ui->lineEditWeekOfNext->setText(qsNext);
    this->ui->btcamtAmount->setValue(this->pumpInfo.fee);
    this->ui->btcamtRequired->setValue(this->pumpInfo.minBalance);
    this->updateCurrentPump();
}



void PumpPage::findAddress()
{
    if(!this->model)
        return;

    // [TODO] alert user that finding address meaningless during block download
    if (IsInitialBlockDownload()) {
          return;
    }

    FindAddressDialog dlg;
    dlg.setModel(this->model);
    dlg.exec();

    AddrPair addr;
    if (FindAddressDialog::addressControl->GetSelected(addr))
    {
        ui->lineEditAddressNext->setText(QString(addr.first.c_str()));
        this->sPumpAddress = addr.first;
        ui->btcamtBalanceNext->setValue(atoi64(addr.second));
    }

}


void PumpPage::selectStealthAddress()
{
    if(!model)
        return;
    AddressBookPage dlg(AddressBookPage::ForSending, AddressBookPage::ReceivingTab, this);
    dlg.setModel(model->getAddressTableModel());
    if(dlg.exec())
    {
        QString qsRV = dlg.getReturnValue();
        std::string sRV = qsRV.toStdString();
        if (IsStealthAddress(sRV))
        {
             ui->lineEditStealthAddress->setText(qsRV);
             this->sPumpStealthAddress = sRV;
        }
        else
        {
             QMessageBox::critical(this, QString("Set Stealth Address"),
                                           QString("Selected address is not a stealth address."),
                                           QMessageBox::Ok, QMessageBox::Ok);
        }
    }
}

void PumpPage::joinPump()
{
    PumpInfo info = this->pumpInfo;

    if (IsInitialBlockDownload()) {
           QMessageBox::critical(this, QString("Join Pump"),
                                         QString("Block chain not yet syncronized."),
                                         QMessageBox::Ok, QMessageBox::Ok);
           return;
    }
    if (!info.isSet)
    {
           QMessageBox::critical(this, QString("Join Pump"),
                                         QString("Please wait for pump info from P2P network."),
                                         QMessageBox::Ok, QMessageBox::Ok);
           return;
    }
    if (sPumpAddress == "")
    {
           QMessageBox::critical(this, QString("Join Pump"),
                                         QString("Please set the address you will register."),
                                         QMessageBox::Ok, QMessageBox::Ok);
           return;
    }
    if (sPumpStealthAddress == "")
    {
           QMessageBox::critical(this, QString("Join Pump"),
                                         QString("Please set the stealth address for communication."),
                                         QMessageBox::Ok, QMessageBox::Ok);
           return;
    }


    std::map<QString, int64_t> mapAddrs;
    this->model->listAddresses(mapAddrs);
    int64_t nAddrBalance = mapAddrs[this->sPumpAddress.c_str()];

    // might as well update since we have the value
    ui->btcamtBalanceNext->setValue(nAddrBalance);

    // [TODO] Need to dynamically calculate fee
    int64_t nTxFee = 0.15;  // should be less than 0.0142, but be safe

    int64_t nToSend = info.fee + nTxFee;

    if (nAddrBalance < (info.minBalance + nToSend))
    {
           QMessageBox::critical(this, QString("Join Pump"),
                                         QString("Address has insufficient balance to join pump."),
                                         QMessageBox::Ok, QMessageBox::Ok);
           return;
    }

    char comment[200];
    char hexDate[17];
    sprintf(hexDate, "%"PRIx64, info.nextDate);
    snprintf(comment, sizeof(comment), "{\"stealth\":\"%s\",\"pump\":\"%s\"}",
                                 sPumpStealthAddress.c_str(), hexDate);
    QString txcomment = QString(comment);

    QString qsJoinMessage = tr("Are you sure you want to send <b>%1</b> for the pump fee?").arg((
               BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, info.fee)));
 
    QMessageBox::StandardButton retval = QMessageBox::question(this,
                                             tr("Confirm Joining Pump"),
                                             qsJoinMessage,
                                             QMessageBox::Yes|QMessageBox::Cancel,
                                             QMessageBox::Cancel);

    if(retval != QMessageBox::Yes)
    {
        return;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if(!ctx.isValid())
    {
        // Unlock wallet was cancelled
        return;
    }

    unsigned int nProdTypeID = (unsigned int) SNRG_PUMP;

    QList<SendCoinsRecipient> recipients;

    SendCoinsRecipient rv;
    rv.address = info.nextAddr;
    rv.label = info.nextLabel;
    rv.narration = QString("");
    rv.typeInd = AddressTableModel::AT_Normal;
    rv.amount = (qint64) info.fee;

    recipients.append(rv);

    // [TODO] refactor to separate function
    // auto coin control: select lowest outputs until there are enough
    CCoinControl *cctrl = CoinControlDialog::coinControl;
    cctrl->SetNull();
    std::map<QString, std::vector<COutput> > mapCoins;
    model->groupAddresses(mapCoins);
    StructCOutValueSorter coutValueSorter;

    int64_t amt = 0;
    bool fNoInputsYet = true;
    std::map<QString, std::vector<COutput> >::iterator vit;
    for (vit = mapCoins.begin(); vit != mapCoins.end(); ++vit)
    {
          if (vit->first.toStdString() == this->sPumpAddress)
          {
               std::vector<COutput> vOutPuts = vit->second;
               std::sort(vOutPuts.begin(), vOutPuts.end(), coutValueSorter);
               std::vector<COutput>::iterator it;
               for (it = vOutPuts.begin(); it != vOutPuts.end(); ++it)
               { 
                    unsigned int n = it->i;
                    CTxOut txout = it->tx->vout[n];
                    if (fDebug)
                    {
                        printf("hash: %s, amout: %s\n", it->tx->GetHash().ToString().c_str(),
                            BitcoinUnits::formatWithUnit(BitcoinUnits::BTC,
                                                         txout.nValue).toStdString().c_str());
                    }

                    // change address is same as sending
                    if (fNoInputsYet)
                    {
                           CTxDestination destaddr;
                           if (ExtractDestination(txout.scriptPubKey, destaddr))
                           {
                                cctrl->destChange = destaddr;
                                fNoInputsYet = false;
                           }
                           else
                           {
                                // should never fail, just worked
                                continue;
                           }
                    }
                    amt += txout.nValue;
                    COutPoint outPoint = COutPoint(it->tx->GetHash(), n);
                    cctrl->Select(outPoint);
                    if (amt >= nToSend)
                    {
                        break;
                    }
               }
               break;
          }
    }
    // end of automated coin control

    if (fDebug)
    {
        int64_t nCCAmt = 0;
        std::vector<COutPoint> vCoinControl;
        std::vector<COutput>   vOutputs;
        cctrl->ListSelected(vCoinControl);
        model->getOutputs(vCoinControl, vOutputs);

        BOOST_FOREACH(const COutput& out, vOutputs)
        {
            nCCAmt += out.tx->vout[out.i].nValue;
        }
        printf("Coin control amount is %s\n",
                  BitcoinUnits::formatWithUnit(BitcoinUnits::BTC,
                                                 nCCAmt).toStdString().c_str());
        printf("Coin control selected is %d\n", cctrl->GetSize());
    }

    WalletModel::SendCoinsReturn sendstatus;
    sendstatus = model->sendCoins(txcomment, recipients, nProdTypeID, cctrl);

    switch(sendstatus.status)
    {
    case WalletModel::InvalidAddress:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("The recipient address is not valid, please recheck."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::InvalidAmount:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("The amount to pay must be larger than 0."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::AmountExceedsBalance:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("The amount exceeds your balance."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::AmountWithFeeExceedsBalance:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("The total exceeds your balance when the %1 transaction fee is included.").
            arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, sendstatus.fee)),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::DuplicateAddress:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("Duplicate address found, can only send to each address once per send operation."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::TransactionCreationFailed:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("Error: Transaction creation failed.  Please open Console and type                          'clearwallettransactions' followed by 'scanforalltxns' to repair."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::TransactionCommitFailed:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("Error: The transaction was rejected. This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not      marked as spent here."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::NarrationTooLong:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("Error: Narration is too long."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::Aborted: // User aborted, nothing to do
        break;
    case WalletModel::OK:
        accept();
        break;
    }

    // clear coin control
    cctrl->SetNull();
}

void PumpPage::clear()
{
    // left for future
}

void PumpPage::reject()
{
    clear();
}

void PumpPage::accept()
{
    clear();
}



static void NotifyBlocksChanged(PumpPage *pumpPage)
{
    if (!IsInitialBlockDownload() && !fRescanLock) {
           pumpPage->updateCurrentPump();
    }
}


void PumpPage::subscribeToCoreSignals()
{
    uiInterface.NotifyBlocksChanged.connect(boost::bind(NotifyBlocksChanged, this));
}

void PumpPage::unsubscribeFromCoreSignals()
{
    uiInterface.NotifyBlocksChanged.disconnect(boost::bind(NotifyBlocksChanged, this));
}
