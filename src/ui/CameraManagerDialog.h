/**
 * @file CameraManagerDialog.h
 * @brief 相机管理对话框 — CCD1~CCD8 多相机手动配置与连接
 */
#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QGridLayout>
#include <QVector>
#include "../hal/ICameraDriver.h"
#include "../hal/GigECamera.h"

namespace VisionInspector {

/**
 * 单个相机位配置
 */
struct CameraSlotConfig {
    QString name;          // 别名 (如 "CCD1-正面检测")
    QString ipAddress;     // 相机 IP
    QString serialNumber;  // 芯片编号 (只读, 连接后自动读取)
    bool connected = false;
};

class CameraManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit CameraManagerDialog(QWidget* parent = nullptr);
    ~CameraManagerDialog() override;

    /// 获取当前连接的相机驱动 (CCD1)
    ICameraDriver* currentCamera() const { return m_cameras.isEmpty() ? nullptr : m_cameras[0]; }

    /// 获取指定槽位的相机驱动
    ICameraDriver* cameraAt(int index) const;

    /// 所有槽位配置
    QVector<CameraSlotConfig> slotConfigs() const;

signals:
    /// 相机连接状态变化 (index, connected, model)
    void cameraStatusChanged(int index, bool connected, const QString& info);

private slots:
    void onScanNetwork();       // 扫描网络上的 GigE 相机
    void onConnectClicked(int index);
    void onDisconnectClicked(int index);
    void onTestGrab(int index);

private:
    void setupUI();
    void updateStatus(int index);

    static constexpr int MAX_CAMERAS = 8;

    struct SlotWidgets {
        QLabel* nameLabel = nullptr;
        QLineEdit* nameEdit = nullptr;
        QLineEdit* ipEdit = nullptr;
        QLabel* serialLabel = nullptr;
        QLabel* statusLabel = nullptr;
        QPushButton* connectBtn = nullptr;
        QPushButton* disconnectBtn = nullptr;
        QPushButton* testBtn = nullptr;
    };

    QVector<SlotWidgets> m_slotWidgets;
    QVector<ICameraDriver*> m_cameras;
    QVector<CameraSlotConfig> m_configs;
};

} // namespace VisionInspector
