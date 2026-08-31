/**
 * @file GigECamera.h
 * @brief GigE Vision 通用相机驱动 — 独立实现, 不依赖厂商SDK
 *
 * 通过 GigE Vision 标准协议 (GVCP/GVSP) 直接与相机通信:
 * - GVCP: UDP 3956 端口, 设备发现/寄存器读写/流控制
 * - GVSP: UDP 随机端口, 图像数据流传输
 *
 * 兼容所有 GigE Vision 标准相机 (Basler acA, 海康 MV 系列, 度申GigE等)
 */
#pragma once
#include "ICameraDriver.h"
#include <QUdpSocket>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QByteArray>
#include <atomic>

namespace VisionInspector {

/**
 * GigE 设备信息 (GVCP DISCOVERY_ACK 结构)
 */
struct GigEDeviceInfo {
    QString manufacturer;
    QString model;
    QString serialNumber;
    QString deviceVersion;
    QString macAddress;
    QHostAddress ipAddress;
    QHostAddress deviceIp;
    uint32_t deviceId = 0;
    uint16_t devicePort = 3956;
};

/**
 * GigE Vision 通用相机驱动
 */
class GigECamera : public ICameraDriver {
    Q_OBJECT
    Q_INTERFACES(VisionInspector::ICameraDriver)
public:
    explicit GigECamera(QObject* parent = nullptr);
    ~GigECamera() override;

    QString driverName() const override { return QStringLiteral("GigE Vision"); }

    QList<CameraInfo> enumerateCameras() override;
    bool openCamera(const QString& cameraId) override;
    void closeCamera() override;
    bool isOpen() const override { return m_isOpen; }
    CameraInfo currentCamera() const override { return m_currentCamera; }
    CameraParams getParams() const override;
    bool setParams(const CameraParams& params) override;
    bool startAcquisition() override;
    bool stopAcquisition() override;
    bool isAcquiring() const override { return m_isAcquiring; }
    CvImage grabFrame(int timeoutMs = 3000) override;
    bool triggerOnce() override;
    QStringList supportedPixelFormats() const override;

private:
    // GVCP 操作
    bool gvspBindDataChannel();
    void sendDiscovery();
    bool readRegister(uint32_t address, uint32_t& value);
    bool writeRegister(uint32_t address, uint32_t value);
    bool writeRegisterString(uint32_t address, const QString& str, int maxLen);
    bool controlChannel(int32_t heartbeatMs = 3000);

    // GVSP 数据接收
    void processGvspPacket(const QByteArray& data);
    void resetGvspState();

    // GigE Vision 标准寄存器地址
    static constexpr uint32_t REG_DEVICE_MODE       = 0x00000000;
    static constexpr uint32_t REG_DEVICE_VENDOR     = 0x00000048;
    static constexpr uint32_t REG_DEVICE_MODEL      = 0x00000068;
    static constexpr uint32_t REG_DEVICE_SERIAL     = 0x000000A8;
    static constexpr uint32_t REG_DEVICE_VERSION    = 0x00000088;
    static constexpr uint32_t REG_WIDTH             = 0x00000100;
    static constexpr uint32_t REG_HEIGHT            = 0x00000104;
    static constexpr uint32_t REG_PIXEL_FORMAT      = 0x00000108;
    static constexpr uint32_t REG_ACQ_START         = 0x00000138;
    static constexpr uint32_t REG_ACQ_STOP          = 0x0000013C;
    static constexpr uint32_t REG_EXPOSURE_TIME     = 0x00000140;
    static constexpr uint32_t REG_GAIN              = 0x00000144;
    static constexpr uint32_t REG_TRIGGER_MODE      = 0x00000148;
    static constexpr uint32_t REG_TRIGGER_SOFTWARE  = 0x0000015C;
    static constexpr uint32_t REG_HEARTBEAT_TIMEOUT = 0x00000938;
    static constexpr uint32_t REG_STREAM_CHANNEL_PORT = 0x00000D00;
    static constexpr uint32_t REG_STREAM_CHANNEL_IP   = 0x00000D18;

    // GVCP 协议常量
    static constexpr uint16_t GVCP_DISCOVERY_CMD     = 0x0002;
    static constexpr uint16_t GVCP_DISCOVERY_ACK     = 0x0003;
    static constexpr uint16_t GVCP_READREG_CMD       = 0x0080;
    static constexpr uint16_t GVCP_READREG_ACK       = 0x0081;
    static constexpr uint16_t GVCP_WRITEREG_CMD      = 0x0082;
    static constexpr uint16_t GVCP_WRITEREG_ACK      = 0x0083;
    static constexpr uint16_t GVCP_PACKETID_HEARTBEAT = 0xFFFF;

    // GVSP 协议常量
    static constexpr uint8_t GVSP_HEADER_LEADER  = 0x01;
    static constexpr uint8_t GVSP_HEADER_TRAILER = 0x02;
    static constexpr uint8_t GVSP_HEADER_DATA    = 0x03;

    // 网络
    QUdpSocket* m_ctrlSocket = nullptr;
    QUdpSocket* m_dataSocket = nullptr;
    QHostAddress m_deviceIp;
    uint16_t m_devicePort = 3956;
    uint16_t m_localDataPort = 0;
    uint16_t m_lastPacketId = 0;

    // 状态
    bool m_isOpen = false;
    bool m_isAcquiring = false;
    CameraInfo m_currentCamera;
    GigEDeviceInfo m_deviceInfo;

    // GVSP 图像接收缓冲
    QMutex m_frameMutex;
    QByteArray m_frameBuffer;
    int m_expectedSize = 0;
    int m_packetsReceived = 0;
    int m_lastBlockId = -1;
    bool m_leaderReceived = false;
    bool m_frameComplete = false;
    CvImage m_lastFrame;

    // 心跳
    QTimer* m_heartbeatTimer = nullptr;

    // 采集线程
    QThread* m_grabThread = nullptr;
    QTimer* m_grabTimer = nullptr;

    // 图像参数缓存
    int m_imageWidth = 0;
    int m_imageHeight = 0;
    int m_pixelFormat = 0x01080001; // Mono8 default

private slots:
    void onHeartbeat();
    void onDataReady();
    void onGrabFrame();
    void onStartGrabThread();
    void onStopGrabThread();
};

} // namespace VisionInspector
