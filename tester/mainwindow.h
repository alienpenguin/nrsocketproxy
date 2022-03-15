#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class NrSocketProxy;
class QUdpSocket;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
    QUdpSocket *m_pUdpClientSocket = nullptr;
    QUdpSocket *m_pUdpServerSocket = nullptr;
    NrSocketProxy *m_pProxy;
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
};
#endif // MAINWINDOW_H
