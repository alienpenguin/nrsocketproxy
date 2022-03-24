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
    m_pSslServerSideSock = new QSslSocket(this);

    NrServerConfig sslcfg;
    sslcfg.portBindingPolicy = NrServerConfig::E_BindToSpecificPort;
    sslcfg.serverPort = cfg.localPort;

    if (cfg.protocolType == NrSocketProxyConfig::UDP) {
        connect(m_pUdpClientSideSock, &QUdpSocket::readyRead, this, &NrSocketProxy::onClientDataAvailable);
        connect(m_pUdpServerSideSock, &QUdpSocket::readyRead, this, &NrSocketProxy::onServerDataAvailable);
        m_pUdpClientSideSock->bind(QHostAddress(cfg.localAddress), cfg.localPort);
        m_pUdpServerSideSock->bind();
    } else if (cfg.protocolType == NrSocketProxyConfig::TCP) {
        connect(m_pTcpServerSideSock, &QTcpSocket::connected, this, &NrSocketProxy::onConnectedToServer);
        connect(m_pTcpServerSideSock, &QTcpSocket::disconnected, this, &NrSocketProxy::onDisconnectedFromServer);
        connect(m_pTcpServerSideSock, &QTcpSocket::readyRead, this, &NrSocketProxy::onServerDataAvailable);
        m_pTcpServerSideSock->connectToHost(QHostAddress(cfg.remoteAddress), cfg.remotePort);
        sslcfg.disableEncryption = true;
    } else {
        connect(m_pSslServerSideSock, &QTcpSocket::connected, this, &NrSocketProxy::onConnectedToServer);
        connect(m_pSslServerSideSock, &QTcpSocket::disconnected, this, &NrSocketProxy::onDisconnectedFromServer);
        connect(m_pSslServerSideSock, &QTcpSocket::readyRead, this, &NrSocketProxy::onServerDataAvailable);
        bool b = (bool) connect(m_pSslServerSideSock, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(onSslErrors(QList<QSslError>)));
        Q_ASSERT(b);
        m_pSslServerSideSock->ignoreSslErrors();
        m_pSslServerSideSock->connectToHostEncrypted(cfg.remoteAddress, cfg.remotePort);
        m_pSslServerSideSock->ignoreSslErrors();
        sslcfg.certfile = "test_cacert.pem";
        sslcfg.keyfile = "test_privkey.pem";
        sslcfg.ignoreSslErrors = true;
    }

    m_pSslServer = new SslServer(sslcfg);
    connect(m_pSslServer, &SslServer::connectedClient, this, &NrSocketProxy::onClientConnected);
    Q_ASSERT(m_pSslServer->listen());

    qDebug() << "Created a SocketProxy for protocol " << cfg.protocolType
             << " from " << cfg.localAddress << ":" << cfg.localPort
             << " to " << cfg.remoteAddress << ":" << cfg.remotePort;
}


NrSocketProxy::~NrSocketProxy()
{

}


void NrSocketProxy::onSslErrors(QList<QSslError> errorlist)
{
    qDebug() << Q_FUNC_INFO << errorlist;
    m_pSslServerSideSock->ignoreSslErrors(errorlist);
}

void NrSocketProxy::onClientDataAvailable()
{
    //qDebug() << "Data arrived from client...";
    if (m_Config.protocolType == NrSocketProxyConfig::UDP) {
        while (m_pUdpClientSideSock->hasPendingDatagrams()) {
            QNetworkDatagram msg = m_pUdpClientSideSock->receiveDatagram();
            m_lastUdpClientAddress = msg.senderAddress().toString();
            m_lastUdpClientPort = msg.senderPort();
            //qDebug() << "received datagram" << msg.data() << " from " << msg.senderAddress() << ":" << msg.senderPort() << " to" << msg.destinationAddress() << ":" << msg.destinationPort();
            if (!m_isSendingToServerPaused) {
                msg.setData(msg.data());
                //qDebug() << "About to send datagram" << msg.data() << " from " << msg.senderAddress() << ":" << msg.senderPort() << " to" << m_Config.remoteAddress << ":" << m_Config.remotePort;
                qint64 wb = m_pUdpServerSideSock->writeDatagram(msg.data(), QHostAddress(m_Config.remoteAddress), m_Config.remotePort);
                //qDebug() << "written bytes: " << wb << (wb>0?"":m_pUdpServerSideSock->errorString());
            } else {
                QString dmsg = "Not sending to server following message because link is paused: " + msg.data();
                //qDebug() << dmsg;
                emit sigLogEvent(dmsg);
            }
        }
    } else if (m_Config.protocolType == NrSocketProxyConfig::TCP) {
        QByteArray data = m_pTcpClientSideSock->readAll();
        if (!m_isSendingToServerPaused) {
            qint64 wb = m_pTcpServerSideSock->write(data);
            //qDebug() << "written bytes: " << wb << (wb>0?"":m_pTcpServerSideSock->errorString());
        } else {
            QString msg = "<span style=\"color: red\">dPROXY - Sending (TCP) to server is paused, dropping message: </span>" + data;
            emit sigLogEvent(msg);
        }
    } else {
        QByteArray data = m_pTcpClientSideSock->readAll();
        if (!m_isSendingToServerPaused) {
            qint64 wb = m_pSslServerSideSock->write(data);
            //qDebug() << "written bytes: " << wb << (wb>0?"":m_pTcpServerSideSock->errorString());
        } else {
            QString msg = "<span style=\"color: red\">dPROXY - Sending (SSL) to server is paused, dropping message: </span>" + data;
            emit sigLogEvent(msg);
        }
    }
}


