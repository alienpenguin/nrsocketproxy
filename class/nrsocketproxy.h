#ifndef NRSOCKETPROXY_H
#define NRSOCKETPROXY_H

#include <QObject>
#include <QUdpSocket>

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
    QUdpSocket *m_pUdpSockToRemote = nullptr;
    QUdpSocket *m_pUdpSockFromLocal = nullptr;
    bool m_isLocalSendingPaused = false;
    bool m_isRemoteSendingPaused = false;
public:
    explicit NrSocketProxy(const NrSocketProxyConfig &cfg, QObject *parent = nullptr);
    NrSocketProxy(const NrSocketProxy &rhs) = delete;
    NrSocketProxy& operator=(const NrSocketProxy &rhs) = delete;
    virtual ~NrSocketProxy();
    void pauseRemoteSending(bool);
    void pauseLocalSending(bool);
    QString listeningAddress() const;
    quint16 listeningPort() const;
    QString proxiedAddress() const;
    quint16 proxiedPort() const;

signals:
    void sigRemoteConnected();
    void sigRemoteDisconnected();
private slots:
    void onRemoteConnected();
    void onRemoteDisconnected();
    void onLocalConnected();
    void onLocalDisconnected();
    void onLocalReadyRead();
    void onRemoteReadyRead();

};

#endif // NRSOCKETPROXY_H
