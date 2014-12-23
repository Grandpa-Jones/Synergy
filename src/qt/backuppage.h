#ifndef BACKUPPAGE_H
#define BACKUPPAGE_H


#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QByteArray>
#include <QTimer>

namespace Ui {
    class BackupPage;
}
class ClientModel;
class WalletModel;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Trade page widget */
class BackupPage : public QWidget
{
    Q_OBJECT

public:
    explicit BackupPage(QWidget *parent = 0);
    ~BackupPage();

    void setModel(ClientModel *clientModel);
    void setModel(WalletModel *walletModel);

public slots:

// signals:

private:
    Ui::BackupPage *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;

private slots:

};

#endif // BACKUPPAGE_H
