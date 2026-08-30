/**
 * @file YoloTeachWizard.cpp
 * @brief YOLO 检测教导向导实现 — 实时检测框/样本采集
 */
#include "YoloTeachWizard.h"
#include "IconHelper.h"
#include "../ai/InferEngine.h"
#include "../utils/Logger.h"
#include "../core/ConfigManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QDir>
#include <QDateTime>
#include <QMessageBox>

#ifdef VI_HAS_OPENCV
#include <opencv2/imgcodecs.hpp>
#endif

namespace VisionInspector {

YoloTeachWizard::YoloTeachWizard(ICameraDriver* camera, QWidget* parent)
    : QDialog(parent), m_camera(camera)
{
    setupUi();
    onStationChanged(0);

    if (m_camera) {
        connect(m_camera, &ICameraDriver::imageReceived, this,
                [this](const CvImage& frame) {
#ifdef VI_HAS_OPENCV
                    if (!frame.empty()) {
                        m_lastLiveFrame = cvMatToQImage(frame);
                        if (!m_detecting)
                            m_liveView->setImage(m_lastLiveFrame);
                    }
#endif
                });
    }
}

void YoloTeachWizard::setupUi() {
    setWindowTitle(QStringLiteral("YOLO 检测教导向导"));
    setWindowIcon(IconHelper::categoryIcon(ToolCategory::Detection, 32));
    resize(1120, 720);
    auto* root = new QVBoxLayout(this);

    // ── 参数设置 ──
    auto* cfgGroup = new QGroupBox(QStringLiteral("检测参数"), this);
    auto* cfgForm = new QFormLayout(cfgGroup);

    m_stationCombo = new QComboBox(cfgGroup);
    m_stationCombo->addItems(ConfigManager::instance().stationNames());
    cfgForm->addRow(QStringLiteral("工位:"), m_stationCombo);

    auto* modelRow = new QHBoxLayout();
    m_modelEdit = new QLineEdit(QStringLiteral("models/yolov8n.onnx"), cfgGroup);
    m_modelEdit->setMinimumWidth(360);
    auto* browseBtn = new QPushButton(QStringLiteral("浏览..."), cfgGroup);
    modelRow->addWidget(m_modelEdit);
    modelRow->addWidget(browseBtn);
    cfgForm->addRow(QStringLiteral("模型:"), modelRow);

    m_confSpin = new QDoubleSpinBox(cfgGroup);
    m_confSpin->setRange(0.05, 0.95);
    m_confSpin->setSingleStep(0.05);
    m_confSpin->setValue(0.25);
    cfgForm->addRow(QStringLiteral("置信度阈值:"), m_confSpin);

    m_classesEdit = new QLineEdit(cfgGroup);
    m_classesEdit->setPlaceholderText(
        QStringLiteral("可选: 逗号分隔的类别名, 如 划痕,凹坑,裂纹 (留空则用模型自带元数据)"));
    cfgForm->addRow(QStringLiteral("类别名:"), m_classesEdit);

    root->addWidget(cfgGroup);

    // ── 实时画面 ──
    m_liveView = new ImageViewWidget();
    m_liveView->setMinimumSize(640, 420);
    root->addWidget(m_liveView, 1);

    // ── 状态 + 按钮 ──
    m_statusLabel = new QLabel(
        QStringLiteral("① 选模型  ② 设置信度  ③ [进入检测模式] 实时检测  ④ [抓取此帧] 存样本"), this);
    m_statusLabel->setStyleSheet("color:#8ab4ff;");
    root->addWidget(m_statusLabel);

    auto* btnRow = new QHBoxLayout();
    auto* capBtn = new QPushButton(QStringLiteral("📸 抓取此帧"), this);
    capBtn->setMinimumHeight(38);
    capBtn->setStyleSheet("font-size:14px; font-weight:bold;");
    m_detectBtn = new QPushButton(QStringLiteral("🔍 进入检测模式"), this);
    m_detectBtn->setMinimumHeight(38);
    m_detectBtn->setCheckable(true);
    btnRow->addWidget(capBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_detectBtn);
    root->addLayout(btnRow);

    connect(m_stationCombo, &QComboBox::currentIndexChanged,
            this, &YoloTeachWizard::onStationChanged);
    connect(browseBtn, &QPushButton::clicked, this, &YoloTeachWizard::onBrowseModel);
    connect(capBtn, &QPushButton::clicked, this, &YoloTeachWizard::onCapture);
    connect(m_detectBtn, &QPushButton::toggled, this, &YoloTeachWizard::onToggleDetect);
    connect(&m_detectTimer, &QTimer::timeout, this, &YoloTeachWizard::onDetectTick);
    m_detectTimer.setInterval(200);
}

QString YoloTeachWizard::stationDir() const {
    return QStringLiteral("teach/") + m_stationCombo->currentText();
}

QString YoloTeachWizard::modelPath() const {
    return m_modelEdit->text().trimmed();
}

void YoloTeachWizard::onStationChanged(int) {
    m_liveView->clearOverlays();
    m_statusLabel->setText(
        QStringLiteral("当前工位: %1").arg(m_stationCombo->currentText()));
}

void YoloTeachWizard::onBrowseModel() {
    const QString f = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 YOLO 检测模型"), QStringLiteral("models"),
        QStringLiteral("ONNX 模型 (*.onnx);;所有文件 (*.*)"));
    if (!f.isEmpty()) m_modelEdit->setText(f);
}

