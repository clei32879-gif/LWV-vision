/**
 * @file GigECamera.cpp
 * @brief GigE Vision 通用相机驱动实现
 *
 * 实现 GigE Vision 标准协议:
 *   GVCP (UDP 3956): 设备发现/寄存器读写/心跳
 *   GVSP (UDP 随机端口): 图像数据流
 *
 * 兼容所有 GigE Vision 1.x/2.x 标准相机
 */
#include "GigECamera.h"
#include "../utils/Logger.h"
#include <QNetworkInterface>
#include <QElapsedTimer>
#include <cstring>
#include <QtEndian>

#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif

namespace VisionInspector {

// ============================================================
// GVCP 协议工具
// ============================================================

static quint16 nextPacketId(quint16& id) {
    if (id >= 0xEFFF) id = 1;
    return id++;
}

static QByteArray buildGvcpHeader(quint16 cmd, quint16 length, quint16 packetId) {
    QByteArray hdr(8, '\0');
    // Byte 0: message type (0x42 = cmd, 0x00 = ack)
    hdr[0] = 0x42;
    // Byte 1: flags (0x11 = acknowledge required + broadcast)
    hdr[1] = 0x11;
    // Byte 2-3: command
    qToBigEndian(cmd, (uchar*)hdr.data() + 2);
    // Byte 4-5: length (payload in 32-bit words)
    qToBigEndian(length, (uchar*)hdr.data() + 4);
    // Byte 6-7: packet id
    qToBigEndian(packetId, (uchar*)hdr.data() + 6);
    return hdr;
}

static void append32(QByteArray& buf, quint32 val) {
    char d[4];
    qToBigEndian(val, (uchar*)d);
    buf.append(d, 4);
}

static quint32 read32(const char* data, int offset = 0) {
    return qFromBigEndian<quint32>((const uchar*)data + offset);
}

static quint16 read16(const char* data, int offset = 0) {
    return qFromBigEndian<quint16>((const uchar*)data + offset);
}

// ============================================================
// 构造/析构
// ============================================================

GigECamera::GigECamera(QObject* parent)
    : ICameraDriver(parent)
{
    m_ctrlSocket = new QUdpSocket(this);
    m_heartbeatTimer = new QTimer(this);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &GigECamera::onHeartbeat);
}

GigECamera::~GigECamera()
{
    closeCamera();
}

// ============================================================
// 设备发现
// ============================================================

