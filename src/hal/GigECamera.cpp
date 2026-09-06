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
#include <QProcess>
#include <QThread>
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

static QByteArray buildGvcpHeader(quint16 cmd, quint16 length, quint16 packetId, bool broadcast = false) {
    QByteArray hdr(8, '\0');
    // Byte 0: message type (0x42 = cmd, 0x00 = ack)
    hdr[0] = 0x42;
    // Byte 1: flags — bit0=ack required, bit4=broadcast
    // 单播 READREG/WRITEREG 用 0x01; DISCOVERY 广播用 0x11
    hdr[1] = broadcast ? 0x11 : 0x01;
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

    // 构建 DISCOVERY 命令 (广播)
    QByteArray pkt = buildGvcpHeader(GVCP_DISCOVERY_CMD, 0x0000, 0x0001, true);
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

    // 自动查找与相机IP对应的本地网卡
    // 针对 169.254.x.x 多网口配置:
    //   CCD1 网卡 169.254.1.1  →  相机 169.254.1.11
    //   CCD2 网卡 169.254.2.2  →  相机 169.254.2.22
    // 匹配规则: 本地IP的第三段 == 相机IP的第三段
    QHostAddress localBindAddr = QHostAddress::Any;
    quint32 camIpVal = m_deviceIp.toIPv4Address();
    int camThirdOctet = (camIpVal >> 8) & 0xFF;  // 第三段

    VI_LOG_INFO(QString("GigE: 相机IP %1, 第三段=%2, 正在匹配本地网卡...")
        .arg(cameraId).arg(camThirdOctet));

    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto& iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp)) continue;
        if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
        for (const auto& entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
            quint32 localIpVal = entry.ip().toIPv4Address();
            int localThirdOctet = (localIpVal >> 8) & 0xFF;
            if (localThirdOctet == camThirdOctet) {
                localBindAddr = entry.ip();
                VI_LOG_INFO(QString("GigE: 匹配网卡 %1 (%2) → 相机 %3")
                    .arg(iface.humanReadableName(), entry.ip().toString(), cameraId));
                break;
            }
        }
        if (localBindAddr != QHostAddress::Any) break;
    }

    // 绑定控制通道到匹配的本地网卡
    m_ctrlSocket->close();
    if (!m_ctrlSocket->bind(localBindAddr, 0)) {
        VI_LOG_ERROR("GigE: 控制通道绑定失败 (本地地址: " + localBindAddr.toString() + ")");
        return false;
    }

    // 先发一个 DISCOVERY 单播到相机IP (有些相机需要先收到发现包才响应寄存器读)
    QByteArray discoverPkt = buildGvcpHeader(GVCP_DISCOVERY_CMD, 0x0000, 0x0001, false);
    discoverPkt.append(8, '\0');
    m_ctrlSocket->writeDatagram(discoverPkt, m_deviceIp, m_devicePort);

    // 等待 DISCOVERY ACK (非阻塞, 某些相机不需要)
    bool discoveryOk = false;
    if (m_ctrlSocket->waitForReadyRead(1000)) {
        QByteArray resp;
        resp.resize((int)m_ctrlSocket->pendingDatagramSize());
        m_ctrlSocket->readDatagram(resp.data(), resp.size());
        if (resp.size() >= 256) {
            quint16 ackCmd = read16(resp, 2);
            if (ackCmd == GVCP_DISCOVERY_ACK) {
                discoveryOk = true;
                VI_LOG_INFO(QString("GigE: DISCOVERY ACK 收到, 相机 %1 可达").arg(cameraId));
            }
        }
    }

    // 测试连接: 读取设备模式寄存器
    uint32_t modelVal = 0;
    if (!readRegister(REG_DEVICE_MODE, modelVal)) {
        // 尝试自动添加防火墙规则 (仅首次)
        static bool firewallAttempted = false;
        if (!firewallAttempted) {
            firewallAttempted = true;
            VI_LOG_WARN("GigE: 尝试添加防火墙规则...");
            QProcess proc;
            proc.start("netsh", {"advfirewall", "firewall", "add", "rule",
                "name=LWVision GigE Camera",
                "dir=in", "action=allow", "protocol=UDP", "localport=3956"});
            proc.waitForFinished(3000);
            if (proc.exitCode() == 0) {
                VI_LOG_INFO("GigE: 防火墙规则已添加, 重试连接...");
                // 重新绑定并重试
                m_ctrlSocket->close();
                m_ctrlSocket->bind(localBindAddr, 0);
                QThread::msleep(100);
                m_ctrlSocket->writeDatagram(discoverPkt, m_deviceIp, m_devicePort);
                if (m_ctrlSocket->waitForReadyRead(1000)) {
                    QByteArray r;
                    r.resize((int)m_ctrlSocket->pendingDatagramSize());
                    m_ctrlSocket->readDatagram(r.data(), r.size());
                }
                if (readRegister(REG_DEVICE_MODE, modelVal)) {
                    goto connectSuccess;
                }
            }
        }

        QString detail;
        if (discoveryOk) {
            detail = QString("相机 DISCOVERY 可达但寄存器读取失败 (协议不兼容?)");
        } else if (localBindAddr == QHostAddress::Any) {
            detail = QString("未找到与 %1 同网段的本地网卡 (IP第三段不匹配)").arg(cameraId);
        } else {
            detail = QString("网卡 %1 → 相机 %2 不通 (网线/电源/防火墙?)")
                .arg(localBindAddr.toString(), cameraId);
        }
        VI_LOG_ERROR("GigE: 连接失败 " + detail);
        m_lastError = detail;
        return false;
    }

