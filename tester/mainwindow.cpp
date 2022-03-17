#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "nrsocketproxy.h"
#include "sslserver.h"

#include <QNetworkDatagram>
#include <QSslSocket>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->btnConfig, &QPushButton::clicked, this, &MainWindow::onConfigButtonPressed);
    connect(ui->btnSendToLocal, &QPushButton::clicked, this, &MainWindow::onSendToLocal);
    connect(ui->btnSendToRemote, &QPushButton::clicked, this, &MainWindow::onSendToRemote);
    connect(ui->chkPauseSendToRemote, &QCheckBox::clicked, this, &MainWindow::onPauseToRemoteChecked);
    connect(ui->chkPauseSendToLocal, &QCheckBox::clicked, this, &MainWindow::onPauseToLocalChecked);

    m_pUdpClientSocket = new QUdpSocket(this);
    m_pUdpServerSocket = new QUdpSocket(this);
    m_pTcpClientSocket = new QTcpSocket(this);
    m_pSslClientSocket = new QSslSocket(this);

    connect (m_pUdpClientSocket, &QUdpSocket::readyRead, this, &MainWindow::onClientReadyRead);
    connect (m_pUdpServerSocket, &QUdpSocket::readyRead, this, &MainWindow::onServerReadyRead);

    connect(m_pTcpClientSocket, &QTcpSocket::connected, this, &MainWindow::onClientConnectedToProxy);
    connect(m_pTcpClientSocket, &QTcpSocket::disconnected, this, &MainWindow::onClientDisconnectedFromProxy);
    connect(m_pTcpClientSocket, &QTcpSocket::readyRead, this, &MainWindow::onClientReadyRead);

    connect(m_pSslClientSocket, &QTcpSocket::connected, this, &MainWindow::onClientConnectedToProxy);
    connect(m_pSslClientSocket, &QTcpSocket::disconnected, this, &MainWindow::onClientDisconnectedFromProxy);
    connect(m_pSslClientSocket, &QTcpSocket::readyRead, this, &MainWindow::onClientReadyRead);
}

MainWindow::~MainWindow()
{
    m_pTcpClientSocket->disconnectFromHost();
    m_pTcpServerSocket->disconnectFromHost();
    m_pProxy->deleteLater();
    delete ui;
}


QString MainWindow::timestampMessage(const QString msg) const
{
    QString s = msg;
    QDateTime dt = QDateTime::currentDateTime();
    s.prepend(dt.toString("hh:mm:ss.zzz "));
    return s;
}


void
MainWindow::logMessage(QString s)
{
    if (ui && ui->txtLog)
        ui->txtLog->append(timestampMessage(s));
}

void MainWindow::onConfigButtonPressed()
{
    qDebug() << Q_FUNC_INFO;

    //Config for tcp / ssl server
    NrServerConfig sslcfg;

    NrSocketProxyConfig cfg;
    cfg.localAddress = ui->txtListenAddress->text();
    cfg.remoteAddress = ui->txtSendAddress->text();
    cfg.localPort = ui->txtListenPort->text().toUInt();
    cfg.remotePort = ui->txtSendPort->text().toUInt();
    if (ui->rdoUdp->isChecked()) {
        cfg.protocolType = NrSocketProxyConfig::UDP;
    } else if (ui->rdoTcp->isChecked()) {
        cfg.protocolType = NrSocketProxyConfig::TCP;
        qDebug() << "disabling local server encryption";
        sslcfg.disableEncryption = true;
    } else {
        cfg.protocolType = NrSocketProxyConfig::SSL;
        sslcfg.certfile = "test_cacert.pem";
        sslcfg.keyfile = "test_privkey.pem";
        sslcfg.ignoreSslErrors = true;
    }

    //Setup TCP / SSL Server
    sslcfg.portBindingPolicy = NrServerConfig::E_BindToSpecificPort;
    sslcfg.serverPort = cfg.remotePort;
    m_pSslServer = new SslServer(sslcfg);
    connect(m_pSslServer, &SslServer::connectedClient, this, &MainWindow::onProxyConnectedToServerSide);
    Q_ASSERT(m_pSslServer->listen());

    //UDP
    qDebug() << "Binding receiving socket to " << cfg.remoteAddress << cfg.remotePort;
    Q_ASSERT(m_pUdpServerSocket->bind(QHostAddress(cfg.remoteAddress), cfg.remotePort));

    //Setup Proxy
    m_pProxy = new NrSocketProxy(cfg);
    connect(m_pProxy, &NrSocketProxy::sigLogEvent, this, &MainWindow::logMessage);

    //GUI Sanity state
    ui->btnSendToLocal->setEnabled(true);
    ui->btnSendToRemote->setEnabled(true);
    ui->gboxProtocol->setEnabled(false);
    ui->gboxSendTo->setEnabled(false);
    ui->gboxListen->setEnabled(false);

    if (cfg.protocolType == NrSocketProxyConfig::TCP) {
        //connect to proxy on tcp
        m_pTcpClientSocket->connectToHost(cfg.localAddress, cfg.localPort);
    }

    if (cfg.protocolType == NrSocketProxyConfig::SSL) {
        //connect to proxy on ssl
        //m_pSslClientSocket->connectToHostEncrypted(cfg.localAddress, cfg.localPort);
    }
}