QList<CameraInfo> GigECamera::enumerateCameras()
{
    QList<CameraInfo> result;

    // 绑定本地控制端口
    if (!m_ctrlSocket->bind(QHostAddress::Any, 3956, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        // 3956 可能被占用, 用随机端口
        m_ctrlSocket->bind(QHostAddress::Any, 0);
    }

    // 构建 DISCOVERY 命令
    // GVCP DISCOVERY: flags=0x11, cmd=0x0002, length=0 (无 payload)
    // 但需要发一个 8 字节的全 0 payload (有些固件要求)
    QByteArray pkt = buildGvcpHeader(GVCP_DISCOVERY_CMD, 0x0000, 0x0001);
    pkt.append(8, '\0'); // discover payload (全0 = 所有设备)

    // 广播发送到 255.255.255.255:3956
    m_ctrlSocket->writeDatagram(pkt, QHostAddress::Broadcast, 3956);

    // 也发到每个网卡的子网广播
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto& iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp)) continue;
        if (!(iface.flags() & QNetworkInterface::CanBroadcast)) continue;
        for (const auto& entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
            QHostAddress bcast(entry.broadcast());
            if (!bcast.isNull() && bcast != QHostAddress::Broadcast) {
                m_ctrlSocket->writeDatagram(pkt, bcast, 3956);
            }
        }
    }

    // 等待 DISCOVERY_ACK 响应 (最多2秒)
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 2000) {
        if (m_ctrlSocket->waitForReadyRead(200)) {
            while (m_ctrlSocket->hasPendingDatagrams()) {
                QByteArray resp;
                resp.resize((int)m_ctrlSocket->pendingDatagramSize());
                QHostAddress sender;
                quint16 senderPort;
                m_ctrlSocket->readDatagram(resp.data(), resp.size(), &sender, &senderPort);

                // 解析 DISCOVERY_ACK
                if (resp.size() >= 256) {
                    quint16 ackCmd = read16(resp, 2);
                    if (ackCmd == GVCP_DISCOVERY_ACK) {
                        GigEDeviceInfo dev;
                        dev.ipAddress = sender;
                        dev.deviceIp = sender;
                        dev.devicePort = senderPort;

                        // 解析设备信息 (偏移量基于 GVCP 标准)
                        // Device specific data 从 byte 8 开始
                        if (resp.size() >= 568) {
                            char vendor[33] = {0};
                            char model[33] = {0};
                            char serial[17] = {0};
                            memcpy(vendor, resp.constData() + 8 + 52, 32);
                            memcpy(model,  resp.constData() + 8 + 84, 32);
                            memcpy(serial, resp.constData() + 8 + 168, 16);
                            dev.manufacturer = QString::fromLatin1(vendor).trimmed();
                            dev.model = QString::fromLatin1(model).trimmed();
                            dev.serialNumber = QString::fromLatin1(serial).trimmed();
                        }

                        // MAC 地址 (偏移 8+0 开始 6 字节)
                        if (resp.size() >= 14) {
                            const char* macData = resp.constData() + 8;
                            dev.macAddress = QString("%1:%2:%3:%4:%5:%6")
                                .arg((quint8)macData[0], 2, 16, QChar('0'))
                                .arg((quint8)macData[1], 2, 16, QChar('0'))
                                .arg((quint8)macData[2], 2, 16, QChar('0'))
                                .arg((quint8)macData[3], 2, 16, QChar('0'))
                                .arg((quint8)macData[4], 2, 16, QChar('0'))
                                .arg((quint8)macData[5], 2, 16, QChar('0'));
                        }

                        CameraInfo cam;
                        cam.vendor = dev.manufacturer;
                        cam.model = dev.model;
                        cam.serialNumber = dev.serialNumber;
                        cam.ipAddress = dev.ipAddress.toString();
                        cam.id = dev.ipAddress.toString(); // 用IP做标识

                        // 去重 (同IP不重复)
                        bool dup = false;
                        for (const auto& r : result) {
                            if (r.ipAddress == cam.ipAddress) { dup = true; break; }
                        }
                        if (!dup && !cam.model.isEmpty()) {
                            result.append(cam);
                            VI_LOG_INFO(QString("GigE 发现: %1 %2 (%3) IP=%4")
                                .arg(cam.vendor, cam.model, cam.serialNumber, cam.ipAddress));
                        }
                    }
                }
            }
        }
    }

    if (result.isEmpty()) {
        VI_LOG_WARN("GigE 扫描: 未发现任何 GigE Vision 相机");
    }

    return result;
}

// ============================================================
// 连接/断开
// ============================================================

bool GigECamera::openCamera(const QString& cameraId)
{
    if (m_isOpen) return true;

    // cameraId = IP 地址
    m_deviceIp = QHostAddress(cameraId);
    if (m_deviceIp.isNull()) {
        VI_LOG_ERROR("GigE: 无效的相机IP: " + cameraId);
        return false;
    }
    m_devicePort = 3956;

    // 绑定控制通道
    m_ctrlSocket->close();
    if (!m_ctrlSocket->bind(QHostAddress::Any, 0)) {
        VI_LOG_ERROR("GigE: 控制通道绑定失败");
        return false;
    }

    // 测试连接: 读取设备型号寄存器
    uint32_t modelVal = 0;
    if (!readRegister(REG_DEVICE_MODE, modelVal)) {
        VI_LOG_ERROR("GigE: 无法连接相机 " + cameraId);
        return false;
    }

    // 读取图像尺寸
    uint32_t w = 0, h = 0, fmt = 0;
    readRegister(REG_WIDTH, w);
    readRegister(REG_HEIGHT, h);
    readRegister(REG_PIXEL_FORMAT, fmt);
    m_imageWidth = (int)w;
    m_imageHeight = (int)h;
    if (m_imageWidth > 0 && m_imageHeight > 0) m_pixelFormat = (int)fmt;

    // 设置心跳
    controlChannel(3000);

    // 绑定数据通道
    if (!gvspBindDataChannel()) {
        VI_LOG_ERROR("GigE: 数据通道绑定失败");
        return false;
    }

    // 更新相机信息
    m_currentCamera.id = cameraId;
    m_currentCamera.ipAddress = cameraId;
    m_currentCamera.isConnected = true;
    m_deviceInfo.ipAddress = m_deviceIp;

    // 读取厂商信息 (字符串寄存器)
    auto readStr = [&](uint32_t reg, int len) -> QString {
        QByteArray buf(len, '\0');
        for (int i = 0; i < len; i += 4) {
            uint32_t val = 0;
            if (!readRegister(reg + i, val)) break;
            buf[i]   = (char)((val >> 24) & 0xFF);
            buf[i+1] = (char)((val >> 16) & 0xFF);
            buf[i+2] = (char)((val >> 8)  & 0xFF);
            buf[i+3] = (char)(val & 0xFF);
        }
        return QString::fromLatin1(buf).trimmed();
    };

    m_currentCamera.vendor = readStr(REG_DEVICE_VENDOR, 32);
    m_currentCamera.model = readStr(REG_DEVICE_MODEL, 32);
    m_currentCamera.serialNumber = readStr(REG_DEVICE_SERIAL, 16);

    m_isOpen = true;

    // 启动心跳定时器
    m_heartbeatTimer->start(1500);

    VI_LOG_INFO(QString("GigE 相机已打开: %1 %2 %3x%4 IP=%5")
        .arg(m_currentCamera.vendor, m_currentCamera.model)
        .arg(m_imageWidth).arg(m_imageHeight)
        .arg(cameraId));
    emit connectionChanged(true);
    return true;
}

