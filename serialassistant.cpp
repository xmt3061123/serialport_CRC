#include "serialassistant.h"
#include "ui_serialassistant.h"
#include <QMessageBox>
#include <QDateTime>
#include <QDebug>

// 定义静态成员：增加头尾效验
const QByteArray SerialAssistant::START_DELIMITER = QByteArray::fromHex("AABB");
const QByteArray SerialAssistant::END_DELIMITER = QByteArray::fromHex("CCDD");

// 在构造函数中打开日志文件
SerialAssistant::SerialAssistant(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::SerialAssistant)
    , totalSentBytes(0)
    , totalReceivedBytes(0)
    , totalPacketsSent(0)
    , totalPacketsReceived(0)
    , lostPackets(0)
    , logFile("packet_log.txt")
{
    ui->setupUi(this); // 设置UI
    serial = new QSerialPort(this); // 创建QSerialPort对象
    initSerialPort(); // 初始化串口设置

    connect(serial, &QSerialPort::readyRead, this, &SerialAssistant::readData); // 连接readyRead信号到readData槽

    // 添加状态标签到状态栏
    statusBar()->addPermanentWidget(ui->packetLossLabel);
    statusBar()->addPermanentWidget(ui->totalSentLabel);
    statusBar()->addPermanentWidget(ui->totalReceivedLabel);

    updateStatusBar(); // 更新状态栏

    // 打开日志文件进行写入
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QMessageBox::critical(this, tr("错误"), tr("无法打开日志文件!"));
    } else {
        logStream.setDevice(&logFile); // 设置日志流的设备为日志文件
    }
}

// 析构函数，关闭日志文件
SerialAssistant::~SerialAssistant()
{
    delete ui;
    logFile.close();
}

// 初始化串口设置
void SerialAssistant::initSerialPort()
{
    updatePortList(); // 更新可用的串口列表

    // 添加波特率选项
    ui->baudRateCombo->addItem(QStringLiteral("115200"), QSerialPort::Baud115200);
    ui->baudRateCombo->addItem(QStringLiteral("9600"), QSerialPort::Baud9600);


    // 添加数据位选项
    ui->dataBitsCombo->addItem(QStringLiteral("8"), QSerialPort::Data8);
    ui->dataBitsCombo->addItem(QStringLiteral("7"), QSerialPort::Data7);

    // 添加校验位选项
    ui->parityCombo->addItem(QStringLiteral("None"), QSerialPort::NoParity);
    ui->parityCombo->addItem(QStringLiteral("Even"), QSerialPort::EvenParity);
    ui->parityCombo->addItem(QStringLiteral("Odd"), QSerialPort::OddParity);

    // 添加停止位选项
    ui->stopBitsCombo->addItem(QStringLiteral("1"), QSerialPort::OneStop);
    ui->stopBitsCombo->addItem(QStringLiteral("2"), QSerialPort::TwoStop);
}

// 更新串口列表
void SerialAssistant::updatePortList()
{
    ui->portCombo->clear(); // 清空串口下拉列表
    const auto infos = QSerialPortInfo::availablePorts(); // 获取可用串口信息
    for (const QSerialPortInfo &info : infos) {
        ui->portCombo->addItem(info.portName()); // 将串口名称添加到下拉列表中
    }
}

// 打开/关闭串口按钮点击事件处理函数
void SerialAssistant::on_openCloseButton_clicked()
{
    if (serial->isOpen()) {
        serial->close(); // 关闭串口
        ui->openCloseButton->setText("打开"); // 更新按钮文本
        statusBar()->showMessage("串口已关闭"); // 更新状态栏信息
    } else {
        // 设置串口参数
        serial->setPortName(ui->portCombo->currentText());
        serial->setBaudRate(ui->baudRateCombo->currentData().toInt());
        serial->setDataBits(static_cast<QSerialPort::DataBits>(ui->dataBitsCombo->currentData().toInt()));
        serial->setParity(static_cast<QSerialPort::Parity>(ui->parityCombo->currentData().toInt()));
        serial->setStopBits(static_cast<QSerialPort::StopBits>(ui->stopBitsCombo->currentData().toInt()));

        if (serial->open(QIODevice::ReadWrite)) {
            ui->openCloseButton->setText("关闭"); // 更新按钮文本
            statusBar()->showMessage("串口已打开"); // 更新状态栏信息
        } else {
            QMessageBox::critical(this, tr("错误"), serial->errorString()); // 弹出错误信息
            statusBar()->showMessage("打开串口失败"); // 更新状态栏信息
        }
    }
}