void YoloTeachWizard::onToggleDetect(bool on) {
    m_detecting = on;
    if (on) {
        if (m_lastLiveFrame.isNull()) {
            QMessageBox::information(this, QStringLiteral("检测"),
                QStringLiteral("还没有实时画面 — 请先连接相机并开始采集"));
            m_detectBtn->setChecked(false);
            m_detecting = false;
            return;
        }
        m_detectBtn->setText(QStringLiteral("⏹ 退出检测模式"));
        m_statusLabel->setText(QStringLiteral("检测模式运行中..."));
        m_detectTimer.start();
    } else {
        m_detectBtn->setText(QStringLiteral("🔍 进入检测模式"));
        m_detectTimer.stop();
        m_liveView->clearOverlays();
        if (!m_lastLiveFrame.isNull()) m_liveView->setImage(m_lastLiveFrame);
        m_statusLabel->setText(QStringLiteral("已退出检测模式"));
    }
}

void YoloTeachWizard::onDetectTick() {
#ifdef VI_HAS_ONNXRT
#ifdef VI_HAS_OPENCV
    if (m_lastLiveFrame.isNull()) return;

    QString err;
    auto engine = InferEngine::acquire(modelPath(), &err);
    if (!engine) {
        m_detectTimer.stop();
        m_detectBtn->setChecked(false);
        m_detecting = false;
        m_detectBtn->setText(QStringLiteral("🔍 进入检测模式"));
        m_statusLabel->setText(QStringLiteral("模型加载失败: %1").arg(err));
        return;
    }
    if (engine->outputKind() != InferEngine::OutputKind::Detection) {
        m_detectTimer.stop();
        m_detectBtn->setChecked(false);
        m_detecting = false;
        m_detectBtn->setText(QStringLiteral("🔍 进入检测模式"));
        m_statusLabel->setText(QStringLiteral("所选模型不是检测模型 (输出应为[1,4+nc,anchors])"));
        return;
    }

    const cv::Mat bgr = qImageToCvMat(m_lastLiveFrame);
    const float conf = (float)m_confSpin->value();
    const std::vector<AiDetection> dets = engine->detectYolo(bgr, conf, 0.45f, &err);
    if (!err.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("推理错误: %1").arg(err));
        return;
    }

    // 手填类别名优先
    QStringList manual = m_classesEdit->text().split(",", Qt::SkipEmptyParts);
    for (auto& s : manual) s = s.trimmed();

    QList<QVariant> overlays;
    for (const auto& d : dets) {
        QString name = (d.classId >= 0 && d.classId < manual.size())
                           ? manual[d.classId]
                           : engine->className(d.classId);

        QVariantMap rect;
        rect["type"] = "rect";
        rect["x"] = d.x; rect["y"] = d.y;
        rect["w"] = d.w; rect["h"] = d.h;
        rect["color"] = "#00e676";
        overlays.append(rect);

        QVariantMap label;
        label["type"] = "text";
        label["x"] = d.x; label["y"] = d.y - 6.0;
        label["size"] = 18.0;
        label["color"] = "#00e676";
        label["text"] = QStringLiteral("%1 %.0f%%").arg(name).arg(d.score * 100);
        overlays.append(label);
    }
    m_liveView->setImage(m_lastLiveFrame);
    m_liveView->setOverlays(overlays);
    m_statusLabel->setText(
        QStringLiteral("检测到 %1 个目标 (置信度≥%2)").arg(dets.size()).arg(conf));
#endif
#endif
}

void YoloTeachWizard::onCapture() {
    if (m_lastLiveFrame.isNull()) {
        QMessageBox::information(this, QStringLiteral("抓取"),
                                 QStringLiteral("没有实时画面 — 请先连接相机并开始采集"));
        return;
    }
    QDir dir(stationDir() + QStringLiteral("/detect_samples"));
    dir.mkpath(".");
    const QString file = dir.absoluteFilePath(
        QStringLiteral("det_%1.png").arg(QDateTime::currentMSecsSinceEpoch()));
    m_lastLiveFrame.save(file);
    Logger::instance().log(LogLevel::Info,
        QStringLiteral("YOLO教导抓帧[%1]: %2").arg(m_stationCombo->currentText(), file));
    m_statusLabel->setText(QStringLiteral("已保存样本: %1").arg(file));
}

} // namespace VisionInspector
