#include "nrsocketproxy.h"

#include <QDebug>
#include <QNetworkDatagram>

NrSocketProxy::NrSocketProxy(const NrSocketProxyConfig &cfg, QObject *parent)
    : QObject{parent}
    , m_Config(cfg)
{
    m_pUdpSockToRemote = new QUdpSocket(this);
    m_pUdpSockFromLocal = new QUdpSocket(this);

    if (cfg.protocolType == NrSocketProxyConfig::UDP) {
        connect(m_pUdpSockFromLocal, &QUdpSocket::readyRead, this, &NrSocketProxy::onLocalReadyRead);
        connect(m_pUdpSockToRemote, &QUdpSocket::readyRead, this, &NrSocketProxy::onRemoteReadyRead);
        m_pUdpSockFromLocal->bind(QHostAddress(cfg.localAddress), cfg.localPort);
        m_pUdpSockToRemote->bind(QHostAddress(cfg.remoteAddress), cfg.remotePort);
    }

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
    while (m_pUdpSockFromLocal->hasPendingDatagrams()) {
        QNetworkDatagram msg = m_pUdpSockFromLocal->receiveDatagram();
        qDebug() << "received datagram" << msg.data() << " from " << msg.senderAddress() << ":" << msg.senderPort()
                 << " to" << msg.destinationAddress() << ":" << msg.destinationPort();
        if (!m_isRemoteSendingPaused) {
            msg.setData(msg.data()+"#");
            qDebug() << "About to send datagram" << msg.data() << " from " << msg.senderAddress() << ":" << msg.senderPort()
                     << " to" << m_Config.remoteAddress << ":" << m_Config.remotePort;
            qint64 wb = m_pUdpSockToRemote->writeDatagram(msg.data(), QHostAddress(m_Config.remoteAddress), m_Config.remotePort);
            qDebug() << "written bytes: " << wb << m_pUdpSockToRemote->errorString();

        } else {
            qDebug() << "Not sending following message because link is paused: " << msg.data();
        }
    }
}


void NrSocketProxy::onRemoteReadyRead()
{
    qDebug() << "Data arrived from server...";
    while (m_pUdpSockToRemote->hasPendingDatagrams()) {
        QNetworkDatagram msg = m_pUdpSockToRemote->receiveDatagram();
        qDebug() << "received datagram" << msg.data() << " from " << msg.senderAddress() << ":" << msg.senderPort()
                 << " to" << msg.destinationAddress() << ":" << msg.destinationPort();
        if (!m_isLocalSendingPaused) {
            msg.setData(msg.data()+"$");
            qDebug() << "About to send datagram" << msg.data() << " from " << msg.senderAddress() << ":" << msg.senderPort()
                     << " to" << m_Config.localAddress << ":" << m_Config.localPort;
            qint64 wb = m_pUdpSockToRemote->writeDatagram(msg.data(), m_pUdpSockFromLocal->peerAddress(), m_pUdpSockFromLocal->peerPort());
            qDebug() << "written bytes: " << wb << m_pUdpSockToRemote->errorString();
        } else {
            qDebug() << "";
        }
    }
}

void NrSocketProxy::onRemoteConnected()
{
    qDebug() << Q_FUNC_INFO;
}
void NrSocketProxy::onRemoteDisconnected()
{
    qDebug() << Q_FUNC_INFO;
}
void NrSocketProxy::onLocalConnected()
{
    qDebug() << Q_FUNC_INFO;
}
void NrSocketProxy::onLocalDisconnected()
{
    qDebug() << Q_FUNC_INFO;
}



QString NrSocketProxy::listeningAddress() const
{ return m_Config.localAddress; }

quint16 NrSocketProxy::listeningPort() const
{ return m_Config.localPort; }

QString NrSocketProxy::proxiedAddress() const
{ return m_Config.remoteAddress; }

quint16 NrSocketProxy::proxiedPort() const
{ return m_Config.remotePort; }
