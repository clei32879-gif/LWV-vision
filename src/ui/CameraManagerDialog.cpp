/**
 * @file CameraManagerDialog.cpp
 * @brief 相机管理对话框实现 — CCD1~CCD8 手动IP配置 + 连接/断开/测试
 */
#include "CameraManagerDialog.h"
#include "../hal/GigECamera.h"
#include <QMessageBox>
#include <QHeaderView>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTableWidget>
#include <QTimer>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QInputDialog>
#include <QApplication>
#include <QSettings>
#include <QDoubleSpinBox>
#include <QSpinBox>

namespace VisionInspector {

CameraManagerDialog::CameraManagerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("相机管理 (CCD1~CCD8)");
    setMinimumSize(700, 520);
    resize(780, 600);
    setupUI();
}

CameraManagerDialog::~CameraManagerDialog()
{
    for (auto* cam : m_cameras) {
        if (cam) { cam->closeCamera(); delete cam; }
    }
}

ICameraDriver* CameraManagerDialog::cameraAt(int index) const
{
    if (index < 0 || index >= m_cameras.size()) return nullptr;
    return m_cameras[index];
}

QMap<QString, ICameraDriver*> CameraManagerDialog::connectedCameras() const
{
    QMap<QString, ICameraDriver*> result;
    for (int i = 0; i < m_cameras.size() && i < m_configs.size(); ++i) {
        if (m_cameras[i] && m_cameras[i]->isOpen()) {
            // 别名 = 名称框内容; 为空时用默认 "CCD{n}"
            QString alias = m_configs[i].name.trimmed();
            if (alias.isEmpty()) alias = QString("CCD%1").arg(i + 1);
            result[alias] = m_cameras[i];
        }
    }
    return result;
}

QVector<CameraSlotConfig> CameraManagerDialog::slotConfigs() const
{
    return m_configs;
}

void CameraManagerDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // 顶部: 扫描按钮 + 本机IP提示
    auto* topRow = new QHBoxLayout;
    auto* scanBtn = new QPushButton("扫描网络相机");
    connect(scanBtn, &QPushButton::clicked, this, &CameraManagerDialog::onScanNetwork);
    topRow->addWidget(scanBtn);

    // 本机 IP 提示
    QString localIps;
    for (const auto& iface : QNetworkInterface::allInterfaces()) {
        for (const auto& entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !entry.ip().isLoopback()) {
                if (!localIps.isEmpty()) localIps += ", ";
                localIps += entry.ip().toString();
            }
        }
    }
    topRow->addWidget(new QLabel(QString("本机IP: %1 (相机需在同一网段)").arg(localIps.isEmpty() ? "未检测" : localIps)));
    topRow->addStretch();
    mainLayout->addLayout(topRow);

    // CCD1~CCD8 表格 (启用列: 标配N台也可只启用其中几台, 禁用=断开且不参与检测)
    auto* table = new QTableWidget(MAX_CAMERAS, 9, this);
    table->setHorizontalHeaderLabels({"槽位", "启用", "别名", "相机IP", "芯片编号", "状态", "操作", "参数", "测试"});
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_slotWidgets.resize(MAX_CAMERAS);
    m_cameras.resize(MAX_CAMERAS, nullptr);
    m_configs.resize(MAX_CAMERAS);

    // 默认别名
    QStringList defaultNames;
    for (int i = 0; i < MAX_CAMERAS; i++) {
        defaultNames << QString("CCD%1").arg(i + 1);
        m_configs[i].name = defaultNames[i];
    }
    loadStationSettings();

    for (int i = 0; i < MAX_CAMERAS; i++) {
        auto& w = m_slotWidgets[i];
        int row = i;

        // 0: 槽位
        table->setCellWidget(row, 0, new QLabel(QString("CCD%1").arg(i + 1)));

        // 1: 工位启用
        w.enableCheck = new QCheckBox;
        w.enableCheck->setChecked(m_configs[i].enabled);
        w.enableCheck->setToolTip(QStringLiteral(
            "工位启用: 取消勾选后该工位断开且不参与检测 (如标配4台只用1/2/4号, 禁用3号)"));
        connect(w.enableCheck, &QCheckBox::toggled, this, [this, i](bool on) {
            onEnableToggled(i, on);
        });
        table->setCellWidget(row, 1, w.enableCheck);

        // 2: 别名
        w.nameEdit = new QLineEdit(defaultNames[i]);
        connect(w.nameEdit, &QLineEdit::textChanged, this, [this, i](const QString& text) {
            m_configs[i].name = text;
        });
        table->setCellWidget(row, 2, w.nameEdit);

        // 3: IP
        w.ipEdit = new QLineEdit;
        w.ipEdit->setPlaceholderText("192.168.1.100");
        table->setCellWidget(row, 3, w.ipEdit);

        // 4: 芯片编号
        w.serialLabel = new QLabel("-");
        table->setCellWidget(row, 4, w.serialLabel);

        // 5: 状态
        w.statusLabel = new QLabel("未连接");
        w.statusLabel->setStyleSheet("color: gray;");
        table->setCellWidget(row, 5, w.statusLabel);

        // 6: 连接/断开
        auto* btnWidget = new QWidget;
        auto* btnLayout = new QHBoxLayout(btnWidget);
        btnLayout->setContentsMargins(2, 2, 2, 2);
        btnLayout->setSpacing(4);
        w.connectBtn = new QPushButton("连接");
        w.disconnectBtn = new QPushButton("断开");
        w.disconnectBtn->setEnabled(false);
        btnLayout->addWidget(w.connectBtn);
        btnLayout->addWidget(w.disconnectBtn);
        table->setCellWidget(row, 6, btnWidget);

        // 7: 相机参数
        w.paramsBtn = new QPushButton("参数");
        w.paramsBtn->setEnabled(false);
        w.paramsBtn->setToolTip(QStringLiteral("曝光/增益/分辨率/触发模式/像素格式"));
        table->setCellWidget(row, 7, w.paramsBtn);

        // 8: 测试拍照
        w.testBtn = new QPushButton("拍照");
        w.testBtn->setEnabled(false);
        table->setCellWidget(row, 8, w.testBtn);

        // 信号连接
        int idx = i;  // 捕获用
        connect(w.connectBtn, &QPushButton::clicked, this, [this, idx]() { onConnectClicked(idx); });
        connect(w.disconnectBtn, &QPushButton::clicked, this, [this, idx]() { onDisconnectClicked(idx); });
        connect(w.testBtn, &QPushButton::clicked, this, [this, idx]() { onTestGrab(idx); });
        connect(w.paramsBtn, &QPushButton::clicked, this, [this, idx]() { onParamsClicked(idx); });

        // 禁用状态下的控件初始可见性
        if (!m_configs[i].enabled) {
            w.ipEdit->setEnabled(false);
            w.connectBtn->setEnabled(false);
        }
    }

    mainLayout->addWidget(table);

    // 底部: 确定/取消
    auto* bottomRow = new QHBoxLayout;
    bottomRow->addStretch();
    auto* okBtn = new QPushButton("确定");
    auto* cancelBtn = new QPushButton("取消");
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    bottomRow->addWidget(okBtn);
    bottomRow->addWidget(cancelBtn);
    mainLayout->addLayout(bottomRow);
}