connectSuccess:

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
    // READREG: cmd=0x0080, payload=1个寄存器地址 (length=1, 单位是32位字)
    QByteArray pkt = buildGvcpHeader(GVCP_READREG_CMD, 0x0001, pid);
    append32(pkt, address);

    // 重试3次, 逐步增加超时 (巴斯勒相机首次响应可能较慢)
    for (int retry = 0; retry < 3; retry++) {
        int timeout = 300 + retry * 500;  // 300ms, 800ms, 1300ms
        m_ctrlSocket->writeDatagram(pkt, m_deviceIp, m_devicePort);

        if (m_ctrlSocket->waitForReadyRead(timeout)) {
            QByteArray resp;
            resp.resize((int)m_ctrlSocket->pendingDatagramSize());
            QHostAddress sender;
            quint16 senderPort;
            m_ctrlSocket->readDatagram(resp.data(), resp.size(), &sender, &senderPort);

            // GVCP ACK: 8字节头 [0]=类型0x00 [1]=flag [2-3]=ack_cmd [4-5]=length [6-7]=ack_id,
            // 之后是 status(2)+reserved(2)+寄存器值(4) → 值在偏移12.
            // (与 DISCOVERY_ACK 解析同基准: ack_cmd 在 2-3, 载荷从 8 开始)
            if (sender == m_deviceIp && resp.size() >= 16) {
                quint16 ackCmd = read16(resp, 2);
                if (ackCmd == GVCP_READREG_ACK) {
                    value = read32(resp, 12);
                    return true;
                }
                // 不匹配的包(如迟到的 DISCOVERY_ACK)丢弃, 由重试循环继续等待
            }
        }
    }

    VI_LOG_ERROR(QString("GigE READREG 失败: addr=0x%1 目标=%2 (3次重试均超时)")
        .arg(address, 8, 16, QChar('0')).arg(m_deviceIp.toString()));
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
        QHostAddress sender;
        m_ctrlSocket->readDatagram(resp.data(), resp.size(), &sender);

        // WRITEREG_ACK: 8字节头(同上) + status(2)+reserved(2) = 12字节, status 在偏移8.
        // 标准应答总长12字节, 不要用 >=16 判断 (会永远失败).
        if (sender == m_deviceIp && resp.size() >= 12) {
            quint16 ackCmd = read16(resp, 2);
            if (ackCmd == GVCP_WRITEREG_ACK) {
                return read16(resp, 8) == 0; // 0=GVCP_STATUS_SUCCESS
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

    // 找到与控制通道相同的本地IP (即与相机同子网的网卡)
    QHostAddress localAddr = QHostAddress::Any;
    if (m_ctrlSocket->localAddress() != QHostAddress::Any &&
        m_ctrlSocket->localAddress() != QHostAddress::LocalHost) {
        localAddr = m_ctrlSocket->localAddress();
    }

    // 绑定到同一网卡的随机端口
    if (!m_dataSocket->bind(localAddr, 0)) {
        VI_LOG_ERROR("GigE: 无法绑定GVSP数据端口 (地址: " + localAddr.toString() + ")");
        return false;
    }
    m_localDataPort = m_dataSocket->localPort();

    // 向相机注册数据通道端口和IP
    writeRegister(REG_STREAM_CHANNEL_PORT, m_localDataPort);
    quint32 localIp = localAddr.toIPv4Address();
    writeRegister(REG_STREAM_CHANNEL_IP, localIp);

    VI_LOG_INFO(QString("GigE 数据通道: %1:%2 → 相机 %3")
        .arg(localAddr.toString()).arg(m_localDataPort).arg(m_deviceIp.toString()));

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
    // GVSP 包头 (GigE Vision 2.0 GVSP 格式):
    //   status(2,大端) | block_id(2,大端) | 数据含义由第4字节 packet_format 决定:
    //     0x01=LEADER 0x02=TRAILER 0x03=PAYLOAD(数据)
    // 注意: leader 的图像信息在8字节扩展头之后 (offset 8+8=16 起为 payload):
    //   16: x_padding+reso信息, leader 固定结构中 width@+28, height@+32,
    //       pixel_format@+36 (相对包起点)
    quint16 blockId = read16(p, 2);
    quint8 packetFormat = (quint8)p[4];
    quint8 packetStatus = (quint8)p[0] >> 0;  // 低字节在前? 实际 status 是大端2字节
    Q_UNUSED(packetStatus);

    // 状态非0 = 出错包, 丢弃
    if (read16(p, 0) != 0) {
        resetGvspState();
        return;
    }

    // packet_format 字段 (p[4]): 低4位为实体格式, 0x8X 标志位忽略
    const quint8 type = packetFormat & 0x0F;

    if (type == GVSP_HEADER_LEADER) {
        // LEADER: 8字节GVSP头 + 8字节扩展头 + payload
        // 标准leader payload: tag(4) + reserved(4) + x_offset(4) + y_offset(4)
        //                     + width(4) + height(4) + pixel_format(4) ...
        // 相对包头: width@28+16=... 按GIgeV2: payload起始=8, width@8+20, height@8+24, pf@8+28
        if (data.size() >= 8 + 32) {
            m_imageWidth = (int)read32(p, 8 + 20);
            m_imageHeight = (int)read32(p, 8 + 24);
            m_pixelFormat = (int)read32(p, 8 + 28);

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
    else if (type == GVSP_HEADER_DATA && m_leaderReceived) {
        // PAYLOAD包: 标准头8字节(本格式下无扩展头)后即像素数据
        if (data.size() > 8) {
            QMutexLocker lk(&m_frameMutex);
            if (blockId == m_lastBlockId) {
                m_frameBuffer.append(data.constData() + 8, data.size() - 8);
                m_packetsReceived++;
            }
        }
    }
    else if (type == GVSP_HEADER_TRAILER && m_leaderReceived) {
        // TRAILER: 一帧完成
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

    // GVCP 心跳标准做法: 发 READREG 读任意安全寄存器 (相机回复即证明控制通道活跃).
    // 之前的"CONTROL cmd=0x0002"是错误的 — 0x0002 是 DISCOVERY, 发出去会触发相机
    // 广播式 DISCOVERY_ACK, 污染控制通道且不维持心跳.
    uint32_t v = 0;
    quint16 pid = nextPacketId(m_lastPacketId);
    QByteArray pkt = buildGvcpHeader(GVCP_READREG_CMD, 0x0001, pid);
    append32(pkt, REG_DEVICE_MODE);   // 读设备模式寄存器作为心跳
    m_ctrlSocket->writeDatagram(pkt, m_deviceIp, m_devicePort);

    // 非阻塞读 (清除 ACK, 交由事件循环)
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
