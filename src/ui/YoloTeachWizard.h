/**
 * @file YoloTeachWizard.h
 * @brief YOLO 检测教导向导 — 加载检测模型/实时检测框/采集样本 (把 AI检测·YOLOv8检测 接入 AI 菜单)
 *
 * 用户流程:
 *   1. 选一个 YOLO 检测模型 (.onnx, 默认 models/yolov8n.onnx)
 *   2. 设定置信度阈值 (可选手填类别名, 模型无元数据时用)
 *   3. [进入检测模式] — 实时画面实时画框 + 类别 + 分数
 *   4. [抓取此帧] — 把当前帧存到 teach/<工位>/detect_samples/ 供后续标注训练
 */
#pragma once

#include "../hal/ICameraDriver.h"
#include "widgets/ImageViewWidget.h"
#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QTimer>

namespace VisionInspector {

class YoloTeachWizard : public QDialog {
    Q_OBJECT
public:
    explicit YoloTeachWizard(ICameraDriver* camera, QWidget* parent = nullptr);

private slots:
    void onStationChanged(int index);
    void onBrowseModel();
    void onToggleDetect(bool on);
    void onDetectTick();
    void onCapture();

private:
    void setupUi();
    QString stationDir() const;
    QString modelPath() const;

    ICameraDriver* m_camera = nullptr;
    QImage m_lastLiveFrame;          // 最近一帧实时画面

    QComboBox* m_stationCombo = nullptr;
    QLineEdit* m_modelEdit = nullptr;
    QDoubleSpinBox* m_confSpin = nullptr;
    QLineEdit* m_classesEdit = nullptr;
    QComboBox* m_deviceCombo = nullptr;   // 运行设备: 自动/CPU/DirectML
    ImageViewWidget* m_liveView = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_detectBtn = nullptr;

    QTimer m_detectTimer;
    bool m_detecting = false;
};

} // namespace VisionInspector
