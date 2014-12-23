#include "backuppage.h"
#include "ui_backuppage.h"

#include "clientmodel.h"
#include "walletmodel.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "guiutil.h"
#include "guiconstants.h"

#include <QAbstractItemDelegate>
#include <QPainter>

#include "backuppage.moc"

BackupPage::BackupPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BackupPage),
    clientModel(0)
{
    ui->setupUi(this);        
}

BackupPage::~BackupPage()
{
    delete ui;
}

void BackupPage::setModel(ClientModel *model)
{
    this->clientModel = model;
}