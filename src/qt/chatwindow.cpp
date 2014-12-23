#include "chatwindow.h"
#include "ui_chatwindow.h"

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

#include "chatwindow.moc"

ChatWindow::ChatWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChatWindow),
    clientModel(0)
{
    ui->setupUi(this);        
}

ChatWindow::~ChatWindow()
{
    delete ui;
}

void ChatWindow::setModel(ClientModel *model)
{
    this->clientModel = model;
}