void GigECamera::closeCamera()
{
    if (!m_isOpen) return;

    stopAcquisition();

    m_heartbeatTimer->stop();

    // 关闭数据通道
    if (m_dataSocket) {
        m_dataSocket->close();
        m_dataSocket->deleteLater();
        m_dataSocket = nullptr;
    }

    m_ctrlSocket->close();
    m_isOpen = false;
    m_currentCamera.isConnected = false;

    VI_LOG_INFO("GigE 相机已关闭");
    emit connectionChanged(false);
}

// ============================================================
// GVCP 寄存器读写
// ============================================================

bool GigECamera::readRegister(uint32_t address, uint32_t& value)
{
    if (!m_ctrlSocket->isValid()) return false;

    quint16 pid = nextPacketId(m_lastPacketId);
    // READREG: cmd=0x0080, payload=1 register (4 bytes = 1 word)
    QByteArray pkt = buildGvcpHeader(GVCP_READREG_CMD, 0x0001, pid);
    append32(pkt, address);

    m_ctrlSocket->writeDatagram(pkt, m_deviceIp, m_devicePort);

    // 等待 ACK
    if (m_ctrlSocket->waitForReadyRead(500)) {
        QByteArray resp;
        resp.resize((int)m_ctrlSocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;
        m_ctrlSocket->readDatagram(resp.data(), resp.size(), &sender, &senderPort);

        if (resp.size() >= 12) {
            quint16 ackCmd = read16(resp, 2);
            if (ackCmd == GVCP_READREG_ACK) {
                // ACK: 8 header + 4 status + 4 register value
                if (resp.size() >= 16) {
                    value = read32(resp, 12);
                    return true;
                }
            }
        }
    }
    return false;
}

bool GigECamera::writeRegister(uint32_t address, uint32_t value)
{
    if (!m_ctrlSocket->isValid()) return false;

    quint16 pid = nextPacketId(m_lastPacketId);
    // WRITEREG: cmd=0x0082, payload=2 words (address + value)
    QByteArray pkt = buildGvcpHeader(GVCP_WRITEREG_CMD, 0x0002, pid);
    append32(pkt, address);
    append32(pkt, value);

    m_ctrlSocket->writeDatagram(pkt, m_deviceIp, m_devicePort);

    if (m_ctrlSocket->waitForReadyRead(500)) {
        QByteArray resp;
        resp.resize((int)m_ctrlSocket->pendingDatagramSize());
        m_ctrlSocket->readDatagram(resp.data(), resp.size());

        if (resp.size() >= 12) {
            quint16 ackCmd = read16(resp, 2);
            if (ackCmd == GVCP_WRITEREG_ACK) {
                quint32 status = read32(resp, 8);
                return status == 0; // 0=成功
            }
        }
    }
    return false;
}

bool GigECamera::controlChannel(int32_t heartbeatMs)
{
    // 写心跳超时寄存器
    return writeRegister(REG_HEARTBEAT_TIMEOUT, (uint32_t)heartbeatMs);
}

// ============================================================
// GVSP 数据通道
// ============================================================

bool GigECamera::gvspBindDataChannel()
{
    m_dataSocket = new QUdpSocket(this);

    // 绑定随机端口接收图像数据
    if (!m_dataSocket->bind(QHostAddress::Any, 0)) {
        VI_LOG_ERROR("GigE: 无法绑定GVSP数据端口");
        return false;
    }
    m_localDataPort = m_dataSocket->localPort();

    // 向相机注册数据通道端口
    // Stream channel 0 destination port
    writeRegister(REG_STREAM_CHANNEL_PORT, m_localDataPort);

    // 设置流通道 IP (本机 IP)
    const auto addrs = QNetworkInterface::allAddresses();
    for (const auto& addr : addrs) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) {
            // 写入 IP (大端 uint32)
            quint32 ip = addr.toIPv4Address();
            writeRegister(REG_STREAM_CHANNEL_IP, ip);
            VI_LOG_INFO(QString("GigE 数据通道: 本地 %1:%2")
                .arg(addr.toString()).arg(m_localDataPort));
            break;
        }
    }

    connect(m_dataSocket, &QUdpSocket::readyRead, this, &GigECamera::onDataReady);
    return true;
}

