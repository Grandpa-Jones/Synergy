#include "turbopage.h"
#include "ui_turbopage.h"
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
#include "optionsmodel.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "turbopage.moc"

#include <boost/lexical_cast.hpp>

using namespace json_spirit;

TurboPage::TurboPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TurboPage)
{
    ui->setupUi(this);

    connect(ui->startButton, SIGNAL(pressed()), this, SLOT(updateChart()));

    ui->mainGauge->addArc(54);
    ui->mainGauge->addDegrees(66)->setValueRange(0, MAX_TURBO_MULTIPLIER);
    QcColorBand *clrBand = ui->mainGauge->addColorBand(48);
    clrBand->setValueRange(0, 100);


    QcValuesItem *values = ui->mainGauge->addValues(78);
    values->setStep(40);
    values->setValueRange(0, MAX_TURBO_MULTIPLIER);
    ui->mainGauge->addLabel(60)->setText("Turbo");
    labMultiplier = ui->mainGauge->addLabel(40);
    labAccount = ui->mainGauge->addLabel(74);
    mainNeedle = ui->mainGauge->addNeedle(60);
    mainNeedle->setLabel(labMultiplier);
    mainNeedle->setColor(Qt::blue);
    mainNeedle->setValueRange(1, MAX_TURBO_MULTIPLIER);
    mainNeedle->setCurrentValue(0);
    ui->mainGauge->addBackground(7);
    labAccount->setText("");

    // generate some data:
    QVector<double> x(101), y(101); // initialize with entries 0..100
    for (int i=0; i<101; ++i)
    {
      x[i] = i/400.0 - 1; // x goes from -1 to 1
      y[i] = x[i]*x[i]; // let's plot a quadratic function
    }
    // create graph and assign data to it:
    ui->plotAddress->addGraph();
    // give the axes some labels:
    ui->plotAddress->xAxis->setLabel("Block Number");
    ui->plotAddress->yAxis->setLabel("Turbo Multiplier");
    // set axes ranges, so we see all data:
    ui->plotAddress->xAxis->setRange(0, 1000);
    ui->plotAddress->yAxis->setRange(0, 288);
    ui->vertLayoutA2->addWidget(ui->plotAddress);

    connect(ui->turboTableView, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(RowDoubleClicked(const QModelIndex&)));

    subscribeToCoreSignals();
    
}

void TurboPage::RowDoubleClicked(QModelIndex idx)
{
    // TurboAddressTableEntry *rec = static_cast<TurboAddressTableEntry*>(idx.internalPointer());

    int row = idx.row();
    QString address = idx.sibling(row, 1).data().toString();

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(address);

    QMessageBox::information(this, "Turbo Stake", address + QString(" copied to clipboard."));
}

void TurboPage::updateTurbo()
{

    json_spirit::Object myTurbos = getmyturboaddresses(json_spirit::Array(), false).get_obj();
    if (!myTurbos.empty()) {
           json_spirit::Pair pair = myTurbos[0];
           QString name = QString::fromStdString(pair.name_);
           int turbo = pair.value_.get_int();
           labAccount->setText(name);
           mainNeedle->setCurrentValue(turbo);
    }

    /*
    QVector<double> x(101), y(101); // initialize with entries 0..100
    for (int i=0; i<101; ++i)
    {
      x[i] = i/50.0 - 1; // x goes from -1 to 1
      y[i] = x[i]*x[i]; // let's plot a quadratic function
    }

    ui->plotAddress->graph(0)->setData(x, y);
    ui->plotAddress->replot();
    */

}

void TurboPage::updateChart()
{

    updateTurbo();

    bool chart_it = false;

    std::string sAddress;

    QString qsAddress = ui->editAddress->text();
    if (qsAddress.trimmed().isEmpty())
    {
         json_spirit::Object myTurbos = getmyturboaddresses(json_spirit::Array(), false).get_obj();
         if (!myTurbos.empty())
         {
                json_spirit::Pair pair = myTurbos[0];
                sAddress = pair.name_;
                qsAddress = QString::fromStdString(sAddress);
                ui->editAddress->setText(qsAddress);
                chart_it = true;
         }
    }
    else
    {
         sAddress = qsAddress.toStdString();
         chart_it = true;
    }
  
    if (chart_it)
    {
       CBitcoinAddress address(sAddress);
       if (address.IsValid())
       {
             json_spirit::Array ary = json_spirit::Array();
             ary.push_back(sAddress);
             json_spirit::Array xy = getturbo(ary, false).get_array();
             if (xy.empty())
             {
                   QMessageBox::warning(this, "Turbo Stake", "Address has no Turbo Stake.");
                   return;
             }
             json_spirit::Array X = xy[0].get_array();
             json_spirit::Array Y = xy[1].get_array();
             QVector<double> qX(X.size()), qY(Y.size());
             int i = 0;
             for (json_spirit::Array::iterator it = X.begin(); it != X.end(); ++it)
             {
                    qX[i] = it->get_real();
                    ++i;
             }
             i = 0;
             for (json_spirit::Array::iterator it = Y.begin(); it != Y.end(); ++it)
             {
                    qY[i] = it->get_real();
                    ++i;
             }
             ui->plotAddress->graph(0)->setData(qX, qY);
             ui->plotAddress->graph(0)->rescaleAxes();
             ui->plotAddress->replot();
       }
       else
       {
             QMessageBox::warning(this, "Turbo Stake", "Address is not valid.");
             return;
       }
    }
}

void TurboPage::setModel(TurboAddressTableModel *model)
{
    this->model = model;

    if (!model) {
        return;
    }

    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    ui->turboTableView->setModel(proxyModel);
    ui->turboTableView->sortByColumn(0, Qt::AscendingOrder);

        // Set column widths
    ui->turboTableView->horizontalHeader()->resizeSection(
            TurboAddressTableModel::Address, 320);
#if QT_VERSION <0x050000
    ui->turboTableView->horizontalHeader()->setResizeMode(
            TurboAddressTableModel::Rank, QHeaderView::Stretch);
#else
    ui->turboTableView->horizontalHeader()->setSectionResizeMode(TurboAddressTableModel::Rank, QHeaderView::Stretch);
#endif

    connect(ui->turboTableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(selectionChanged()));

    selectionChanged();
}


TurboPage::~TurboPage()
{
    unsubscribeFromCoreSignals();
    delete ui;
}

void TurboPage::selectionChanged()
{
    // Set button states based on selected tab and selection
    QTableView *table = ui->turboTableView;
    if(!table->selectionModel())
        return;

    if(table->selectionModel()->hasSelection())
    {
          // ui->copyToClipboard->setEnabled(true);
    }
    else
    {
          // ui->copyToClipboard->setEnabled(false);
    }
}

static void NotifyBlocksChanged(TurboPage *turboPage)
{
    if (!IsInitialBlockDownload()) {
           turboPage->updateTurbo();
    }
}

void TurboPage::subscribeToCoreSignals()
{
    uiInterface.NotifyBlocksChanged.connect(boost::bind(NotifyBlocksChanged, this));
}

void TurboPage::unsubscribeFromCoreSignals()
{
    uiInterface.NotifyBlocksChanged.disconnect(boost::bind(NotifyBlocksChanged, this));
}

