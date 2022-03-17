#include "nrsocketproxy.h"

#include <QDebug>
#include <QNetworkDatagram>

#include "sslserver.h"

NrSocketProxy::NrSocketProxy(const NrSocketProxyConfig &cfg, QObject *parent)
    : QObject{parent}
    , m_Config(cfg)
{
    m_pUdpServerSideSock = new QUdpSocket(this);
    m_pUdpClientSideSock = new QUdpSocket(this);
    m_pTcpServerSideSock = new QTcpSocket(this);

    NrServerConfig sslcfg;
    sslcfg.portBindingPolicy = NrServerConfig::E_BindToSpecificPort;
    sslcfg.serverPort = cfg.localPort;

    if (cfg.protocolType == NrSocketProxyConfig::UDP) {
        connect(m_pUdpClientSideSock, &QUdpSocket::readyRead, this, &NrSocketProxy::onLocalReadyRead);
        connect(m_pUdpServerSideSock, &QUdpSocket::readyRead, this, &NrSocketProxy::onRemoteReadyRead);
        m_pUdpClientSideSock->bind(QHostAddress(cfg.localAddress), cfg.localPort);
        m_pUdpServerSideSock->bind();
    } else if (cfg.protocolType == NrSocketProxyConfig::TCP) {
        connect(m_pTcpServerSideSock, &QTcpSocket::connected, this, &NrSocketProxy::onRemoteConnected);
        connect(m_pTcpServerSideSock, &QTcpSocket::disconnected, this, &NrSocketProxy::onRemoteDisconnected);
        connect(m_pTcpServerSideSock, &QTcpSocket::readyRead, this, &NrSocketProxy::onRemoteReadyRead);
        m_pTcpServerSideSock->connectToHost(QHostAddress(cfg.remoteAddress), cfg.remotePort);
        sslcfg.disableEncryption = true;
    } else {

    }

    m_pSslServer = new SslServer(sslcfg);
    connect(m_pSslServer, &SslServer::connectedClient, this, &NrSocketProxy::onLocalConnected);
    Q_ASSERT(m_pSslServer->listen());

    qDebug() << "Created a SocketProxy for protocol " << cfg.protocolType
             << " from " << cfg.localAddress << ":" << cfg.localPort
             << " to " << cfg.remoteAddress << ":" << cfg.remotePort;
}


NrSocketProxy::~NrSocketProxy()
{

}


void NrSocketProxy::onLocalReadyRead()
{
    qDebug() << "Data arrived from client...";
    if (m_Config.protocolType == NrSocketProxyConfig::UDP) {
        while (m_pUdpClientSideSock->hasPendingDatagrams()) {
            QNetworkDatagram msg = m_pUdpClientSideSock->receiveDatagram();
            m_lastUdpClientAddress = msg.senderAddress().toString();
            m_lastUdpClientPort = msg.senderPort();
            qDebug() << "received datagram" << msg.data() << " from " << msg.senderAddress() << ":" << msg.senderPort()
                     << " to" << msg.destinationAddress() << ":" << msg.destinationPort();
            if (!m_isRemoteSendingPaused) {
                msg.setData(msg.data()+"#");
                qDebug() << "About to send datagram" << msg.data() << " from " << msg.senderAddress() << ":" << msg.senderPort()
                         << " to" << m_Config.remoteAddress << ":" << m_Config.remotePort;
                qint64 wb = m_pUdpServerSideSock->writeDatagram(msg.data(), QHostAddress(m_Config.remoteAddress), m_Config.remotePort);
                qDebug() << "written bytes: " << wb << (wb>0?"":m_pUdpServerSideSock->errorString());
            } else {
                QString dmsg = "Not sending to server following message because link is paused: " + msg.data();
                qDebug() << dmsg;
                emit sigLogEvent(dmsg);
            }
        }
    } else {
        QByteArray data = m_pTcpClientSideSock->readAll();
        if (!m_isRemoteSendingPaused) {
            qint64 wb = m_pTcpServerSideSock->write(data);
            qDebug() << "written bytes: " << wb << (wb>0?"":m_pTcpServerSideSock->errorString());
        } else {
            QString msg = "<span style=\"color: red\">dPROXY - Sending (TCP) to server is paused, dropping message: </span>" + data;
            emit sigLogEvent(msg);
        }
    }
}