void GigECamera::onDataReady()
{
    while (m_dataSocket->hasPendingDatagrams()) {
        QByteArray data;
        data.resize((int)m_dataSocket->pendingDatagramSize());
        m_dataSocket->readDatagram(data.data(), data.size());
        processGvspPacket(data);
    }
}

void GigECamera::processGvspPacket(const QByteArray& data)
{
    if (data.size() < 8) return;

    const char* p = data.constData();
    // GVSP header: status(2) + header_size(1) + packet_type(1) + block_id(4) + packet_id(4)
    // 但实际格式: block_id_high(2) + packet_id(2) + format_flag(1) + packet_type(1) + ...
    // 简化: 按 packet_type 分类

    quint16 blockId = read16(p, 0);
    quint16 packetId = read16(p, 2);
    quint8 formatId = (quint8)p[4];
    quint8 packetType = (quint8)p[5];

    Q_UNUSED(formatId);

    if (packetType == GVSP_HEADER_LEADER) {
        // 帧头: 解析图像信息
        if (data.size() >= 36) {
            m_imageWidth = (int)read32(p, 20);
            m_imageHeight = (int)read32(p, 24);
            m_pixelFormat = (int)read32(p, 28);

            // 计算预期大小
            int bpp = 1;
            if (m_pixelFormat == 0x01080001) bpp = 1;       // Mono8
            else if (m_pixelFormat == 0x01100003) bpp = 2;  // Mono16
            else if (m_pixelFormat == 0x02180015) bpp = 3;  // RGB8
            else if (m_pixelFormat == 0x02180016) bpp = 3;  // BGR8
            m_expectedSize = m_imageWidth * m_imageHeight * bpp;

            QMutexLocker lk(&m_frameMutex);
            m_frameBuffer.clear();
            m_frameBuffer.reserve(m_expectedSize);
            m_packetsReceived = 0;
            m_lastBlockId = blockId;
            m_leaderReceived = true;
            m_frameComplete = false;
        }
    }
    else if (packetType == GVSP_HEADER_DATA && m_leaderReceived) {
        // 帧数据: 8字节 GVSP header 后面是像素数据
        if (data.size() > 8) {
            QMutexLocker lk(&m_frameMutex);
            if (blockId == m_lastBlockId) {
                m_frameBuffer.append(data.constData() + 8, data.size() - 8);
                m_packetsReceived++;
            }
        }
    }
    else if (packetType == GVSP_HEADER_TRAILER && m_leaderReceived) {
        // 帧尾: 一帧完成
        QMutexLocker lk(&m_frameMutex);
        if (blockId == m_lastBlockId) {
            m_frameComplete = true;
        }
    }
}

void GigECamera::resetGvspState()
{
    QMutexLocker lk(&m_frameMutex);
    m_frameBuffer.clear();
    m_packetsReceived = 0;
    m_lastBlockId = -1;
    m_leaderReceived = false;
    m_frameComplete = false;
}

// ============================================================
// 心跳
// ============================================================

void GigECamera::onHeartbeat()
{
    if (!m_isOpen) return;

    // 发送心跳 (READREG 自己的寄存器, 或发 HEARTBEAT 命令)
    // GVCP 心跳: 0x0003 = DISCOVERY 但实际应用中发 READREG 也行
    // 更标准的做法: 发 CONTROL packet (cmd=0x0002, flag=0x0001)
    quint16 pid = nextPacketId(m_lastPacketId);
    QByteArray pkt = buildGvcpHeader(0x0002, 0x0000, pid); // CONTROL cmd
    pkt.append(8, '\0');
    m_ctrlSocket->writeDatagram(pkt, m_deviceIp, m_devicePort);

    // 非阻塞读 (清除可能的 ACK)
    while (m_ctrlSocket->hasPendingDatagrams()) {
        QByteArray r;
        r.resize((int)m_ctrlSocket->pendingDatagramSize());
        m_ctrlSocket->readDatagram(r.data(), r.size());
    }
}

// ============================================================
// 相机参数
// ============================================================

