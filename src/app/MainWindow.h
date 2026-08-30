/**
 * @file MainWindow.h
 * @brief Main Window
 */

#pragma once

#include "../core/ProjectManager.h"
#include "../core/UserManager.h"
#include "../core/GlobalVariables.h"
#include "../engine/FlowEngine.h"
#include "../engine/InspectionResult.h"
#include "../hal/HardwareManager.h"
#include "../hal/ICameraDriver.h"
#include "../hal/DeshengCamera.h"
#include "../hal/VirtualCamera.h"
#include "../ui/widgets/MultiViewWidget.h"
#include <QMainWindow>
#include <QSplitter>
#include <QPointer>

class QDockWidget;
class QToolBar;
class QMenu;
class QAction;
class QLabel;

namespace VisionInspector {

class DisplayArea;
class FlowEditor;
class Toolbox;
class DataPanel;
class LogPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNewProject();
    void onNewFromTemplate();
    void onOpenProject();
    void onSaveProject();
    void onSaveAsProject();
    void onSaveAnnotatedImage();
    void onExecuteOnce();
    void onStartRunning();
    void onStopRunning();
    void onSystemSettings();
    void onProjectSettings();
    void onUISettings();
    void onGlobalVariables();
    void onSwitchUser();
    void onPlcSimulator();
    void onAnnotationTool();
    void onTeachWizard();
    void onAbout();
    void onExportCsv();
    void onScanCameras();
    void onOpenCamera();
    void onUseVirtualCamera();
    void onCameraImageReceived(const CvImage& image);
    void onToolAdded(const QString& typeName);
    void onEditToolProperties(int index);
    void toggleFullscreen();
    void onZoomIn();
    void onZoomOut();
    void onZoomFit();
    void onZoom1x1();

private:
    void setupUI();
    void createMenus();
    void createToolBar();
    void createViewMenu();
    void createStatusBar();
    void loadSettings();
    void saveSettings();
    void applyUserRole(UserRole role);

    // Core components
    ProjectManager* m_projectMgr = nullptr;
    UserManager* m_userMgr = nullptr;
    GlobalVariables* m_globalVars = nullptr;
    FlowEngine* m_flowEngine = nullptr;
    GlobalStats* m_stats = nullptr;
    HardwareManager* m_hardware = nullptr;
    ICameraDriver* m_camera = nullptr;      // 当前相机驱动(度申/虚拟相机等)

    void setCameraDriver(ICameraDriver* cam); // 切换驱动并重连信号

    // UI components
    DisplayArea* m_displayArea = nullptr;
    FlowEditor* m_flowEditor = nullptr;
    Toolbox* m_toolbox = nullptr;
    DataPanel* m_dataPanel = nullptr;
    LogPanel* m_logPanel = nullptr;
    MultiViewWidget* m_multiView = nullptr;

    QSplitter* m_mainSplitter = nullptr;
    QSplitter* m_vSplitter = nullptr;

    // Dock widgets
    QDockWidget* m_logDock = nullptr;

    // Toolbar and menus
    QToolBar* m_mainToolBar = nullptr;

    // Status bar
    QLabel* m_statusLabel = nullptr;
    QLabel* m_userLabel = nullptr;
    QLabel* m_connectionLabel = nullptr;
    QLabel* m_cameraLabel = nullptr;
    QLabel* m_okCountLabel = nullptr;
    QLabel* m_ngCountLabel = nullptr;
    QLabel* m_yieldLabel = nullptr;
    QLabel* m_fileLabel = nullptr;
};

} // namespace VisionInspector
