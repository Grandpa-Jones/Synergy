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
#include "optionsmodel.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "pumppage.moc"

#include <boost/lexical_cast.hpp>

using namespace json_spirit;

PumpPage::PumpPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PumpPage)
{
    ui->setupUi(this);

    subscribeToCoreSignals();
    
}

void PumpPage::updatePump()
{
    if (IsInitialBlockDownload()) {
          return;
    }
}

void PumpPage::updateChart()
{

    if (IsInitialBlockDownload()) {
          return;
    }

    updatePump();
}


PumpPage::~PumpPage()
{
    unsubscribeFromCoreSignals();
    delete ui;
}

void PumpPage::subscribeToCoreSignals()
{
    // empty for now
}

void PumpPage::unsubscribeFromCoreSignals()
{
    // empty for now
}

