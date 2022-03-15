#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "nrsocketproxy.h"

#include <QNetworkDatagram>
#include <QDateTime>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->btnConfig, &QPushButton::clicked, this, &MainWindow::onConfigButtonPressed);
    connect(ui->btnSendToLocal, &QPushButton::clicked, this, &MainWindow::onSendToLocal);
    connect(ui->btnSendToRemote, &QPushButton::clicked, this, &MainWindow::onSendToRemote);

    m_pUdpClientSocket = new QUdpSocket(this);
    m_pUdpServerSocket = new QUdpSocket(this);

    connect (m_pUdpClientSocket, &QUdpSocket::readyRead, this, &MainWindow::onClientReadyRead);
    connect (m_pUdpServerSocket, &QUdpSocket::readyRead, this, &MainWindow::onServerReadyRead);
}

MainWindow::~MainWindow()
{
    delete ui;
}


QString MainWindow::timestampMessage(const QString msg) const
{
    QString s = msg;
    QDateTime dt = QDateTime::currentDateTime();
    s.prepend(dt.toString("hh:mm:ss.zzz "));
    return s;
}

void MainWindow::onConfigButtonPressed()
{
    qDebug() << Q_FUNC_INFO;
    NrSocketProxyConfig cfg;
    cfg.localAddress = ui->txtListenAddress->text();
    cfg.remoteAddress = ui->txtSendAddress->text();
    cfg.localPort = ui->txtListenPort->text().toUInt();
    cfg.remotePort = ui->txtSendPort->text().toUInt();
    //FIXME udp is hardcoded protocol
    m_pProxy = new NrSocketProxy(cfg);
    qDebug() << "Binding receiving socket to " << cfg.remoteAddress << cfg.remotePort;
    m_pUdpServerSocket->bind(QHostAddress(cfg.remoteAddress), cfg.remotePort);
    ui->btnSendToLocal->setEnabled(true);
    ui->btnSendToRemote->setEnabled(true);
}

void MainWindow::onSendToLocal()
{
    qDebug() << Q_FUNC_INFO;
    QString msg = ui->txtToLocal->text();
    qDebug() << "Sending " << msg << " to " << m_pProxy->proxiedAddress() << m_pProxy->proxiedPort();
    m_pUdpServerSocket->writeDatagram(msg.toUtf8(), QHostAddress(m_pProxy->proxiedAddress()), m_pProxy->proxiedPort());
}

void MainWindow::onSendToRemote()
{
    qDebug() << Q_FUNC_INFO;
    QString msg = ui->txtToRemote->text();
    qDebug() << "Sending " << msg << " to " << m_pProxy->listeningAddress() << m_pProxy->listeningPort();
    m_pUdpClientSocket->writeDatagram(msg.toUtf8(), QHostAddress(m_pProxy->listeningAddress()), m_pProxy->listeningPort());
}

void MainWindow::onPauseToLocalChecked(bool paused)
{
    qDebug() << Q_FUNC_INFO << paused;
}

void MainWindow::onPauseToRemoteChecked(bool paused)
{
    qDebug() << Q_FUNC_INFO << paused;
}


void MainWindow::onClientReadyRead()
{
    qDebug() << Q_FUNC_INFO;
    if (true || ui->rdoUdp->isChecked()) {
        while (m_pUdpClientSocket->hasPendingDatagrams())
        {
            QNetworkDatagram msg = m_pUdpClientSocket->receiveDatagram();
            ui->txtClient->append(timestampMessage("<-" + msg.data()));
        }
    }
}

void MainWindow::onServerReadyRead()
{
    qDebug() << Q_FUNC_INFO;
    if (true || ui->rdoUdp->isChecked()) {
        while (m_pUdpServerSocket->hasPendingDatagrams())
        {
            QNetworkDatagram msg = m_pUdpServerSocket->receiveDatagram();
            ui->txtServer->append(timestampMessage("->" + msg.data()));
        }
    }
}
