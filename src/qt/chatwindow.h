#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QByteArray>
#include <QTimer>

namespace Ui {
    class ChatWindow;
}
class ClientModel;
class WalletModel;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Chat page widget */
class ChatWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWindow(QWidget *parent = 0);
    ~ChatWindow();

    void setModel(ClientModel *clientModel);
    void setModel(WalletModel *walletModel);

public slots:

//signals:

private:
    Ui::ChatWindow *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;

private slots:

};

#endif // CHATWINDOW_H