// 发送按钮点击事件处理函数
void SerialAssistant::on_sendButton_clicked()
{
    if (serial->isOpen()) {
        QByteArray data = ui->sendTextEdit->toPlainText().toUtf8(); // 获取要发送的数据
        QByteArray packet = START_DELIMITER + data + END_DELIMITER; // 构建数据包
        quint16 crc = calculateCRC(packet); // 计算CRC校验码
        packet.append((char)(crc >> 8)); // 添加高位CRC
        packet.append((char)(crc & 0xFF)); // 添加低位CRC

        qint64 bytesWritten = serial->write(packet); // 发送数据包
        if (bytesWritten == -1) {
            QMessageBox::warning(this, tr("发送错误"), tr("数据发送失败!")); // 弹出警告信息
        } else {
            totalSentBytes += bytesWritten; // 更新发送字节数
            totalPacketsSent++; // 更新发送包数
            updateStatusBar(); // 更新状态栏
            qDebug() << "Sent packet:" << packet.toHex(); // 调试输出发送的数据包
        }
    } else {
        QMessageBox::warning(this, tr("警告"), tr("串口未打开!")); // 弹出警告信息
    }
}

// 读取数据
void SerialAssistant::readData()
{
    receiveBuffer += serial->readAll(); // 读取所有可用数据到接收缓冲区
    qDebug() << "Current buffer:" << receiveBuffer.toHex(); // 调试输出当前缓冲区数据

    while (receiveBuffer.contains(START_DELIMITER) && receiveBuffer.contains(END_DELIMITER)) {
        int start = receiveBuffer.indexOf(START_DELIMITER); // 找到起始标志位置
        int end = receiveBuffer.indexOf(END_DELIMITER, start + START_DELIMITER.size()); // 找到结束标志位置

        if (start != -1 && end != -1) {
            QByteArray packet = receiveBuffer.mid(start, end - start + END_DELIMITER.size() + 2); // +2 for CRC
            receiveBuffer = receiveBuffer.mid(end + END_DELIMITER.size() + 2); // 更新接收缓冲区

            totalReceivedBytes += packet.size(); // 更新接收字节数
            totalPacketsReceived++; // 更新接收包数

            if (verifyCRC(packet)) {
                QByteArray data = packet.mid(START_DELIMITER.size(), packet.size() - START_DELIMITER.size() - END_DELIMITER.size() - 2); // 提取数据
                ui->receiveTextEdit->append(QString(data)); // 显示接收的数据
                qDebug() << "Received valid packet. Data:" << data; // 调试输出接收到的数据
            } else {
                ui->receiveTextEdit->append("接收到损坏的数据!"); // 显示接收到的损坏数据
                lostPackets++; // 更新丢包数
                logLostPacket(packet); // 记录丢包信息
                qDebug() << "Received corrupted packet:" << packet.toHex(); // 调试输出接收到的损坏数据包
            }
        } else {
            break; // 如果没有完整的数据包，则退出循环
        }
    }

    updateStatusBar(); // 更新状态栏
}

// 清空接收窗口按钮点击事件处理函数
void SerialAssistant::on_clearReceiveButton_clicked()
{
    ui->receiveTextEdit->clear(); // 清空接收窗口
}

// 清空发送窗口按钮点击事件处理函数
void SerialAssistant::on_clearSendButton_clicked()
{
    ui->sendTextEdit->clear(); // 清空发送窗口
}

// 计算CRC校验码
quint16 SerialAssistant::calculateCRC(const QByteArray &data)
{
    quint16 crc = 0xFFFF; // 初始化CRC
    for (int i = 0; i < data.size(); i++) {
        crc ^= (quint8)data[i]; // 逐字节计算CRC
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001; // 如果最低位为1，则右移并异或多项式
            } else {
                crc = crc >> 1; // 否则直接右移
            }
        }
    }
    return crc;
}

// 校验CRC
bool SerialAssistant::verifyCRC(const QByteArray &packet)
{
    if (packet.size() < 2) return false; // 如果数据包长度小于2，则返回false
    QByteArray data = packet.left(packet.size() - 2); // 获取数据部分，不包括CRC
    quint16 receivedCRC = ((quint8)packet[packet.size() - 2] << 8) | (quint8)packet[packet.size() - 1]; // 提取收到的CRC
    quint16 calculatedCRC = calculateCRC(data); // 计算数据部分的CRC
    return receivedCRC == calculatedCRC; // 返回校验结果
}

// 更新状态栏
void SerialAssistant::updateStatusBar()
{
    ui->packetLossLabel->setText(QString("丢包个数: %1").arg(lostPackets)); // 更新丢包个数标签
    ui->totalSentLabel->setText(QString("发送总计: %1 字节").arg(totalSentBytes)); // 更新发送字节数标签
    ui->totalReceivedLabel->setText(QString("接收总计: %1 字节").arg(totalReceivedBytes)); // 更新接收字节数标签
}

// 记录丢包信息
void SerialAssistant::logLostPacket(const QByteArray &packet)
{
    if (logStream.device() != nullptr) {
        QString packetString = QString::fromUtf8(packet); // 将包数据转换为字符串
        logStream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                  << " - 丢包: " << packet.toHex() << " \n(字符串: " << packetString << ")\n"; // 写入日志
        logStream.flush(); // 刷新日志流
    }
}