void CameraManagerDialog::onConnectClicked(int index)
{
    if (index < 0 || index >= MAX_CAMERAS) return;
    auto& w = m_slotWidgets[index];
    QString ip = w.ipEdit->text().trimmed();

    if (ip.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入相机IP地址");
        return;
    }

    QHostAddress addr(ip);
    if (addr.isNull()) {
        QMessageBox::warning(this, "提示", "IP地址格式不正确");
        return;
    }

    // 创建相机驱动: GigE通用 (兼容所有GigE相机)
    if (!m_cameras[index]) {
        m_cameras[index] = new GigECamera();
    }

    auto* cam = m_cameras[index];

    // 如果已连接先断开
    if (cam->isOpen()) cam->closeCamera();

    w.statusLabel->setText("连接中...");
    w.statusLabel->setStyleSheet("color: orange;");
    w.connectBtn->setEnabled(false);
    repaint(); // 强制刷新UI

    // 连接
    bool ok = cam->openCamera(ip);
    if (ok) {
        CameraInfo info = cam->currentCamera();
        QString model = info.model.isEmpty() ? "GigE相机" : info.model;
        QString serial = info.serialNumber.isEmpty() ? "-" : info.serialNumber;

        w.statusLabel->setText(QString("已连接: %1").arg(model));
        w.statusLabel->setStyleSheet("color: green; font-weight: bold;");
        w.serialLabel->setText(serial);
        w.connectBtn->setEnabled(false);
        w.disconnectBtn->setEnabled(true);
        w.paramsBtn->setEnabled(true);
        w.testBtn->setEnabled(true);
        w.ipEdit->setReadOnly(true);

        m_configs[index].ipAddress = ip;
        m_configs[index].serialNumber = serial;
        m_configs[index].connected = true;

        // 更新别名 (如果还是默认值)
        QString currentName = w.nameEdit->text();
        if (currentName == QString("CCD%1").arg(index + 1)) {
            w.nameEdit->setText(QString("CCD%1-%2").arg(index + 1).arg(model));
        }

        emit cameraStatusChanged(index, true, model);
    } else {
        w.statusLabel->setText("连接失败");
        w.statusLabel->setStyleSheet("color: red;");
        w.connectBtn->setEnabled(true);

        // 从相机驱动获取详细错误信息
        QString detail;
        auto* gigeCam = qobject_cast<GigECamera*>(m_cameras[index]);
        if (gigeCam && !gigeCam->lastError().isEmpty()) {
            detail = "\n\n诊断: " + gigeCam->lastError();
        }

        QMessageBox::warning(this, "连接失败",
            QString("无法连接到 %1\n\n"
                    "请逐项检查:\n"
                    "1. 关闭 pylon Viewer 等其他相机软件 (GigE相机同时只能一个程序连接)\n"
                    "2. 相机已通电, 网线已插好\n"
                    "3. 网口和相机在同一网段 (如 169.254.4.4 ↔ 169.254.4.44)\n"
                    "4. Windows 防火墙已放行 UDP 3956%2").arg(ip, detail));
    }
}

void CameraManagerDialog::onDisconnectClicked(int index)
{
    if (index < 0 || index >= MAX_CAMERAS) return;
    auto& w = m_slotWidgets[index];

    if (m_cameras[index]) {
        m_cameras[index]->closeCamera();
    }

    w.statusLabel->setText("未连接");
    w.statusLabel->setStyleSheet("color: gray;");
    w.serialLabel->setText("-");
    w.connectBtn->setEnabled(true);
    w.disconnectBtn->setEnabled(false);
    w.paramsBtn->setEnabled(false);
    w.testBtn->setEnabled(false);
    w.ipEdit->setReadOnly(false);

    m_configs[index].connected = false;
    emit cameraStatusChanged(index, false, "");
}

// ============================================================
// 工位启用/禁用 (标配N台也可只启用其中几台)
// ============================================================

void CameraManagerDialog::onEnableToggled(int index, bool on)
{
    if (index < 0 || index >= MAX_CAMERAS) return;
    m_configs[index].enabled = on;
    saveStationSetting(index);

    auto& w = m_slotWidgets[index];
    if (!on) {
        // 禁用: 断开连接 → 检测链路查不到该相机, 天然不参与检测
        if (m_cameras[index] && m_cameras[index]->isOpen())
            onDisconnectClicked(index);
        w.ipEdit->setEnabled(false);
        w.connectBtn->setEnabled(false);
        w.statusLabel->setText("已禁用");
        w.statusLabel->setStyleSheet("color: #888;");
    } else {
        w.ipEdit->setEnabled(true);
        w.connectBtn->setEnabled(true);
        w.statusLabel->setText("未连接");
        w.statusLabel->setStyleSheet("color: gray;");
    }
}

