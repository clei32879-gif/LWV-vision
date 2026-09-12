/**
 * @file MainToolBar.cpp
 * @brief 主工具栏实现
 */

#include "MainToolBar.h"

#include <QApplication>
#include <QStyle>
#include <QLabel>

namespace VisionInspector {

MainToolBar::MainToolBar(QWidget* parent)
    : QToolBar("主工具栏", parent)
{
    setObjectName("MainToolBar");
    setMovable(false);
    setIconSize(QSize(24, 24));
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    createActions();
    setupToolBar();
}

MainToolBar::~MainToolBar() = default;

// ======== 创建 Action ========

QAction* MainToolBar::createAction(const QString& text, const QString& iconText,
                                    const QString& shortcut, const QString& tooltip)
{
    auto* action = new QAction(text, this);
    if (!shortcut.isEmpty()) {
        action->setShortcut(QKeySequence(shortcut));
    }
    if (!tooltip.isEmpty()) {
        action->setToolTip(tooltip);
    }
    // 使用文字图标作为占位 (后续可替换为真实图标)
    if (!iconText.isEmpty()) {
        action->setIconText(iconText);
    }
    return action;
}

void MainToolBar::createActions()
{
    // 文件操作
    m_newAction = createAction("新建", "📄", "Ctrl+N", "新建项目 (Ctrl+N)");
    m_openAction = createAction("打开", "📂", "Ctrl+O", "打开项目 (Ctrl+O)");
    m_saveAction = createAction("保存", "💾", "Ctrl+S", "保存项目 (Ctrl+S)");

    // 运行控制
    m_startAction = createAction("开始", "▶", "F5", "开始检测 (F5)");
    m_stopAction = createAction("停止", "⏹", "Shift+F5", "停止检测 (Shift+F5)");
    m_stopAction->setEnabled(false);
    m_pauseAction = createAction("暂停", "⏸", "F6", "暂停/继续检测 (F6)");
    m_pauseAction->setEnabled(false);
    m_pauseAction->setCheckable(true);

    // 视图缩放
    m_zoomInAction = createAction("放大", "🔍+", "Ctrl++", "放大图像 (Ctrl++)");
    m_zoomOutAction = createAction("缩小", "🔍-", "Ctrl+-", "缩小图像 (Ctrl+-)");
    m_zoomFitAction = createAction("适配", "⊞", "Ctrl+0", "适配窗口 (Ctrl+0)");

    // 缩放下拉框
    m_zoomCombo = new QComboBox(this);
    m_zoomCombo->setEditable(true);
    m_zoomCombo->setFixedWidth(80);
    m_zoomCombo->setToolTip("缩放比例");
    m_zoomCombo->addItems({"25%", "50%", "75%", "100%", "150%", "200%", "400%"});
    m_zoomCombo->setCurrentText("100%");
    m_zoomCombo->setStyleSheet(
        "QComboBox { background-color: #f4f9ff; color: #ddd; border: 1px solid #9cc2e8; "
        "padding: 2px 4px; border-radius: 2px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background-color: #f4f9ff; color: #ddd; "
        "selection-background-color: #505050; }"
    );
}

void MainToolBar::setupToolBar()
{
    // ==== 文件操作组 ====
    addAction(m_newAction);
    addAction(m_openAction);
    addAction(m_saveAction);

    addSeparator();

    // ==== 运行控制组 ====
    addAction(m_startAction);
    addAction(m_pauseAction);
    addAction(m_stopAction);

    addSeparator();

    // ==== 视图缩放组 ====
    addAction(m_zoomOutAction);
    addWidget(m_zoomCombo);
    addAction(m_zoomInAction);
    addAction(m_zoomFitAction);

    // 连接信号
    connect(m_newAction, &QAction::triggered, this, &MainToolBar::newClicked);
    connect(m_openAction, &QAction::triggered, this, &MainToolBar::openClicked);
    connect(m_saveAction, &QAction::triggered, this, &MainToolBar::saveClicked);

    connect(m_startAction, &QAction::triggered, this, &MainToolBar::startClicked);
    connect(m_stopAction, &QAction::triggered, this, &MainToolBar::stopClicked);
    connect(m_pauseAction, &QAction::toggled, this, &MainToolBar::pauseClicked);

    connect(m_zoomInAction, &QAction::triggered, this, &MainToolBar::zoomInClicked);
    connect(m_zoomOutAction, &QAction::triggered, this, &MainToolBar::zoomOutClicked);
    connect(m_zoomFitAction, &QAction::triggered, this, &MainToolBar::zoomFitClicked);

    // 缩放下拉框变化
    connect(m_zoomCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        QString numText = text;
        numText.remove('%');
        bool ok = false;
        int percent = numText.toInt(&ok);
        if (ok && percent > 0) {
            emit zoomLevelChanged(percent);
        }
    });
}

// ======== 状态控制 ========

void MainToolBar::setRunning(bool running)
{
    m_startAction->setEnabled(!running);
    m_stopAction->setEnabled(running);
    m_pauseAction->setEnabled(running);

    if (!running) {
        m_pauseAction->setChecked(false);
    }
}

void MainToolBar::setPaused(bool paused)
{
    m_pauseAction->setChecked(paused);
    if (paused) {
        m_pauseAction->setText("继续");
        m_pauseAction->setIconText("▶");
    } else {
        m_pauseAction->setText("暂停");
        m_pauseAction->setIconText("⏸");
    }
}

void MainToolBar::setZoomLevel(double percent)
{
    m_zoomCombo->blockSignals(true);
    m_zoomCombo->setCurrentText(QString("%1%").arg(static_cast<int>(percent)));
    m_zoomCombo->blockSignals(false);
}

void MainToolBar::setContinuousMode(bool continuous)
{
    emit continuousModeChanged(continuous);
}

} // namespace VisionInspector
