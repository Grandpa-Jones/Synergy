#include "turboaddresstablemodel.h"
#include "guiutil.h"
#include "walletmodel.h"

#include "wallet.h"
#include "base58.h"

#include "bitcoinrpc.h"

#include "ui_interface.h"

#include <QFont>
#include <QColor>


// Private implementation
class TurboAddressTablePriv
{
public:
    CWallet *wallet;
    QList<TurboAddressTableEntry> cachedTurboAddressTable;
    TurboAddressTableModel *parent;
    json_spirit::Object allTurbos;

    TurboAddressTablePriv(CWallet *wallet, TurboAddressTableModel *parent):
        wallet(wallet), parent(parent) {}

    void refreshTurboAddressTable()
    {
        printf("TurboAddressTablePriv::refreshTurboAddressTable: called\n");
        cachedTurboAddressTable.clear();

        int rank = 0;
        for (json_spirit::Object::iterator it = allTurbos.begin(); it != allTurbos.end(); ++it)
        {
            ++rank;
            cachedTurboAddressTable.append(TurboAddressTableEntry(rank, QString::fromStdString(it->name_),
                                                                it->value_.get_int()));
            printf("TurboAddressTablePriv::refreshTurboAddressTable: rank: %d\n", rank);
        }

        // qLowerBound() and qUpperBound() require
        // our cachedTurboAddressTable list to be sorted in asc order
        qSort(cachedTurboAddressTable.begin(), cachedTurboAddressTable.end(),
                                              TurboAddressTableEntryLessThan());
    }

    int size()
    {
        return cachedTurboAddressTable.size();
    }

    TurboAddressTableEntry *index(int idx)
    {
        if(idx >= 0 && idx < cachedTurboAddressTable.size())
        {
            return &cachedTurboAddressTable[idx];
        }
        else
        {
            return 0;
        }
    }
};

TurboAddressTableModel::TurboAddressTableModel(CWallet *wallet, WalletModel *parent) :
    QAbstractTableModel(parent),walletModel(parent),wallet(wallet),priv(0)
{
    columns << tr("Rank") << tr("Address") << tr("Turbo");
    // json_spirit::Object allTurbos = getallturboaddresses(json_spirit::Array(), false).get_obj();
    // json_spirit::Object allTurbos = json_spirit::Object();
    priv = new TurboAddressTablePriv(wallet, this);
    // priv->refreshTurboAddressTable();
}

TurboAddressTableModel::~TurboAddressTableModel()
{
    delete priv;
}

int TurboAddressTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int TurboAddressTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant TurboAddressTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    TurboAddressTableEntry *rec = static_cast<TurboAddressTableEntry*>(index.internalPointer());

    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch(index.column())
        {
        case Rank:
            return rec->rank;
        case Address:
            return rec->address;
        case Turbo:
            return rec->turbo;
        }
    }
    else if (role == Qt::FontRole)
    {
        QFont font;
        if(index.column() == Address)
        {
            font = GUIUtil::bitcoinAddressFont();
        }
        return font;
    }
    return QVariant();
}

bool TurboAddressTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if(!index.isValid())
        return false;

    return true;
}

QVariant TurboAddressTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole)
        {
            return columns[section];
        }
    }
    return QVariant();
}

Qt::ItemFlags TurboAddressTableModel::Flags(const QModelIndex &index) const
{
    if(!index.isValid())
        return 0;

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    return retval;
}

QModelIndex TurboAddressTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    TurboAddressTableEntry *data = priv->index(row);
    if(data)
    {
        return createIndex(row, column, priv->index(row));
    }
    else
    {
        return QModelIndex();
    }
}

QString TurboAddressTableModel::addRow(int rank, const QString &address, int turbo)
{
    std::string strAddress = address.toStdString();
    return QString::fromStdString(strAddress);
}

bool TurboAddressTableModel::removeRows(int row, int count, const QModelIndex &parent)
{
    return false;
}

int TurboAddressTableModel::lookupAddress(const QString &address) const
{
    QModelIndexList lst = match(index(0, Address, QModelIndex()),
                                Qt::EditRole, address, 1, Qt::MatchExactly);
    if(lst.isEmpty())
    {
        return -1;
    }
    else
    {
        return lst.at(0).row();
    }
}

void TurboAddressTableModel::emitDataChanged()
{
    emit dataChanged(index(0, 0, QModelIndex()), index(0, columns.length()-1, QModelIndex()));
}

void TurboAddressTableModel::emitLayoutAboutToBeChanged()
{
    emit layoutAboutToBeChanged();
}

void TurboAddressTableModel::emitLayoutChanged()
{
    emit layoutChanged();
}


void TurboAddressTableModel::setTurbos(json_spirit::Object allTurbos)
{
    printf("TurboAddressTableModel::setTurbos: called\n");
    emit beginResetModel();
    priv->allTurbos = allTurbos;
    priv->refreshTurboAddressTable();
    emit endResetModel();
    // emit DataChanged();
}


void TurboAddressTableModel::update()
{
    if (fDebug) {
          printf("TurboAddressTableModel::update\n");
    }

    if (!IsInitialBlockDownload()) {
          // [TODO] factor getallturboaddresses
          printf("TurboAddressTableModel::update: called\n");
          json_spirit::Object allTurbos = getallturboaddresses(json_spirit::Array(), false).get_obj();
          this->setTurbos(allTurbos);
    }
}

