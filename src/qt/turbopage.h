#ifndef TURBOPAGE_H
#define TURBOPAGE_H

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
class TurboPage;
}
class ClientModel;
class WalletModel;

class TurboPage : public QWidget
{
    Q_OBJECT

public:
    explicit TurboPage(QWidget *parent = 0);
    ~TurboPage();
    
    void setModel(ClientModel *model);
    void setModel(WalletModel *walletModel);
    
public slots:

    void updateTurbo();

private slots:

private:
    Ui::TurboPage *ui;
    ClientModel *model;
    WalletModel *walletModel;
    
};

#endif // TURBOPAGE_H
