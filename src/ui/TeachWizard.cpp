/**
 * @file TeachWizard.cpp
 * @brief AI 教导向导实现 — 现场抓帧/标记/训练/实时检测
 */
#include "TeachWizard.h"
#include "../ai/InferEngine.h"
#include "../utils/Logger.h"
#include "../core/ConfigManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QScrollArea>
#include <QSplitter>
#include <QCoreApplication>
#include <QListWidget>
#include <QGridLayout>
#include <QGroupBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QMessageBox>

#ifdef VI_HAS_OPENCV
#include <opencv2/imgcodecs.hpp>
#endif

namespace VisionInspector {

TeachWizard::TeachWizard(ICameraDriver* camera, QWidget* parent)
    : QDialog(parent), m_camera(camera)
{
    setupUi();
    onStationChanged(0);

    // 实时画面: 订阅相机帧信号
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

void TeachWizard::setupUi() {
    setWindowTitle(QStringLiteral("AI 教导向导 — 现场教导缺陷检测"));
    resize(1180, 720);
    auto* root = new QVBoxLayout(this);

    // ── 工位选择 ──
    auto* topBar = new QHBoxLayout();
    topBar->addWidget(new QLabel(QStringLiteral("工位:"), this));
    m_stationCombo = new QComboBox(this);
    m_stationCombo->addItems(ConfigManager::instance().stationNames());
    m_stationCombo->setMinimumWidth(120);
    topBar->addWidget(m_stationCombo);
    topBar->addStretch();
    m_statusLabel = new QLabel(QStringLiteral("① 连接相机并摆正工件  ② 抓取样帧  ③ 标记OK/NG  ④ 训练  ⑤ 检测"), this);
    m_statusLabel->setStyleSheet("color:#8ab4ff;");
    topBar->addWidget(m_statusLabel, 1);
    root->addLayout(topBar);
    connect(m_stationCombo, &QComboBox::currentIndexChanged,
            this, &TeachWizard::onStationChanged);

    // ── 中部: 实时画面 | 样本库 ──
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    m_liveView = new ImageViewWidget();
    m_liveView->setMinimumSize(560, 420);
    splitter->addWidget(m_liveView);

    auto* samplePanel = new QWidget(this);
    auto* sampleLayout = new QVBoxLayout(samplePanel);
    sampleLayout->addWidget(new QLabel(QStringLiteral("已抓取样帧 (点击选中后标记):"), samplePanel));
    m_sampleList = new QListWidget(samplePanel);
    m_sampleList->setViewMode(QListView::IconMode);
    m_sampleList->setIconSize(QSize(96, 72));
    m_sampleList->setUniformItemSizes(true);
    m_sampleList->setMinimumWidth(300);
    sampleLayout->addWidget(m_sampleList, 1);

    auto* markRow = new QHBoxLayout();
    m_okRadio = new QRadioButton(QStringLiteral("OK 良品"), samplePanel);
    m_ngRadio = new QRadioButton(QStringLiteral("NG 缺陷"), samplePanel);
    m_okRadio->setChecked(true);
    markRow->addWidget(m_okRadio);
    markRow->addWidget(m_ngRadio);
    auto* markBtn = new QPushButton(QStringLiteral("标记选中样本"), samplePanel);
    auto* delBtn = new QPushButton(QStringLiteral("删除选中"), samplePanel);
    markRow->addWidget(markBtn);
    markRow->addWidget(delBtn);
    sampleLayout->addLayout(markRow);

    m_sampleCountLabel = new QLabel(samplePanel);
    sampleLayout->addWidget(m_sampleCountLabel);
    splitter->addWidget(samplePanel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);

    // ── 底部: 抓取/训练/检测 ──
    auto* btnRow = new QHBoxLayout();
    auto* capBtn = new QPushButton(QStringLiteral("📸 抓取此帧"), this);
    capBtn->setMinimumHeight(38);
    capBtn->setStyleSheet("font-size:14px; font-weight:bold;");
    m_trainBtn = new QPushButton(QStringLiteral("🎓 开始训练"), this);
    m_trainBtn->setMinimumHeight(38);
    m_detectBtn = new QPushButton(QStringLiteral("🔍 进入检测模式"), this);
    m_detectBtn->setMinimumHeight(38);
    m_detectBtn->setCheckable(true);
    btnRow->addWidget(capBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_trainBtn);
    btnRow->addWidget(m_detectBtn);
    root->addLayout(btnRow);

    connect(capBtn, &QPushButton::clicked, this, &TeachWizard::onCapture);
    connect(markBtn, &QPushButton::clicked, this, &TeachWizard::onMarkSample);
    connect(delBtn, &QPushButton::clicked, this, &TeachWizard::onDeleteSample);
    connect(m_trainBtn, &QPushButton::clicked, this, &TeachWizard::onStartTrain);
    connect(m_detectBtn, &QPushButton::toggled, this, &TeachWizard::onToggleDetect);
    connect(&m_detectTimer, &QTimer::timeout, this, &TeachWizard::onDetectTick);
    m_detectTimer.setInterval(400);
}

QString TeachWizard::stationDir() const {
    return QStringLiteral("teach/") + m_stationCombo->currentText();
}

QString TeachWizard::stationModelPath() const {
    return QStringLiteral("models/") + m_stationCombo->currentText() + QStringLiteral(".onnx");
}

void TeachWizard::onStationChanged(int) {
    m_liveView->setAnnotations({});
    refreshSampleList();
    // 若该工位已有模型, 提示可检测
    if (QFile::exists(stationModelPath()))
        m_statusLabel->setText(QStringLiteral("工位[%1]已有教导模型, 可直接进入检测模式")
                                   .arg(m_stationCombo->currentText()));
}

void TeachWizard::onLiveFrame(const CvImage&) {}   // 帧处理已在ctor的lambda里

void TeachWizard::onCapture() {
#ifdef VI_HAS_OPENCV
    if (m_lastLiveFrame.isNull()) {
        QMessageBox::information(this, QStringLiteral("抓取"),
                                 QStringLiteral("没有实时画面 — 请先连接相机并开始采集"));
        return;
    }
    const bool asOk = m_okRadio->isChecked();
    QDir dir(stationDir() + (asOk ? "/OK" : "/NG"));
    dir.mkpath(".");
    const QString file = dir.absoluteFilePath(
        QStringLiteral("%1_%2.png")
            .arg(asOk ? "OK" : "NG")
            .arg(QDateTime::currentMSecsSinceEpoch()));
    m_lastLiveFrame.save(file);
    Logger::instance().log(LogLevel::Info,
        QStringLiteral("教导抓帧[%1]: %2").arg(m_stationCombo->currentText(), file));
    refreshSampleList();
    m_statusLabel->setText(QStringLiteral("已抓取 → 当前标记: %1  (共%2张)")
        .arg(asOk ? "OK" : "NG").arg(m_sampleList->count()));
#endif
}

void TeachWizard::refreshSampleList() {
    m_sampleList->clear();
    int okCnt = 0, ngCnt = 0;
    for (const QString& sub : {QStringLiteral("OK"), QStringLiteral("NG")}) {
        QDir dir(stationDir() + "/" + sub);
        for (const QFileInfo& fi : dir.entryInfoList({"*.jpg", "*.png"}, QDir::Files,
                                                     QDir::Time)) {
            auto* item = new QListWidgetItem(
                QIcon(fi.absoluteFilePath()),
                QStringLiteral("%1  %2").arg(sub, fi.fileName().left(14)));
            item->setData(Qt::UserRole, fi.absoluteFilePath());
            m_sampleList->addItem(item);
            if (sub == QStringLiteral("OK")) ++okCnt; else ++ngCnt;
        }
    }
    m_sampleCountLabel->setText(QStringLiteral("OK: %1 张   NG: %2 张")
                                    .arg(okCnt).arg(ngCnt));
}

void TeachWizard::onMarkSample() {
    auto* item = m_sampleList->currentItem();
    if (!item) return;
    const QString oldPath = item->data(Qt::UserRole).toString();
    const bool toOk = m_okRadio->isChecked();
    const QString sub = toOk ? QStringLiteral("OK") : QStringLiteral("NG");
    QDir dir(stationDir() + "/" + sub);
    dir.mkpath(".");
    const QString newPath = dir.absoluteFilePath(QFileInfo(oldPath).fileName());
    if (oldPath != newPath) {
        QFile::rename(oldPath, newPath);
        item->setData(Qt::UserRole, newPath);
        item->setText(QStringLiteral("%1  %2").arg(
            sub, QFileInfo(newPath).fileName().left(14)));
        QIcon icon(newPath);
        item->setIcon(icon);
    }
}

void TeachWizard::onDeleteSample() {
    auto* item = m_sampleList->currentItem();
    if (!item) return;
    QFile::remove(item->data(Qt::UserRole).toString());
    delete item;
    refreshSampleList();
}

void TeachWizard::prepareDataset() {
    // 样本 → train(80%) + val(20%), 类别名: 良品 / 缺陷
    const QString dataDir = stationDir() + "/train_data";
    for (const QString& split : {QStringLiteral("train"), QStringLiteral("val")})
        for (const QString& cls : {QStringLiteral("良品"), QStringLiteral("缺陷")})
            QDir(dataDir + "/" + split + "/" + cls)
                .removeRecursively();  // 清旧数据

    for (const QString& sub : {QStringLiteral("OK"), QStringLiteral("NG")}) {
        const QString cls = (sub == QStringLiteral("OK")) ? QStringLiteral("良品")
                                                          : QStringLiteral("缺陷");
        QDir dir(stationDir() + "/" + sub);
        const auto files = dir.entryInfoList({"*.jpg", "*.png"}, QDir::Files, QDir::Time);
        int i = 0;
        for (const QFileInfo& fi : files) {
            const QString split = (i % 5 == 4) ? QStringLiteral("val")
                                               : QStringLiteral("train");  // 80/20
            QDir(dataDir + "/" + split + "/" + cls).mkpath(".");
            QFile::copy(fi.absoluteFilePath(),
                        dataDir + "/" + split + "/" + cls + "/" + fi.fileName());
            ++i;
        }
    }
    // classes.txt
    QFile cf(dataDir + "/classes.txt");
    if (cf.open(QIODevice::WriteOnly)) {
        cf.write("良品\n缺陷\n");
        cf.close();
    }
}

void TeachWizard::onStartTrain() {
    prepareDataset();
    const QString dataDir = stationDir() + "/train_data";

    // 样本量检查
    int nOK = 0, nNG = 0;
    for (const QFileInfo& fi : QDir(stationDir() + "/OK").entryInfoList({"*.jpg", "*.png"})) nOK++;
    for (const QFileInfo& fi : QDir(stationDir() + "/NG").entryInfoList({"*.jpg", "*.png"})) nNG++;
    if (nOK < 3 || nNG < 3) {
        QMessageBox::warning(this, QStringLiteral("样本不足"),
            QStringLiteral("每类至少3张 (当前 OK:%1 NG:%2)\n"
                           "建议每类8张以上, 多角度多位置抓取").arg(nOK).arg(nNG));
        return;
    }

    // 训练脚本 (tools/train_cls.py)
    const QString script = QCoreApplication::applicationDirPath()
                           + "/../../../tools/train_cls.py";
    const QString outModel = stationModelPath();

    setUiBusy(true);
    m_statusLabel->setText(QStringLiteral("训练中... (CPU约3-10分钟, 样本越多越久)"));
    Logger::instance().log(LogLevel::Info,
        QStringLiteral("教导训练[%1]开始: %2张样本").arg(m_stationCombo->currentText()).arg(nOK + nNG));

    m_trainProc = new QProcess(this);
    m_trainProc->setWorkingDirectory(QCoreApplication::applicationDirPath());
    connect(m_trainProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        setUiBusy(false);
        m_statusLabel->setText(QStringLiteral("训练进程启动失败 — 请确认机器已安装 Python+ultralytics"));
        QMessageBox::warning(this, QStringLiteral("训练"),
            QStringLiteral("无法启动Python训练进程。\n"
                           "教导训练需要安装 Python + ultralytics (pip install ultralytics)\n"
                           "训练也可在开发机上完成后将onnx放入models\\目录"));
    });
    connect(m_trainProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TeachWizard::onTrainFinished);
    // 训练进度实时回显 (ultralytics 的逐轮日志 → 状态栏+日志面板)
    connect(m_trainProc, &QProcess::readyReadStandardOutput, this, [this]() {
        const QString out = QString::fromLocal8Bit(m_trainProc->readAllStandardOutput());
        const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString& l : lines) {
            const QString t = l.trimmed();
            if (!t.isEmpty()) {
                m_statusLabel->setText(QStringLiteral("训练中: %1").arg(t.left(80)));
                Logger::instance().log(LogLevel::Info, QStringLiteral("[教导训练] %1").arg(t.left(120)));
            }
        }
    });
    connect(m_trainProc, &QProcess::readyReadStandardError, this, [this]() {
        const QString err = QString::fromLocal8Bit(m_trainProc->readAllStandardError());
        for (const QString& l : err.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const QString t = l.trimmed();
            if (t.contains("Epoch") || t.contains('%'))
                m_statusLabel->setText(QStringLiteral("训练中: %1").arg(t.left(80)));
        }
    });
    m_trainProc->start(QStringLiteral("python"),
        {QCoreApplication::applicationDirPath() + "/../../../tools/train_cls.py",
         QDir::toNativeSeparators(dataDir), QDir::toNativeSeparators(outModel),
         QStringLiteral("60"), QStringLiteral("cpu")});
}