CameraParams GigECamera::getParams() const
{
    CameraParams params;
    if (!m_isOpen) return params;
    params.width = m_imageWidth;
    params.height = m_imageHeight;

    uint32_t v = 0;
    const_cast<GigECamera*>(this)->readRegister(REG_EXPOSURE_TIME, v);
    params.exposureTime = (double)v;
    const_cast<GigECamera*>(this)->readRegister(REG_GAIN, v);
    params.gain = (double)v;

    return params;
}

bool GigECamera::setParams(const CameraParams& params)
{
    if (!m_isOpen) return false;
    writeRegister(REG_EXPOSURE_TIME, (uint32_t)params.exposureTime);
    writeRegister(REG_GAIN, (uint32_t)params.gain);
    return true;
}

// ============================================================
// 采集控制
// ============================================================

bool GigECamera::startAcquisition()
{
    if (!m_isOpen || m_isAcquiring) return false;

    resetGvspState();

    // 发送 ACQ_START
    if (!writeRegister(REG_ACQ_START, 1)) {
        VI_LOG_ERROR("GigE: 采集启动命令失败");
        return false;
    }

    m_isAcquiring = true;

    // 启动采集定时器在独立线程
    emit connectionChanged(true); // 通知状态
    VI_LOG_INFO("GigE 采集已启动");
    return true;
}

bool GigECamera::stopAcquisition()
{
    if (!m_isAcquiring) return true;

    writeRegister(REG_ACQ_STOP, 1);
    m_isAcquiring = false;

    VI_LOG_INFO("GigE 采集已停止");
    return true;
}

CvImage GigECamera::grabFrame(int timeoutMs)
{
    if (!m_isOpen) return CvImage();

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        QThread::msleep(1);

        QMutexLocker lk(&m_frameMutex);
        if (m_frameComplete && m_frameBuffer.size() >= m_expectedSize) {
            // 转换图像
            CvImage frame;
#ifdef VI_HAS_OPENCV
            if (m_pixelFormat == 0x01080001) {
                // Mono8
                frame = cv::Mat(m_imageHeight, m_imageWidth, CV_8UC1,
                               (void*)m_frameBuffer.constData()).clone();
            }
            else if (m_pixelFormat == 0x02180015 || m_pixelFormat == 0x02180016) {
                // RGB8/BGR8
                frame = cv::Mat(m_imageHeight, m_imageWidth, CV_8UC3,
                               (void*)m_frameBuffer.constData()).clone();
                if (m_pixelFormat == 0x02180015)
                    cv::cvtColor(frame, frame, cv::COLOR_RGB2BGR);
            }
            else {
                // 默认当 Mono8
                frame = cv::Mat(m_imageHeight, m_imageWidth, CV_8UC1,
                               (void*)m_frameBuffer.constData()).clone();
            }
#else
            // QImage 模式
            if (m_pixelFormat == 0x01080001) {
                frame = QImage((const uchar*)m_frameBuffer.constData(),
                              m_imageWidth, m_imageHeight,
                              m_imageWidth, QImage::Format_Grayscale8).copy();
            } else {
                frame = QImage((const uchar*)m_frameBuffer.constData(),
                              m_imageWidth, m_imageHeight,
                              m_imageWidth * 3, QImage::Format_RGB888).copy();
            }
#endif
            // 重置状态准备下一帧
            m_frameBuffer.clear();
            m_frameComplete = false;
            m_leaderReceived = false;

            m_lastFrame = frame;
            return frame;
        }
    }

    // 超时: 返回最后一帧
    return m_lastFrame;
}

bool GigECamera::triggerOnce()
{
    if (!m_isOpen) return false;
    return writeRegister(REG_TRIGGER_SOFTWARE, 1);
}

QStringList GigECamera::supportedPixelFormats() const
{
    return {"Mono8", "Mono16", "RGB8", "BGR8"};
}

void GigECamera::onGrabFrame()
{
    CvImage frame = grabFrame(500);
#ifdef VI_HAS_OPENCV
    if (!frame.empty())
#else
    if (!frame.isNull())
#endif
    {
        m_lastFrame = frame;
        emit imageReceived(frame);
    }
}

void GigECamera::onStartGrabThread()
{
    m_grabTimer = new QTimer();
    m_grabTimer->setInterval(33); // ~30fps
    connect(m_grabTimer, &QTimer::timeout, this, &GigECamera::onGrabFrame, Qt::DirectConnection);
    m_grabTimer->start();
}

void GigECamera::onStopGrabThread()
{
    if (m_grabTimer) {
        m_grabTimer->stop();
        delete m_grabTimer;
        m_grabTimer = nullptr;
    }
}

} // namespace VisionInspector
