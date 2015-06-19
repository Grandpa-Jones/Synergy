#ifndef TURBOPAGE_H
#define TURBOPAGE_H

#include "clientmodel.h"
#include "main.h"
#include "wallet.h"
#include "base58.h"

#include "turboaddresstablemodel.h"

#include "qcgaugewidget/qcgaugewidget.h"
#include "qcustomplot/qcustomplot.h"

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
    class TurboPage;
}
class TurboAddressTableModel;

QT_BEGIN_NAMESPACE
class QTableView;
class QItemSelection;
class QSortFilterProxyModel;
QT_END_NAMESPACE

class TurboPage : public QWidget
{
    Q_OBJECT

public:
    explicit TurboPage(QWidget *parent = 0);
    ~TurboPage();
    
    void setModel(TurboAddressTableModel *model);
    void setModel(WalletModel *walletModel);
    
public slots:

    void updateTurbo();
    void updateChart();
    void RowDoubleClicked(QModelIndex idx);

private slots:
    void selectionChanged();

private:
    Ui::TurboPage *ui;
    TurboAddressTableModel *model;
    WalletModel *walletModel;
    // QcGaugeWidget *mainGauge;
    QcNeedleItem *mainNeedle;
    QcLabelItem *labMultiplier;
    QcLabelItem *labAccount;
    // QCustomPlot *plotAddress;
    QSortFilterProxyModel *proxyModel;
    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();
};

#endif // TURBOPAGE_H