void NrSocketProxy::onRemoteReadyRead()
{
    qDebug() << "Data arrived from server...";
    if (m_Config.protocolType == NrSocketProxyConfig::UDP) {
        while (m_pUdpServerSideSock->hasPendingDatagrams()) {
            QNetworkDatagram msg = m_pUdpServerSideSock->receiveDatagram();
            qDebug() << "received datagram" << msg.data() << " from " << msg.senderAddress() << ":" << msg.senderPort()
                     << " to" << msg.destinationAddress() << ":" << msg.destinationPort();
            if (!m_isLocalSendingPaused) {
                msg.setData(msg.data()+"$");
                qDebug() << "About to send datagram" << msg.data() << " from " << m_Config.localAddress << ":" << m_Config.localPort
                         << " to" << m_lastUdpClientAddress << ":" << m_lastUdpClientPort;
                qint64 wb = m_pUdpClientSideSock->writeDatagram(msg.data(), QHostAddress(m_lastUdpClientAddress), m_lastUdpClientPort);
                qDebug() << "written bytes: " << wb << (wb>0?"":m_pUdpClientSideSock->errorString());
            } else {
                QString dmsg = "PROXY - Sending (UDP) to client is paused, dropping message: " + msg.data();
                emit sigLogEvent(dmsg);
                qDebug() << dmsg;
            }
        }
    } else {
        QByteArray data = m_pTcpServerSideSock->readAll();
        if (!m_isLocalSendingPaused) {
            qint64 wb = m_pTcpClientSideSock->write(data);
            qDebug() << "written bytes: " << wb << (wb>0?"":m_pTcpClientSideSock->errorString());
        } else {
            QString msg = "<span style=\"color: red\">dPROXY - Sending (TCP) to client is paused, dropping message: </span>" + data;
            emit sigLogEvent(msg);
        }
    }
}


void NrSocketProxy::onRemoteConnected()
{
    qDebug() << Q_FUNC_INFO;
    emit sigConnectedToServer();
    qint64 wb = m_pTcpServerSideSock->write("Ciao");
    qDebug() << "Written bytes: " << wb << (wb>0?"":m_pTcpServerSideSock->errorString());
}


void NrSocketProxy::onRemoteDisconnected()
{
    qDebug() << Q_FUNC_INFO;
    emit sigDisconnectedFromServer();
}


void NrSocketProxy::onLocalConnected()
{
    qDebug() << Q_FUNC_INFO;
    emit sigClientConnected();
    m_pTcpClientSideSock = m_pSslServer->nextPendingConnection();
    connect(m_pTcpClientSideSock, &QTcpSocket::readyRead, this, &NrSocketProxy::onLocalReadyRead);
    connect(m_pTcpClientSideSock, &QTcpSocket::disconnected, this, &NrSocketProxy::onLocalDisconnected);
}

void NrSocketProxy::onLocalDisconnected()
{
    qDebug() << Q_FUNC_INFO;
    emit sigClientDisconnected();
}

void NrSocketProxy::pauseLocalSending(bool b)
{
    qDebug() << "Setting sending from proxy to client to paused: " << b;
    m_isLocalSendingPaused = b;
}

void NrSocketProxy::pauseRemoteSending(bool b)
{
    qDebug() << "Setting sending from proxy to server to paused: " << b;
    m_isRemoteSendingPaused = b;
}

QString NrSocketProxy::listeningAddress() const
{ return m_Config.localAddress; }

quint16 NrSocketProxy::listeningPort() const
{ return m_Config.localPort; }

QString NrSocketProxy::proxiedAddress() const
{ return m_Config.remoteAddress; }

quint16 NrSocketProxy::proxiedPort() const
{ return m_Config.remotePort; }

quint16 NrSocketProxy::udpPortToProxy() const
{ return m_pUdpServerSideSock->localPort(); }
