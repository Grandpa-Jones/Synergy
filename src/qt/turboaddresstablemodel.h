#ifndef TURBOADDRESSTABLEMODEL_H
#define TURBOADDRESSTABLEMODEL_H


#include <QAbstractTableModel>
#include <QStringList>

#include "bitcoinrpc.h"


class TurboAddressTableModel;
class TurboAddressTablePriv;
class CWallet;
class WalletModel;


struct TurboAddressTableEntry
{
    int rank;
    QString address;
    int turbo;

    TurboAddressTableEntry() {}
    TurboAddressTableEntry(int rank, const QString &address, int turbo):
        rank(rank), address(address), turbo(turbo) {}
};

struct TurboAddressTableEntryLessThan
{
    bool operator()(const TurboAddressTableEntry &a, const TurboAddressTableEntry &b) const
    {
        return a.rank < b.rank;
    }
    bool operator()(const TurboAddressTableEntry &a, int b) const
    {
        return a.rank < b;
    }
    bool operator()(int a, const TurboAddressTableEntry &b) const
    {
        return a < b.rank;
    }
};


/**
   Qt model of the address book in the core. This allows views to access and modify the address book.
 */
class TurboAddressTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit TurboAddressTableModel(CWallet *wallet, WalletModel *parent = 0);
    ~TurboAddressTableModel();

    enum ColumnIndex {
        Rank = 0,     /**< Turbo rank */
        Address = 1,  /**< Bitcoin address */
        Turbo = 2     /**< Turbo */
    };

    /** @name Methods overridden from QAbstractTableModel
        @{*/
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
    Qt::ItemFlags Flags(const QModelIndex &index) const;
    /*@}*/

    /* Add an address to the model.
       Returns the added address on success, and an empty string otherwise.
     */
    QString addRow(int rank, const QString &address, int turbo);

    /* Look up row index of an address in the model.
       Return -1 if not found.
     */
    int lookupAddress(const QString &address) const;

    void setTurbos(json_spirit::Object allTurbos);
    void update();

private:
    WalletModel *walletModel;
    CWallet *wallet;
    TurboAddressTablePriv *priv;
    QStringList columns;

signals:

public slots:

    friend class TurboAddressTablePriv;
};

#endif // TURBOADDRESSTABLEMODEL_H
