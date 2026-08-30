/**
 * @file TeachWizard.h
 * @brief AI 教导向导 — 现场教导式缺陷检测 (阶段4落地核心)
 *
 * 用户流程 (与现场操作一一对应):
 *   1. 硬件固定好后, 选择工位 (上视/下视/侧视/45°斜视)
 *   2. 实时画面摆正工件、调好光 — 之后不动
 *   3. 把 OK/NG 样件摆上玻璃盘, 每转一盘点 [抓取此帧]
 *   4. 每张抓到的图一键标记 OK / NG
 *   5. 点 [开始训练] — 自动切分数据/调训练/导出模型
 *   6. 切到 [检测模式] — 实时画面实时判定 OK/NG
 *
 * 每个工位独立一套样本与模型: teach/<工位名>/
 */
#pragma once

#include "../hal/ICameraDriver.h"
#include "widgets/ImageViewWidget.h"
#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QProcess>
#include <QRadioButton>

namespace VisionInspector {

class TeachWizard : public QDialog {
    Q_OBJECT
public:
    explicit TeachWizard(ICameraDriver* camera, QWidget* parent = nullptr);

private slots:
    void onStationChanged(int index);
    void onLiveFrame(const CvImage& frame);
    void onCapture();
    void onMarkSample();            // 把选中样本标记为 OK/NG
    void onDeleteSample();
    void onStartTrain();
    void onTrainFinished(int exitCode, QProcess::ExitStatus status);
    void onToggleDetect(bool on);
    void onDetectTick();
    void refreshSampleList();

private:
    void setupUi();
    QString stationDir() const;      // teach/<工位>/
    QString stationModelPath() const;// models/<工位>.onnx
    void loadStation();
    void saveCaptured(const QImage& img, bool ok);
    void prepareDataset();           // 样本 → train/val 结构
    void setUiBusy(bool busy);

    ICameraDriver* m_camera = nullptr;
    QImage m_lastLiveFrame;          // 最近一帧实时画面

    QComboBox* m_stationCombo = nullptr;
    ImageViewWidget* m_liveView = nullptr;
    QListWidget* m_sampleList = nullptr;
    QLabel* m_sampleCountLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QRadioButton* m_okRadio = nullptr;
    QRadioButton* m_ngRadio = nullptr;
    QPushButton* m_detectBtn = nullptr;
    QPushButton* m_trainBtn = nullptr;
    QProcess* m_trainProc = nullptr;

    QTimer m_detectTimer;
    bool m_detecting = false;
    QString m_lastResult;
};

} // namespace VisionInspector