void NrSocketProxy::onServerDataAvailable()
{
    //qDebug() << "Data arrived from server...";
    if (m_Config.protocolType == NrSocketProxyConfig::UDP) {
        while (m_pUdpServerSideSock->hasPendingDatagrams()) {
            QNetworkDatagram msg = m_pUdpServerSideSock->receiveDatagram();
            //qDebug() << "received datagram" << msg.data() << " from " << msg.senderAddress() << ":" << msg.senderPort() << " to" << msg.destinationAddress() << ":" << msg.destinationPort();
            if (!m_isSendingToClientPaused) {
                msg.setData(msg.data());
                //qDebug() << "About to send datagram" << msg.data() << " from " << m_Config.localAddress << ":" << m_Config.localPort << " to" << m_lastUdpClientAddress << ":" << m_lastUdpClientPort;
                qint64 wb = m_pUdpClientSideSock->writeDatagram(msg.data(), QHostAddress(m_lastUdpClientAddress), m_lastUdpClientPort);
                //qDebug() << "written bytes: " << wb << (wb>0?"":m_pUdpClientSideSock->errorString());
            } else {
                QString dmsg = "PROXY - Sending (UDP) to client is paused, dropping message: " + msg.data();
                emit sigLogEvent(dmsg);
                //qDebug() << dmsg;
            }
        }
    } else if (m_Config.protocolType == NrSocketProxyConfig::TCP) {
        QByteArray data = m_pTcpServerSideSock->readAll();
        if (!m_isSendingToClientPaused) {
            qint64 wb = m_pTcpClientSideSock->write(data);
            //qDebug() << "written bytes: " << wb << (wb>0?"":m_pTcpClientSideSock->errorString());
        } else {
            QString msg = "<span style=\"color: red\">dPROXY - Sending (TCP) to client is paused, dropping message: </span>" + data;
            emit sigLogEvent(msg);
        }
    } else {
        QByteArray data = m_pSslServerSideSock->readAll();
        if (!m_isSendingToClientPaused) {
            qint64 wb = m_pTcpClientSideSock->write(data);
            //qDebug() << "written bytes: " << wb << (wb>0?"":m_pTcpClientSideSock->errorString());
        } else {
            QString msg = "<span style=\"color: red\">dPROXY - Sending (SSL) to client is paused, dropping message: </span>" + data;
            emit sigLogEvent(msg);
        }
    }
}


void NrSocketProxy::onConnectedToServer()
{
    qDebug() << Q_FUNC_INFO;
    emit sigConnectedToServer();
}


void NrSocketProxy::onDisconnectedFromServer()
{
    qDebug() << Q_FUNC_INFO;
    emit sigDisconnectedFromServer();
}


void NrSocketProxy::onClientConnected()
{
    qDebug() << Q_FUNC_INFO;
    emit sigClientConnected();
    m_pTcpClientSideSock = m_pSslServer->nextPendingConnection();
    connect(m_pTcpClientSideSock, &QTcpSocket::readyRead, this, &NrSocketProxy::onClientDataAvailable);
    connect(m_pTcpClientSideSock, &QTcpSocket::disconnected, this, &NrSocketProxy::onClientDisconnected);
    //m_pTcpClientSideSock->write("test from proxy");
}

void NrSocketProxy::onClientDisconnected()
{
    qDebug() << Q_FUNC_INFO;
    emit sigClientDisconnected();
}

void NrSocketProxy::pauseSendingToClient(bool b)
{
    //qDebug() << "Setting sending from proxy to client to paused: " << b;
    m_isSendingToClientPaused = b;
}

void NrSocketProxy::pauseSendingToServer(bool b)
{
    //qDebug() << "Setting sending from proxy to server to paused: " << b;
    m_isSendingToServerPaused = b;
}

QString NrSocketProxy::listeningAddress() const
{ return m_Config.localAddress; }

quint16 NrSocketProxy::listeningPort() const
{ return m_Config.localPort; }

QString NrSocketProxy::proxiedAddress() const
{ return m_Config.remoteAddress; }

quint16 NrSocketProxy::proxiedPort() const
{ return m_Config.remotePort; }

/*!
 * \brief NrSocketProxy::udpPortToProxy
 * \return the udp port from which the proxy sends data to the proxied server
 */
quint16 NrSocketProxy::udpPortToProxy() const
{ return m_pUdpServerSideSock->localPort(); }
