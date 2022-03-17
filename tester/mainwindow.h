#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class NrSocketProxy;
class QUdpSocket;
class QTcpSocket;
class QSslSocket;
class SslServer;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
    QUdpSocket *m_pUdpClientSocket = nullptr; //to send datagrams to proxy
    QUdpSocket *m_pUdpServerSocket = nullptr; //to get datagrams from proxy and send back to it
    QTcpSocket *m_pTcpClientSocket = nullptr; //to connect to proxy on tcp
    QTcpSocket *m_pTcpServerSocket = nullptr; //got from sslserver to send back data to proxy
    QSslSocket *m_pSslClientSocket = nullptr; //to connect to proxy on ssl
    QSslSocket *m_pSslServerSocket = nullptr;
    NrSocketProxy *m_pProxy = nullptr;
    SslServer *m_pSslServer = nullptr; //to allow proxy connection
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QString timestampMessage(const QString msg) const;
private slots:
    void onConfigButtonPressed();
    void onSendToLocal();
    void onSendToRemote();
    void onPauseToLocalChecked(bool);
    void onPauseToRemoteChecked(bool);
    void onClientReadyRead();
    void onServerReadyRead();
    void onProxyConnectedToServerSide(); //connected to SslServer signal
    void onClientConnectedToProxy(); //connected to m_pTcpClientSocket signal
    void onClientDisconnectedFromProxy();
    void logMessage(QString);
};
#endif // MAINWINDOW_H
