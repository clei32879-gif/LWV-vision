/**
 * @file MenuBar.cpp
 * @brief 主菜单栏实现
 */

#include "MenuBar.h"

#include <QApplication>
#include <QKeySequence>

namespace VisionInspector {

MenuBar::MenuBar(QWidget* parent)
    : QMenuBar(parent)
{
    createFileMenu();
    createEditMenu();
    createViewMenu();
    createRunMenu();
    createHelpMenu();

    setStyleSheet(
        "QMenuBar { background-color: #2d2d2d; color: #ddd; font-size: 13px; padding: 2px; }"
        "QMenuBar::item:selected { background-color: #505050; }"
        "QMenu { background-color: #3c3c3c; color: #ddd; border: 1px solid #555; }"
        "QMenu::item:selected { background-color: #505050; }"
        "QMenu::separator { height: 1px; background: #555; margin: 4px 8px; }"
    );
}

MenuBar::~MenuBar() = default;

// ======== 辅助方法 ========

QAction* MenuBar::createAction(const QString& text, const QString& shortcut,
                                const QString& tooltip)
{
    auto* action = new QAction(text, this);
    if (!shortcut.isEmpty()) {
        action->setShortcut(QKeySequence(shortcut));
    }
    if (!tooltip.isEmpty()) {
        action->setToolTip(tooltip);
    }
    return action;
}

// ======== 文件菜单 ========

void MenuBar::createFileMenu()
{
    m_fileMenu = addMenu("文件(&F)");

    m_newAction = createAction("新建项目(&N)", "Ctrl+N", "创建新检测项目");
    m_fileMenu->addAction(m_newAction);
    connect(m_newAction, &QAction::triggered, this, &MenuBar::newProjectRequested);

    m_openAction = createAction("打开项目(&O)...", "Ctrl+O", "打开已有项目文件");
    m_fileMenu->addAction(m_openAction);
    connect(m_openAction, &QAction::triggered, this, &MenuBar::openProjectRequested);

    m_fileMenu->addSeparator();

    m_saveAction = createAction("保存(&S)", "Ctrl+S", "保存当前项目");
    m_fileMenu->addAction(m_saveAction);
    connect(m_saveAction, &QAction::triggered, this, &MenuBar::saveRequested);

    m_saveAsAction = createAction("另存为(&A)...", "Ctrl+Shift+S", "将项目保存到新位置");
    m_fileMenu->addAction(m_saveAsAction);
    connect(m_saveAsAction, &QAction::triggered, this, &MenuBar::saveAsRequested);

    m_fileMenu->addSeparator();

    // 最近打开项目子菜单
    m_recentMenu = m_fileMenu->addMenu("最近项目(&R)");

    m_fileMenu->addSeparator();

    m_exitAction = createAction("退出(&X)", "Ctrl+Q", "退出程序");
    m_fileMenu->addAction(m_exitAction);
    connect(m_exitAction, &QAction::triggered, this, &MenuBar::exitRequested);
}

// ======== 编辑菜单 ========

void MenuBar::createEditMenu()
{
    m_editMenu = addMenu("编辑(&E)");

    m_undoAction = createAction("撤销(&U)", "Ctrl+Z", "撤销上一步操作");
    m_undoAction->setEnabled(false);
    m_editMenu->addAction(m_undoAction);
    connect(m_undoAction, &QAction::triggered, this, &MenuBar::undoRequested);

    m_redoAction = createAction("重做(&R)", "Ctrl+Y", "重做已撤销的操作");
    m_redoAction->setEnabled(false);
    m_editMenu->addAction(m_redoAction);
    connect(m_redoAction, &QAction::triggered, this, &MenuBar::redoRequested);

    m_editMenu->addSeparator();

    m_cutAction = createAction("剪切(&T)", "Ctrl+X", "剪切选中内容到剪贴板");
    m_editMenu->addAction(m_cutAction);
    connect(m_cutAction, &QAction::triggered, this, &MenuBar::cutRequested);

    m_copyAction = createAction("复制(&C)", "Ctrl+C", "复制选中内容到剪贴板");
    m_editMenu->addAction(m_copyAction);
    connect(m_copyAction, &QAction::triggered, this, &MenuBar::copyRequested);

    m_pasteAction = createAction("粘贴(&P)", "Ctrl+V", "从剪贴板粘贴内容");
    m_editMenu->addAction(m_pasteAction);
    connect(m_pasteAction, &QAction::triggered, this, &MenuBar::pasteRequested);

    m_deleteAction = createAction("删除(&D)", "Del", "删除选中内容");
    m_editMenu->addAction(m_deleteAction);
    connect(m_deleteAction, &QAction::triggered, this, &MenuBar::deleteRequested);
}

// ======== 视图菜单 ========

void MenuBar::createViewMenu()
{
    m_viewMenu = addMenu("视图(&V)");

    m_toggleToolboxAction = createAction("工具箱(&T)", "Ctrl+1", "显示/隐藏工具箱");
    m_toggleToolboxAction->setCheckable(true);
    m_toggleToolboxAction->setChecked(true);
    m_viewMenu->addAction(m_toggleToolboxAction);
    connect(m_toggleToolboxAction, &QAction::toggled, this, &MenuBar::toggleToolbox);

    m_toggleDataPanelAction = createAction("数据面板(&D)", "Ctrl+2", "显示/隐藏数据面板");
    m_toggleDataPanelAction->setCheckable(true);
    m_toggleDataPanelAction->setChecked(true);
    m_viewMenu->addAction(m_toggleDataPanelAction);
    connect(m_toggleDataPanelAction, &QAction::toggled, this, &MenuBar::toggleDataPanel);

    m_toggleLogPanelAction = createAction("日志面板(&L)", "Ctrl+3", "显示/隐藏日志面板");
    m_toggleLogPanelAction->setCheckable(true);
    m_toggleLogPanelAction->setChecked(true);
    m_viewMenu->addAction(m_toggleLogPanelAction);
    connect(m_toggleLogPanelAction, &QAction::toggled, this, &MenuBar::toggleLogPanel);

    m_viewMenu->addSeparator();

    m_fullscreenAction = createAction("全屏(&F)", "F11", "切换全屏模式");
    m_fullscreenAction->setCheckable(true);
    m_viewMenu->addAction(m_fullscreenAction);
    connect(m_fullscreenAction, &QAction::toggled, this, &MenuBar::toggleFullscreen);
}

// ======== 运行菜单 ========

void MenuBar::createRunMenu()
{
    m_runMenu = addMenu("运行(&R)");

    m_startAction = createAction("开始检测(&S)", "F5", "开始执行检测流程");
    m_runMenu->addAction(m_startAction);
    connect(m_startAction, &QAction::triggered, this, &MenuBar::startRequested);

    m_stopAction = createAction("停止检测(&T)", "Shift+F5", "停止当前检测流程");
    m_stopAction->setEnabled(false);
    m_runMenu->addAction(m_stopAction);
    connect(m_stopAction, &QAction::triggered, this, &MenuBar::stopRequested);

    m_runMenu->addSeparator();

    m_stepAction = createAction("单步执行(&E)", "F10", "逐工具单步执行检测流程");
    m_runMenu->addAction(m_stepAction);
    connect(m_stepAction, &QAction::triggered, this, &MenuBar::stepRequested);
}

// ======== 帮助菜单 ========

void MenuBar::createHelpMenu()
{
    m_helpMenu = addMenu("帮助(&H)");

    m_aboutAction = createAction("关于(&A)...", "", "关于 VisionInspector");
    m_helpMenu->addAction(m_aboutAction);
    connect(m_aboutAction, &QAction::triggered, this, &MenuBar::aboutRequested);
}

// ======== 状态更新 ========

void MenuBar::setRecentFiles(const QStringList& files)
{
    m_recentMenu->clear();
    if (files.isEmpty()) {
        m_recentMenu->addAction("(无)")->setEnabled(false);
        return;
    }

    for (int i = 0; i < files.size(); ++i) {
        const QString& path = files[i];
        QString text = QString("&%1 %2").arg(i + 1).arg(path);
        QAction* action = m_recentMenu->addAction(text);
        connect(action, &QAction::triggered, this, [this, path]() {
            emit recentFileRequested(path);
        });
    }
}

void MenuBar::setRunning(bool running)
{
    m_startAction->setEnabled(!running);
    m_stopAction->setEnabled(running);
    m_stepAction->setEnabled(!running);
}

void MenuBar::setUndoAvailable(bool available)
{
    m_undoAction->setEnabled(available);
}

void MenuBar::setRedoAvailable(bool available)
{
    m_redoAction->setEnabled(available);
}

} // namespace VisionInspector