void CameraManagerDialog::loadStationSettings()
{
    QSettings s("VisionInspector", "VisionInspector");
    for (int i = 0; i < MAX_CAMERAS; ++i)
        m_configs[i].enabled = s.value(QStringLiteral("stationEnabled/%1").arg(i), true).toBool();
}

void CameraManagerDialog::saveStationSetting(int index) const
{
    QSettings s("VisionInspector", "VisionInspector");
    s.setValue(QStringLiteral("stationEnabled/%1").arg(index), m_configs[index].enabled);
}

// ============================================================
// 相机参数编辑 (曝光/增益/分辨率/触发模式/像素格式)
// ============================================================

void CameraManagerDialog::onParamsClicked(int index)
{
    if (index < 0 || index >= MAX_CAMERAS) return;
    auto* cam = m_cameras[index];
    if (!cam || !cam->isOpen()) return;
    auto& w = m_slotWidgets[index];

    CameraParams p = cam->getParams();

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("相机参数 - %1").arg(w.nameEdit->text()));
    auto* form = new QFormLayout(&dlg);

    auto* exposure = new QDoubleSpinBox(&dlg);
    exposure->setRange(1.0, 1000000.0);
    exposure->setDecimals(0);
    exposure->setSuffix(QStringLiteral(" μs"));
    exposure->setValue(p.exposureTime > 0 ? p.exposureTime : 5000.0);
    form->addRow(QStringLiteral("曝光时间:"), exposure);

    auto* gain = new QDoubleSpinBox(&dlg);
    gain->setRange(0.0, 48.0);
    gain->setDecimals(1);
    gain->setSuffix(QStringLiteral(" dB"));
    gain->setValue(p.gain >= 0 ? p.gain : 0.0);
    form->addRow(QStringLiteral("增益:"), gain);

    auto* width = new QSpinBox(&dlg);
    width->setRange(1, 24576);
    width->setValue(p.width);
    auto* height = new QSpinBox(&dlg);
    height->setRange(1, 20480);
    height->setValue(p.height);
    auto* roi = new QHBoxLayout;
    roi->addWidget(width);
    roi->addWidget(new QLabel("×", &dlg));
    roi->addWidget(height);
    roi->addStretch();
    form->addRow(QStringLiteral("分辨率:"), roi);

    auto* trigger = new QComboBox(&dlg);
    trigger->addItem(QStringLiteral("连续采集 (内部自由触发)"), 0);
    trigger->addItem(QStringLiteral("外部触发 (硬触发/软触发)"), 1);
    trigger->setCurrentIndex(p.triggerMode ? 1 : 0);
    form->addRow(QStringLiteral("触发模式:"), trigger);

    auto* pixel = new QComboBox(&dlg);
    pixel->addItems(cam->supportedPixelFormats());
    if (!p.pixelFormat.isEmpty()) {
        const int idx = pixel->findText(p.pixelFormat);
        if (idx >= 0) pixel->setCurrentIndex(idx);
    }
    form->addRow(QStringLiteral("像素格式:"), pixel);

    auto* btnRow = new QHBoxLayout;
    auto* refreshBtn = new QPushButton(QStringLiteral("从相机回读"), &dlg);
    auto* applyBtn = new QPushButton(QStringLiteral("应用"), &dlg);
    auto* closeBtn = new QPushButton(QStringLiteral("关闭"), &dlg);
    btnRow->addWidget(refreshBtn);
    btnRow->addWidget(applyBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    form->addRow(btnRow);

    auto apply = [&]() -> QString {
        CameraParams np = p;
        np.exposureTime = exposure->value();
        np.gain = gain->value();
        np.width = width->value();
        np.height = height->value();
        np.triggerMode = trigger->currentIndex() == 1;
        np.pixelFormat = pixel->currentText();
        if (!cam->setParams(np))
            return QStringLiteral("写入失败 (相机拒绝部分参数)");
        CameraParams cur = cam->getParams();
        exposure->setValue(cur.exposureTime > 0 ? cur.exposureTime : exposure->value());
        gain->setValue(cur.gain >= 0 ? cur.gain : gain->value());
        width->setValue(cur.width > 0 ? cur.width : width->value());
        height->setValue(cur.height > 0 ? cur.height : height->value());
        return {};
    };

    connect(refreshBtn, &QPushButton::clicked, &dlg, [&]() {
        CameraParams cur = cam->getParams();
        exposure->setValue(cur.exposureTime > 0 ? cur.exposureTime : exposure->value());
        gain->setValue(cur.gain >= 0 ? cur.gain : gain->value());
        width->setValue(cur.width > 0 ? cur.width : width->value());
        height->setValue(cur.height > 0 ? cur.height : height->value());
        trigger->setCurrentIndex(cur.triggerMode ? 1 : 0);
    });
    connect(applyBtn, &QPushButton::clicked, &dlg, [&]() {
        const QString err = apply();
        if (!err.isEmpty())
            QMessageBox::warning(&dlg, QStringLiteral("应用参数"), err);
    });
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    dlg.exec();
}

