/**
 * @file MenuBar.h
 * @brief 主菜单栏
 *
 * 提供完整的菜单结构:
 *   文件 - 新建项目、打开项目、保存、另存为、最近项目、退出
 *   编辑 - 撤销、重做、剪切、复制、粘贴、删除
 *   视图 - 工具箱、数据面板、日志面板、全屏
 *   运行 - 开始检测、停止检测、单步执行
 *   帮助 - 关于
 */

#pragma once
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStringList>

namespace VisionInspector {

class MenuBar : public QMenuBar {
    Q_OBJECT

public:
    explicit MenuBar(QWidget* parent = nullptr);
    ~MenuBar() override;

    // ======== 菜单访问 ========

    QMenu* fileMenu() const { return m_fileMenu; }
    QMenu* editMenu() const { return m_editMenu; }
    QMenu* viewMenu() const { return m_viewMenu; }
    QMenu* runMenu() const { return m_runMenu; }
    QMenu* helpMenu() const { return m_helpMenu; }

    // ======== action 访问 ========

    QAction* newProjectAction() const { return m_newAction; }
    QAction* openProjectAction() const { return m_openAction; }
    QAction* saveAction() const { return m_saveAction; }
    QAction* saveAsAction() const { return m_saveAsAction; }
    QAction* exitAction() const { return m_exitAction; }

    QAction* undoAction() const { return m_undoAction; }
    QAction* redoAction() const { return m_redoAction; }
    QAction* cutAction() const { return m_cutAction; }
    QAction* copyAction() const { return m_copyAction; }
    QAction* pasteAction() const { return m_pasteAction; }
    QAction* deleteAction() const { return m_deleteAction; }

    QAction* toggleToolboxAction() const { return m_toggleToolboxAction; }
    QAction* toggleDataPanelAction() const { return m_toggleDataPanelAction; }
    QAction* toggleLogPanelAction() const { return m_toggleLogPanelAction; }
    QAction* fullscreenAction() const { return m_fullscreenAction; }

    QAction* startAction() const { return m_startAction; }
    QAction* stopAction() const { return m_stopAction; }
    QAction* stepAction() const { return m_stepAction; }

    QAction* aboutAction() const { return m_aboutAction; }

    // ======== 状态更新 ========

    /** 更新最近打开文件列表 */
    void setRecentFiles(const QStringList& files);

    /** 设置运行状态 (影响菜单启用/禁用) */
    void setRunning(bool running);

    /** 设置是否有可撤销/重做的操作 */
    void setUndoAvailable(bool available);
    void setRedoAvailable(bool available);

signals:
    // 文件操作
    void newProjectRequested();
    void openProjectRequested();
    void saveRequested();
    void saveAsRequested();
    void recentFileRequested(const QString& path);
    void exitRequested();

    // 编辑操作
    void undoRequested();
    void redoRequested();
    void cutRequested();
    void copyRequested();
    void pasteRequested();
    void deleteRequested();

    // 视图操作
    void toggleToolbox(bool visible);
    void toggleDataPanel(bool visible);
    void toggleLogPanel(bool visible);
    void toggleFullscreen(bool fullscreen);

    // 运行操作
    void startRequested();
    void stopRequested();
    void stepRequested();

    // 帮助
    void aboutRequested();

private:
    void createFileMenu();
    void createEditMenu();
    void createViewMenu();
    void createRunMenu();
    void createHelpMenu();

    QAction* createAction(const QString& text, const QString& shortcut = QString(),
                          const QString& tooltip = QString());

    // 菜单
    QMenu* m_fileMenu = nullptr;
    QMenu* m_editMenu = nullptr;
    QMenu* m_viewMenu = nullptr;
    QMenu* m_runMenu = nullptr;
    QMenu* m_helpMenu = nullptr;
    QMenu* m_recentMenu = nullptr;

    // 文件
    QAction* m_newAction = nullptr;
    QAction* m_openAction = nullptr;
    QAction* m_saveAction = nullptr;
    QAction* m_saveAsAction = nullptr;
    QAction* m_exitAction = nullptr;

    // 编辑
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_cutAction = nullptr;
    QAction* m_copyAction = nullptr;
    QAction* m_pasteAction = nullptr;
    QAction* m_deleteAction = nullptr;

    // 视图
    QAction* m_toggleToolboxAction = nullptr;
    QAction* m_toggleDataPanelAction = nullptr;
    QAction* m_toggleLogPanelAction = nullptr;
    QAction* m_fullscreenAction = nullptr;

    // 运行
    QAction* m_startAction = nullptr;
    QAction* m_stopAction = nullptr;
    QAction* m_stepAction = nullptr;

    // 帮助
    QAction* m_aboutAction = nullptr;
};

} // namespace VisionInspector
