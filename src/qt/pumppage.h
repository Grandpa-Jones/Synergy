#ifndef PUMPPAGE_H
#define PUMPPAGE_H

#include "clientmodel.h"
#include "main.h"
#include "wallet.h"
#include "base58.h"

#include <QWidget>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTime>
#include <QTimer>
#include <QStringList>
#include <QMap>
#include <QSettings>
#include <QSlider>


namespace Ui {
    class PumpPage;
}

QT_BEGIN_NAMESPACE
class QTableView;
class QItemSelection;
class QSortFilterProxyModel;
QT_END_NAMESPACE

class PumpPage : public QWidget
{
    Q_OBJECT

public:
    explicit PumpPage(QWidget *parent = 0);
    ~PumpPage();
    
    // void setModel(TurboAddressTableModel *model);
    // void setModel(WalletModel *walletModel);
    
public slots:

    void updatePump();
    void updateChart();

private slots:
    // void selectionChanged();

private:
    Ui::PumpPage *ui;
    // WalletModel *walletModel;
    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();
};

#endif // PUMPPAGE_H
