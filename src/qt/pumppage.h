#ifndef PUMPPAGE_H
#define PUMPPAGE_H

#include "walletmodel.h"
#include "main.h"
#include "wallet.h"
#include "base58.h"
#include "bitcoingui.h"

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
    
    PumpInfo pumpInfo;

    void setModel(WalletModel *model);

    void updatePumpInfo(const QString &qsCurrent, const QString &qsNext);

 
public slots:

    void updateCurrentPump();
    void clear();
    void reject();
    void accept();

private slots:
    // void selectionChanged();
    void findAddress();
    void selectStealthAddress();
    void joinPump();

private:
    Ui::PumpPage *ui;
    WalletModel *model;
    std::string sPumpAddress, sPumpStealthAddress;
    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();
};

#endif // PUMPPAGE_H