void CameraManagerDialog::onTestGrab(int index)
{
    if (index < 0 || index >= MAX_CAMERAS || !m_cameras[index]) return;
    auto* cam = m_cameras[index];

    if (!cam->isOpen()) {
        QMessageBox::information(this, "提示", "相机未连接");
        return;
    }

    // 先开始采集
    cam->startAcquisition();
    QThread::msleep(200); // 等一帧

    CvImage frame = cam->grabFrame(2000);
    cam->stopAcquisition();

    bool hasFrame = false;
#ifdef VI_HAS_OPENCV
    hasFrame = !frame.empty();
#else
    hasFrame = !frame.isNull();
#endif

    if (hasFrame) {
#ifdef VI_HAS_OPENCV
        QMessageBox::information(this, "拍照测试",
            QString("CCD%1 拍照成功!\n图像尺寸: %2x%3")
            .arg(index + 1).arg(frame.cols).arg(frame.rows));
#else
        QMessageBox::information(this, "拍照测试",
            QString("CCD%1 拍照成功!").arg(index + 1));
#endif
    } else {
        QMessageBox::warning(this, "拍照测试", "拍照失败, 未获取到图像");
    }
}

void CameraManagerDialog::onScanNetwork()
{
    // 创建临时 GigE 相机扫描
    GigECamera scanner;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto cameras = scanner.enumerateCameras();
    QApplication::restoreOverrideCursor();

    if (cameras.isEmpty()) {
        QMessageBox::information(this, "扫描结果",
            "未发现 GigE 相机\n\n"
            "请检查:\n"
            "1. 相机已通电\n"
            "2. 网线已连接\n"
            "3. 工控机和相机在同一网段\n\n"
            "也可以手动输入相机IP (从 pylon Viewer 查看)");
        return;
    }

    // 弹出选择对话框, 填入空闲槽位
    QStringList items;
    for (const auto& cam : cameras) {
        items << QString("%1 %2 (%3) IP:%4")
                 .arg(cam.vendor, cam.model, cam.serialNumber, cam.ipAddress);
    }

    QString selected = QInputDialog::getItem(this, "发现相机",
        QString("找到 %1 台相机, 选择要连接的:").arg(cameras.size()),
        items, 0, false);

    if (selected.isEmpty()) return;

    int selIdx = items.indexOf(selected);
    if (selIdx < 0) return;

    // 找一个空闲槽位
    int freeSlot = -1;
    for (int i = 0; i < MAX_CAMERAS; i++) {
        if (!m_configs[i].connected) { freeSlot = i; break; }
    }
    if (freeSlot < 0) {
        QMessageBox::warning(this, "提示", "所有8个槽位已满, 请先断开一个");
        return;
    }

    // 填入IP
    m_slotWidgets[freeSlot].ipEdit->setText(cameras[selIdx].ipAddress);
    onConnectClicked(freeSlot);
}

} // namespace VisionInspector
