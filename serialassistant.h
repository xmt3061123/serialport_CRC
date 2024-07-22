#ifndef SERIALASSISTANT_H
#define SERIALASSISTANT_H

#include <QMainWindow>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QByteArray>
#include <QFile>
#include <QTextStream>

QT_BEGIN_NAMESPACE
namespace Ui { class SerialAssistant; }
QT_END_NAMESPACE

class SerialAssistant : public QMainWindow
{
    Q_OBJECT

public:
    SerialAssistant(QWidget *parent = nullptr);
    ~SerialAssistant();

private slots:
    void on_openCloseButton_clicked();
    void on_sendButton_clicked();
    void readData();
    void on_clearReceiveButton_clicked();
    void on_clearSendButton_clicked();

private:
    Ui::SerialAssistant *ui;
    QSerialPort *serial;
    void initSerialPort();
    void updatePortList();
    quint16 calculateCRC(const QByteArray &data);
    bool verifyCRC(const QByteArray &packet);
    void updateStatusBar();
    void logLostPacket(const QByteArray &packet);

    qint64 totalSentBytes;
    qint64 totalReceivedBytes;
    int totalPacketsSent;
    int totalPacketsReceived;
    int lostPackets;

    QByteArray receiveBuffer;

    static const QByteArray START_DELIMITER;
    static const QByteArray END_DELIMITER;
    QFile logFile;
    QTextStream logStream;
};

#endif // SERIALASSISTANT_H
