/**
 * @file MainToolBar.h
 * @brief 主工具栏
 *
 * 提供常用操作的快速访问:
 *   文件操作: 新建、打开、保存
 *   运行控制: 开始、停止、暂停
 *   视图缩放: 放大、缩小、适配
 */

#pragma once
#include <QToolBar>
#include <QAction>
#include <QComboBox>

namespace VisionInspector {

class MainToolBar : public QToolBar {
    Q_OBJECT

public:
    explicit MainToolBar(QWidget* parent = nullptr);
    ~MainToolBar() override;

    // ======== action 访问 ========

    QAction* newAction() const { return m_newAction; }
    QAction* openAction() const { return m_openAction; }
    QAction* saveAction() const { return m_saveAction; }
    QAction* startAction() const { return m_startAction; }
    QAction* stopAction() const { return m_stopAction; }
    QAction* pauseAction() const { return m_pauseAction; }
    QAction* zoomInAction() const { return m_zoomInAction; }
    QAction* zoomOutAction() const { return m_zoomOutAction; }
    QAction* zoomFitAction() const { return m_zoomFitAction; }

    // ======== 状态控制 ========

    /** 设置运行状态 */
    void setRunning(bool running);
    void setPaused(bool paused);

    /** 设置缩放比例显示 */
    void setZoomLevel(double percent);

    /** 循环执行模式 */
    void setContinuousMode(bool continuous);

signals:
    // 文件操作
    void newClicked();
    void openClicked();
    void saveClicked();

    // 运行控制
    void startClicked();
    void stopClicked();
    void pauseClicked(bool paused);

    // 缩放
    void zoomInClicked();
    void zoomOutClicked();
    void zoomFitClicked();
    void zoomLevelChanged(int percent);

    // 循环模式
    void continuousModeChanged(bool enabled);

private:
    void createActions();
    void setupToolBar();

    QAction* createAction(const QString& text, const QString& iconText,
                          const QString& shortcut, const QString& tooltip);

    // 文件操作
    QAction* m_newAction = nullptr;
    QAction* m_openAction = nullptr;
    QAction* m_saveAction = nullptr;

    // 运行控制
    QAction* m_startAction = nullptr;
    QAction* m_stopAction = nullptr;
    QAction* m_pauseAction = nullptr;

    // 缩放
    QAction* m_zoomInAction = nullptr;
    QAction* m_zoomOutAction = nullptr;
    QAction* m_zoomFitAction = nullptr;
    QComboBox* m_zoomCombo = nullptr;
};

} // namespace VisionInspector