void TeachWizard::onTrainFinished(int exitCode, QProcess::ExitStatus) {
    setUiBusy(false);
    if (exitCode == 0) {
        // classes.txt 随模型部署 (AIDetection 类名显示依赖它)
        const QString clsSrc = stationDir() + "/train_data/classes.txt";
        const QString clsDst = QStringLiteral("models/") +
            QFileInfo(stationModelPath()).completeBaseName() + "_classes.txt";
        if (QFile::exists(clsSrc))
            QFile::copy(clsSrc, clsDst);
        m_statusLabel->setText(QStringLiteral("✅ 训练完成! 模型: %1  可进入检测模式")
                                   .arg(stationModelPath()));

        // 评估图对话框: 混淆矩阵/归一化混淆矩阵/准确率曲线 翻页查看
        const QString modelBase = QFileInfo(stationModelPath()).completeBaseName();
        const QString modelDir = QFileInfo(stationModelPath()).absolutePath();
        const QStringList chartFiles = {
            modelDir + "/" + modelBase + "_confusion_matrix.png",
            modelDir + "/" + modelBase + "_confusion_matrix_normalized.png",
            modelDir + "/" + modelBase + "_results.png" };
        const QStringList chartTitles = { QStringLiteral("混淆矩阵"),
            QStringLiteral("归一化混淆矩阵"), QStringLiteral("训练曲线") };
        QStringList foundCharts, foundTitles;
        for (int i = 0; i < chartFiles.size(); ++i)
            if (QFile::exists(chartFiles[i])) { foundCharts << chartFiles[i]; foundTitles << chartTitles[i]; }

        QString chartNote;
        if (!foundCharts.isEmpty())
            chartNote = QStringLiteral("\n训练评估图已生成, 将在弹出的窗口中展示。");
        QMessageBox::information(this, QStringLiteral("训练完成"),
            QStringLiteral("模型已保存到 %1\n现在点 [进入检测模式] 即可实时判定%2")
                .arg(stationModelPath(), chartNote));
        Logger::instance().log(LogLevel::Info,
            QStringLiteral("教导训练[%1]完成").arg(m_stationCombo->currentText()));

        // 展示评估图 (Tab翻页)
        if (!foundCharts.isEmpty()) {
            auto* viewer = new QDialog(this);
            viewer->setWindowTitle(QStringLiteral("训练评估 - %1").arg(m_stationCombo->currentText()));
            viewer->setAttribute(Qt::WA_DeleteOnClose);
            viewer->resize(900, 640);
            auto* vlay = new QVBoxLayout(viewer);
            auto* tabs = new QTabWidget(viewer);
            for (int i = 0; i < foundCharts.size(); ++i) {
                auto* lbl = new QLabel;
                lbl->setPixmap(QPixmap(foundCharts[i]).scaled(
                    860, 560, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                lbl->setAlignment(Qt::AlignCenter);
                auto* scroll = new QScrollArea(viewer);
                scroll->setWidget(lbl);
                tabs->addTab(scroll, foundTitles[i]);
            }
            vlay->addWidget(tabs);
            viewer->show();
        }
    } else {
        m_statusLabel->setText(QStringLiteral("训练失败 (退出码%1), 详见日志").arg(exitCode));
        Logger::instance().log(LogLevel::Error,
            QStringLiteral("教导训练失败 exit=%1").arg(exitCode));
    }
}

void TeachWizard::setUiBusy(bool busy) {
    m_trainBtn->setEnabled(!busy);
    m_detectBtn->setEnabled(!busy);
    m_stationCombo->setEnabled(!busy);
}

void TeachWizard::onToggleDetect(bool on) {
    m_detecting = on;
    if (on) {
        const QString model = stationModelPath();
        if (!QFile::exists(model)) {
            QMessageBox::warning(this, QStringLiteral("检测"),
                QStringLiteral("该工位还没有模型 — 请先完成训练"));
            m_detectBtn->setChecked(false);
            return;
        }
        QString err;
        auto eng = InferEngine::acquire(QCoreApplication::applicationDirPath()
                                        + "/" + model, &err);
        if (!eng || eng->outputKind() != InferEngine::OutputKind::Classification) {
            QMessageBox::warning(this, QStringLiteral("检测"),
                QStringLiteral("模型加载失败: %1").arg(err));
            m_detectBtn->setChecked(false);
            return;
        }
        if (m_camera && !m_camera->isAcquiring())
            m_camera->startAcquisition();
        m_detectTimer.start();
        m_statusLabel->setText(QStringLiteral("检测模式: 实时判定中..."));
        m_detectBtn->setText(QStringLiteral("⏹ 停止检测"));
    } else {
        m_detectTimer.stop();
        m_statusLabel->setText(QStringLiteral("已退出检测模式"));
        m_detectBtn->setText(QStringLiteral("🔍 进入检测模式"));
    }
}

void TeachWizard::onDetectTick() {
    if (!m_camera || !m_detecting) return;
#ifdef VI_HAS_OPENCV
    const CvImage frame = m_camera->grabFrame(150);
    if (frame.empty()) return;
    QImage qimg = cvMatToQImage(frame);
    m_liveView->setImage(qimg);

    QString err;
    auto eng = InferEngine::acquire(QCoreApplication::applicationDirPath()
                                    + "/" + stationModelPath(), &err);
    if (!eng) return;
    QList<QPair<QString, float>> results;
    if (!eng->classify(frame, results, &err) || results.isEmpty()) return;

    const QString top = results[0].first;
    const float score = results[0].second;
    const bool isOK = top.contains(QStringLiteral("良品"));
    m_lastResult = QStringLiteral("%1  %2%").arg(top).arg((int)(score * 100));

    // 结果叠加
    QList<QVariant> overlays;
    QVariantMap banner;
    banner["type"] = "text";
    banner["x"] = 10.0; banner["y"] = 30.0; banner["size"] = 26.0;
    banner["text"] = (isOK ? QStringLiteral("✅ OK 良品  %1%").arg((int)(score*100))
                           : QStringLiteral("❌ NG 缺陷[%1] %2%").arg(top).arg((int)(score*100)));
    banner["color"] = isOK ? "#00ff00" : "#ff3030";
    overlays.append(banner);
    m_liveView->setOverlays(overlays);
    m_statusLabel->setText(QStringLiteral("检测: %1").arg(m_lastResult));
#endif
}

} // namespace VisionInspector
