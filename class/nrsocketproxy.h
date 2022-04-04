#ifndef NRSOCKETPROXY_H
#define NRSOCKETPROXY_H

#include <QObject>
#include <QUdpSocket>
#include <QSslSocket>

class SslServer;

class NrSocketProxyConfig
{
public:
    QString localAddress = "127.0.0.1";
    QString remoteAddress = "127.0.0.1";
    quint16 localPort = 0;
    quint16 remotePort = 0;
    enum protocol {
        UDP,
        TCP,
        SSL } protocolType = UDP;
};

class NrSocketProxy : public QObject
{
    Q_OBJECT
    NrSocketProxyConfig m_Config;
    QUdpSocket *m_pUdpServerSideSock = nullptr; //udp socket to send data to proxied server
    QUdpSocket *m_pUdpClientSideSock = nullptr; //udp socket to send back data to client (binds on listen address)
    QTcpSocket *m_pTcpServerSideSock = nullptr; //tcp socket used to connect to proxied server
    QTcpSocket *m_pTcpClientSideSock = nullptr; //tcp socket retrieved from sslserver to send back data to client
    QSslSocket *m_pSslServerSideSock = nullptr; //ssl socket used to connect to proxied server
    QSslSocket *m_pSslClientSideSock = nullptr; //ssl socket retrieved from sslserver to send back data to client
    bool m_isSendingToClientPaused = false;
    bool m_isSendingToServerPaused = false;
    QString m_lastUdpClientAddress;
    quint16 m_lastUdpClientPort = 0;
    SslServer *m_pSslServer = nullptr; //Tcp or Ssl server to accept connections from clients

public:
    explicit NrSocketProxy(const NrSocketProxyConfig &cfg, QObject *parent = nullptr);
    NrSocketProxy(const NrSocketProxy &rhs) = delete;
    NrSocketProxy& operator=(const NrSocketProxy &rhs) = delete;
    virtual ~NrSocketProxy();
    void pauseSendingToServer(bool);
    void pauseSendingToClient(bool);
    QString listeningAddress() const;
    quint16 listeningPort() const;
    QString proxiedAddress() const;
    quint16 proxiedPort() const;
    quint16 udpPortToProxy() const;
    void disconnectClient();
    void disconnectServer();
    void connectServer();

signals:
    void sigClientConnected();
    void sigClientDisconnected();
    void sigConnectedToServer();
    void sigDisconnectedFromServer();
    void sigLogEvent(QString);

private slots:
    void onConnectedToServer();
    void onDisconnectedFromServer();
    void onClientConnected();
    void onClientDisconnected();
    void onClientDataAvailable();
    void onServerDataAvailable();
    void onSslErrors(QList<QSslError> errorlist);

};

#endif // NRSOCKETPROXY_H