void MainWindow::onProxyConnectedToServerSide()
{
    qDebug() << Q_FUNC_INFO;
    logMessage("Proxy connected to our server on TCP / SSL");
    m_pTcpServerSocket = m_pSslServer->nextPendingConnection();
    connect(m_pTcpServerSocket, &QTcpSocket::readyRead, this, &MainWindow::onServerReadyRead);
}


void MainWindow::onClientConnectedToProxy()
{
    qDebug() << Q_FUNC_INFO;
    logMessage("Client connected from proxy on TCP / SSL");
}


void MainWindow::onClientDisconnectedFromProxy()
{
    qDebug() << Q_FUNC_INFO;
    logMessage("Client disconnected from proxy on TCP / SSL");
}

void MainWindow::onSendToLocal()
{
    qDebug() << Q_FUNC_INFO;
    QString msg = ui->txtToLocal->text();
    qDebug() << "Sending " << msg << " to " << m_pProxy->proxiedAddress() << m_pProxy->proxiedPort();
    if (ui->rdoUdp->isChecked()) {
        qDebug() << "sending to client over UDP";
        m_pUdpServerSocket->writeDatagram(msg.toUtf8(), QHostAddress(m_pProxy->proxiedAddress()), m_pProxy->udpPortToProxy());
    } else {
        qDebug() << "sending to client over tcp";
        m_pTcpServerSocket->write(msg.toUtf8());
    }
    ui->txtServer->append(timestampMessage(" <- " + msg));
}


void MainWindow::onSendToRemote()
{
    qDebug() << Q_FUNC_INFO;
    QString msg = ui->txtToRemote->text();
    qDebug() << "Sending " << msg << " to " << m_pProxy->listeningAddress() << m_pProxy->listeningPort();
    if (ui->rdoUdp->isChecked()) {
        qDebug() << "sending to server over UDP";
        m_pUdpClientSocket->writeDatagram(msg.toUtf8(), QHostAddress(m_pProxy->listeningAddress()), m_pProxy->listeningPort());
    } else {
        qDebug() << "sending to server over tcp";
        m_pTcpClientSocket->write(msg.toUtf8());
    }
    ui->txtClient->append(timestampMessage(" <- " + msg));
}


void MainWindow::onPauseToLocalChecked(bool paused)
{
    qDebug() << Q_FUNC_INFO << paused;
    m_pProxy->pauseSendingToClient(paused);
}


void MainWindow::onPauseToRemoteChecked(bool paused)
{
    qDebug() << Q_FUNC_INFO << paused;
    m_pProxy->pauseSendingToServer(paused);
}


void MainWindow::onClientReadyRead()
{
    qDebug() << Q_FUNC_INFO;
    if (ui->rdoUdp->isChecked()) {
        qDebug() << "Using UDP";
        while (m_pUdpClientSocket->hasPendingDatagrams())
        {
            QNetworkDatagram msg = m_pUdpClientSocket->receiveDatagram();
            ui->txtClient->append(timestampMessage(" <- " + msg.data()));
        }
    } else if (ui->rdoTcp->isChecked()) {
        qDebug() << "Using TCP";
        QString msg = m_pTcpClientSocket->readAll().trimmed();
        if (!msg.isEmpty())
            ui->txtClient->append(timestampMessage(" <- " + msg));
    } else {
        qDebug() << "Using SSL";
    }
}


void MainWindow::onServerReadyRead()
{
    qDebug() << Q_FUNC_INFO;
    if (ui->rdoUdp->isChecked()) {
        qDebug() << "Using UDP";
        while (m_pUdpServerSocket->hasPendingDatagrams())
        {
            QNetworkDatagram msg = m_pUdpServerSocket->receiveDatagram();
            ui->txtServer->append(timestampMessage(" -> " + msg.data()));
        }
    } else if (ui->rdoTcp->isChecked()) {
        qDebug() << "Using TCP";
        ui->txtServer->append(timestampMessage(" -> " + m_pTcpServerSocket->readAll()));
    } else {
        qDebug() << "Using SSL";
    }
}
