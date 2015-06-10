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

using namespace json_spirit;

TurboPage::TurboPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TurboPage)
{
    ui->setupUi(this);
    
    setFixedSize(400, 420);
    
    connect(ui->startButton, SIGNAL(pressed()), this, SLOT(updateTurbo()));
}

void TurboPage::updateTurbo()
{

}

void TurboPage::setModel(ClientModel *model)
{
    updateTurbo();
    this->model = model;
}


TurboPage::~TurboPage()
{
    delete ui;
}
