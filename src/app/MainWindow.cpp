#include "MainWindow.h"
#include "../utils/Logger.h"
#include "../engine/ToolRegistry.h"
#include "../ui/DisplayArea.h"
#include "../ui/FlowEditor.h"
#include "../ui/Toolbox.h"
#include "../ui/DataPanel.h"
#include "../ui/LogPanel.h"
#include "../ui/PropertyDialog.h"
#include "../ui/widgets/ImageViewWidget.h"
#include "../ui/PlcSimulatorDialog.h"
#include "../ui/SettingsDialogs.h"
#include "../ui/LoginDialog.h"
#include "../ui/AnnotationDialog.h"
#include "../ui/IconHelper.h"
#include "../hal/ModbusTcpMaster.h"
#include "AppSettings.h"

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QDockWidget>
#include <QLabel>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QCloseEvent>
#include <QSettings>
#include <QApplication>
#include <QTimer>
#include <QStyle>
#include <QIcon>

namespace VisionInspector {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    qDebug() << "MainWindow: Starting construction...";
    
    m_projectMgr = new ProjectManager(this);
    m_userMgr = new UserManager(this);
    m_globalVars = new GlobalVariables(this);
    m_flowEngine = new FlowEngine(this);
    m_stats = new GlobalStats(this);
    m_hardware = new HardwareManager(this);
    m_projectMgr->setServices(m_flowEngine, m_globalVars);

    // 相机驱动: 装了度申SDK用真相机, 否则用虚拟相机(回放/合成图案)
#ifdef VI_HAS_DVP2
    setCameraDriver(new DeshengCamera(this));
#else
    setCameraDriver(new VirtualCamera(this));
#endif

    qDebug() << "MainWindow: About to call setupUI...";
    setupUI();
    qDebug() << "MainWindow: setupUI completed";
    
    loadSettings();

    // 连接流程执行信号到FlowEditor状态更新（必须在setupUI之后）
    connect(m_flowEngine, &FlowEngine::toolStatusChanged, this,
        [this](Flow* flow, int index, ToolStatus status) {
            if (m_flowEditor) m_flowEditor->updateToolStatus(index, status);
        });

    // 流程执行结果 (从工作线程异步送达UI线程)
    connect(m_flowEngine, &FlowEngine::flowExecuted, this,
        [this](Flow*, bool allOk) {
#ifdef VI_HAS_OPENCV
            CvImagePtr img = m_flowEngine->lastImage();
            if (img && !img->empty()) {
                m_multiView->setImage(0, cvMatToQImage(*img));
            }
#endif
            // 检测结果叠加层
            m_multiView->setOverlays(0, m_flowEngine->lastOverlays());
            m_statusLabel->setText(allOk ? "执行完成 (全部OK)" : "执行完成 (有NG)");

            // NG图像自动保存 (系统设置开启时)
            if (!allOk && QSettings("VisionInspector", "VisionInspector")
                              .value("autoSaveNGImages", false).toBool()) {
                ImageViewWidget* view = m_multiView->viewAt(0);
                if (view) {
                    const QImage annotated = view->renderAnnotated();
                    if (!annotated.isNull()) {
                        const QString baseDir = QSettings("VisionInspector", "VisionInspector")
                                                    .value("imageSaveDir",
                                                           QCoreApplication::applicationDirPath()
                                                               + "/images")
                                                    .toString();
                        QDir().mkpath(baseDir + "/ng");
                        const QString ngPath = baseDir + "/ng/NG_"
                                               + QDateTime::currentDateTime().toString(
                                                     "yyyyMMdd_hhmmss_zzz")
                                               + ".png";
                        if (annotated.save(ngPath))
                            m_logPanel->appendLog("NG图像已保存: " + ngPath);
                    }
                }
            }
        });

    // 连续运行状态
    connect(m_flowEngine, &FlowEngine::runStateChanged, this,
        [this](bool running) {
            m_statusLabel->setText(running ? "运行中..." : "已停止");
        });

    // 用户权限: 角色变化时启用/禁用编辑功能
    connect(m_userMgr, &UserManager::roleChanged, this, &MainWindow::applyUserRole);

    setWindowTitle("LW Vision v1.0.0 - 立维视觉");
    resize(1400, 900);
    VI_LOG_INFO("MainWindow created");
}

MainWindow::~MainWindow() {
    if (m_camera) m_camera->closeCamera();
    saveSettings();
}

// ============================================================
// UI Setup
// ============================================================

void MainWindow::setupUI() {
    // 菜单栏（不含View菜单）
    createMenus();
    // 工具栏
    createToolBar();
    // 状态栏
    createStatusBar();
    
    // 中间区域：工具箱 | 流程栏 | 相机图像栏（水平分割）
    m_toolbox = new Toolbox(this);
    m_flowEditor = new FlowEditor(this);
    m_multiView = new MultiViewWidget(this);
    m_multiView->setLayout(2, 4);
    
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setHandleWidth(6);
    m_mainSplitter->addWidget(m_toolbox);
    m_mainSplitter->addWidget(m_flowEditor);
    m_mainSplitter->addWidget(m_multiView);
    m_mainSplitter->setStretchFactor(0, 1);  // 工具箱
    m_mainSplitter->setStretchFactor(1, 0);  // 流程栏不参与拉伸
    m_mainSplitter->setStretchFactor(2, 1);  // 图像栏拉伸

    // 底部CCD检测项目表
    m_dataPanel = new DataPanel(this);

    // 整体垂直分割：上部(工具箱+流程+图像) | 下部(检测项目表)
    m_vSplitter = new QSplitter(Qt::Vertical, this);
    m_vSplitter->setHandleWidth(6);
    m_vSplitter->addWidget(m_mainSplitter);
    m_vSplitter->addWidget(m_dataPanel);
    m_vSplitter->setStretchFactor(0, 7);
    m_vSplitter->setStretchFactor(1, 2);
    setCentralWidget(m_vSplitter);
    
    // 日志栏dock（默认隐藏）
    m_logPanel = new LogPanel(this);
    m_logDock = new QDockWidget(QString::fromUtf8("\u65e5\u5fd7"), this);
    m_logDock->setObjectName("LogDock");
    m_logDock->setWidget(m_logPanel);
    m_logDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    m_logDock->hide();
    
    // View菜单（在dock创建之后）
    createViewMenu();
    
    connect(m_toolbox, &Toolbox::toolDoubleClicked, this, &MainWindow::onToolAdded);
    connect(m_flowEditor, &FlowEditor::toolEditProperties, this, &MainWindow::onEditToolProperties);
    
    setWindowTitle("LW Vision v1.0.0 - 立维视觉");
    resize(1400, 900);

    // 延迟设置分割器尺寸，确保布局已完成
    QTimer::singleShot(0, this, [this]() {
        m_mainSplitter->setSizes({200, 160, 1000});
    });
}

void MainWindow::createMenus() {
    QMenu* fileMenu = menuBar()->addMenu(QString::fromUtf8("\u6587\u4ef6(&F)"));
    fileMenu->addAction(QString::fromUtf8("\u65b0\u5efa\u9879\u76ee"), this, &MainWindow::onNewProject);
    fileMenu->addAction(QString::fromUtf8("\u4ece\u6a21\u677f\u65b0\u5efa(\u7b5b\u9009\u673a)"), this, &MainWindow::onNewFromTemplate);
    fileMenu->addAction(QString::fromUtf8("\u6253\u5f00\u9879\u76ee"), this, &MainWindow::onOpenProject);
    fileMenu->addAction(QString::fromUtf8("\u4fdd\u5b58\u9879\u76ee"), this, &MainWindow::onSaveProject);
    fileMenu->addAction(QString::fromUtf8("\u53e6\u5b58\u4e3a..."), this, &MainWindow::onSaveAsProject);
    fileMenu->addSeparator();
    fileMenu->addAction(QString::fromUtf8("\u4fdd\u5b58\u5f53\u524d\u753b\u9762(\u542b\u6807\u6ce8)"), this, &MainWindow::onSaveAnnotatedImage);
    fileMenu->addSeparator();
    fileMenu->addAction(QString::fromUtf8("\u9000\u51fa"), qApp, &QApplication::quit);

    QMenu* setMenu = menuBar()->addMenu(QString::fromUtf8("\u8bbe\u7f6e(&S)"));
    setMenu->addAction(QString::fromUtf8("\u7cfb\u7edf\u8bbe\u7f6e"), this, &MainWindow::onSystemSettings);
    setMenu->addAction(QString::fromUtf8("\u9879\u76ee\u8bbe\u7f6e"), this, &MainWindow::onProjectSettings);
    setMenu->addAction(QString::fromUtf8("\u5168\u5c40\u53d8\u91cf"), this, &MainWindow::onGlobalVariables);
    setMenu->addSeparator();
    setMenu->addAction(QString::fromUtf8("PLC\u6a21\u62df\u5668..."), this, &MainWindow::onPlcSimulator);

    QMenu* annoMenu = menuBar()->addMenu(QString::fromUtf8("\u6807\u6ce8(&A)"));
    annoMenu->addAction(QString::fromUtf8("\u7f3a\u9677\u6807\u6ce8\u5de5\u5177..."), this, &MainWindow::onAnnotationTool);

    QMenu* camMenu = menuBar()->addMenu(QString::fromUtf8("\u76f8\u673a(&C)"));
    camMenu->addAction(QString::fromUtf8("\u626b\u63cf\u76f8\u673a"), this, &MainWindow::onScanCameras);
    camMenu->addAction(QString::fromUtf8("\u6253\u5f00/\u5173\u95ed\u76f8\u673a"), this, &MainWindow::onOpenCamera);
    camMenu->addAction(QString::fromUtf8("\u4f7f\u7528\u865a\u62df\u76f8\u673a"), this, &MainWindow::onUseVirtualCamera);

    QMenu* opMenu = menuBar()->addMenu(QString::fromUtf8("\u64cd\u4f5c(&O)"));
    opMenu->addAction(QString::fromUtf8("\u6267\u884c\u7a0b\u5e8f"), this, &MainWindow::onExecuteOnce);
    opMenu->addAction(QString::fromUtf8("\u8fd0\u884c\u7a0b\u5e8f"), this, &MainWindow::onStartRunning);
    opMenu->addAction(QString::fromUtf8("\u505c\u6b62\u7a0b\u5e8f"), this, &MainWindow::onStopRunning);

    // View菜单会在createDockWidgets之后添加
}

void MainWindow::createViewMenu() {
    QMenu* viewMenu = menuBar()->addMenu(QString::fromUtf8("\u89c6\u56fe(&V)"));
    viewMenu->addAction(m_mainToolBar->toggleViewAction());
    viewMenu->addAction(m_logDock->toggleViewAction());
    viewMenu->addSeparator();
    viewMenu->addAction(QString::fromUtf8("\u5168\u5c4f\u663e\u793a"), this, &MainWindow::toggleFullscreen);
}

void MainWindow::createToolBar() {
    m_mainToolBar = addToolBar(QString::fromUtf8("\u4e3b\u5de5\u5177\u680f"));
    m_mainToolBar->setObjectName("MainToolBar");
    m_mainToolBar->setMovable(false);

    QAction* act;
    act = m_mainToolBar->addAction(IconHelper::newIcon(), QString::fromUtf8("\u65b0\u5efa"));
    connect(act, &QAction::triggered, this, &MainWindow::onNewProject);
    act = m_mainToolBar->addAction(IconHelper::openIcon(), QString::fromUtf8("\u6253\u5f00"));
    connect(act, &QAction::triggered, this, &MainWindow::onOpenProject);
    act = m_mainToolBar->addAction(IconHelper::saveIcon(), QString::fromUtf8("\u4fdd\u5b58"));
    connect(act, &QAction::triggered, this, &MainWindow::onSaveProject);

    m_mainToolBar->addSeparator();

    act = m_mainToolBar->addAction(IconHelper::executeIcon(), QString::fromUtf8("\u6267\u884c"));
    connect(act, &QAction::triggered, this, &MainWindow::onExecuteOnce);
    act = m_mainToolBar->addAction(IconHelper::runIcon(), QString::fromUtf8("\u8fd0\u884c"));
    connect(act, &QAction::triggered, this, &MainWindow::onStartRunning);
    act = m_mainToolBar->addAction(IconHelper::stopIcon(), QString::fromUtf8("\u505c\u6b62"));
    connect(act, &QAction::triggered, this, &MainWindow::onStopRunning);

    m_mainToolBar->addSeparator();

    act = m_mainToolBar->addAction(IconHelper::scanIcon(), QString::fromUtf8("\u626b\u63cf"));
    connect(act, &QAction::triggered, this, &MainWindow::onScanCameras);
    act = m_mainToolBar->addAction(IconHelper::cameraIcon(), QString::fromUtf8("\u76f8\u673a"));
    connect(act, &QAction::triggered, this, &MainWindow::onOpenCamera);

    m_mainToolBar->addSeparator();
    act = m_mainToolBar->addAction(IconHelper::userIcon(), QString::fromUtf8("\u7528\u6237"));
    connect(act, &QAction::triggered, this, &MainWindow::onSwitchUser);
}

void MainWindow::createStatusBar() {
    m_statusLabel = new QLabel("就绪");
    m_userLabel = new QLabel("用户: 管理员");
    m_connectionLabel = new QLabel("未连接");
    m_cameraLabel = new QLabel("相机: 无");
    m_okCountLabel = new QLabel("OK: 0");
    m_ngCountLabel = new QLabel("NG: 0");
    m_yieldLabel = new QLabel("良率: 100%");
    m_fileLabel = new QLabel("无项目");

    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_fileLabel);
    statusBar()->addPermanentWidget(m_okCountLabel);
    statusBar()->addPermanentWidget(m_ngCountLabel);
    statusBar()->addPermanentWidget(m_yieldLabel);
    statusBar()->addPermanentWidget(m_cameraLabel);
}

void MainWindow::onToolAdded(const QString& typeName) {
    ITool* tool = ToolRegistry::instance().createTool(typeName);
    if (!tool) {
        m_logPanel->appendLog(QString("无法创建工具: %1").arg(typeName));
        return;
    }
    
    // 智能命名：只有重复工具才加数字后缀
    Flow* flow = nullptr;
    if (m_flowEngine->flowCount() > 0) {
        flow = m_flowEngine->flows().first();
    } else {
        flow = new Flow(this);
        flow->setName("主流程");
        m_flowEngine->addFlow(flow);
    }
    
    // 统计同类型工具数量
    int sameTypeCount = 0;
    for (int i = 0; i < flow->toolCount(); ++i) {
        ITool* t = flow->toolAt(i);
        if (t && t->typeName() == typeName) sameTypeCount++;
    }
    
    if (sameTypeCount == 0) {
        // 第一个同类型工具，不加数字
        tool->setInstanceName(tool->displayName());
    } else {
        // 重复工具，加数字后缀
        tool->setInstanceName(QString("%1_%2").arg(tool->displayName()).arg(sameTypeCount + 1));
    }

    // 追加工具到流程末尾
    flow->addTool(tool);
    
    // 如果添加的是位置补正，自动在后面添加结束补正
    if (typeName == "PositionCorrection") {
        ITool* endTool = ToolRegistry::instance().createTool("EndCorrection");
        if (endTool) {
            // 结束补正命名：检查是否已有结束补正
            int endCount = 0;
            for (int i = 0; i < flow->toolCount(); ++i) {
                ITool* t = flow->toolAt(i);
                if (t && t->typeName() == "EndCorrection") endCount++;
            }
            if (endCount == 0) {
                endTool->setInstanceName("结束补正");
            } else {
                endTool->setInstanceName(QString("结束补正_%1").arg(endCount + 1));
            }
            flow->addTool(endTool);
            m_logPanel->appendLog(QString("自动添加: %1").arg(endTool->instanceName()));
        }
    }
    
    m_flowEditor->setFlow(flow);
    m_flowEditor->refresh();
    m_logPanel->appendLog(QString("已添加工具: %1").arg(tool->instanceName()));
    m_statusLabel->setText(QString("已添加: %1").arg(tool->displayName()));
}

void MainWindow::onEditToolProperties(int index) {
    if (m_flowEngine->flowCount() == 0) return;
    Flow* flow = m_flowEngine->flows().first();
    ITool* tool = flow->toolAt(index);
    if (!tool) return;

    // 可用作输入图像的前序工具 + 可链接数据(工具实例名 -> 上次运行的结果键)
    QStringList availableImages;
    QMap<QString, QStringList> linkableData;
    for (int i = 0; i < index && i < flow->toolCount(); ++i) {
        ITool* t = flow->toolAt(i);
        if (!t || !t->isActive()) continue;
        availableImages << t->instanceName();
        const QStringList keys = t->resultData().keys();
        if (!keys.isEmpty())
            linkableData[t->instanceName()] = keys;
    }

    PropertyDialog dlg(tool, availableImages, linkableData, m_flowEngine->lastImage(), this);
    if (dlg.exec() == QDialog::Accepted) {
        if (m_flowEditor) m_flowEditor->refresh();
        m_projectMgr->markModified();
        m_statusLabel->setText(QString("已修改工具: %1").arg(tool->instanceName()));
    }
}

void MainWindow::toggleFullscreen() {
    if (isFullScreen()) showNormal(); else showFullScreen();
}

// ============================================================
// Zoom Controls（参考CKvsRUNCtrl）
// ============================================================

void MainWindow::onZoomIn() {
    if (m_displayArea) m_displayArea->zoomIn();
}

void MainWindow::onZoomOut() {
    if (m_displayArea) m_displayArea->zoomOut();
}

void MainWindow::onZoomFit() {
    if (m_displayArea) m_displayArea->zoomFit();
}

void MainWindow::onZoom1x1() {
    if (m_displayArea) m_displayArea->zoom1x1();
}

// ============================================================
// Menu Actions
// ============================================================

void MainWindow::onNewProject() {
    m_projectMgr->newProject();
    if (m_flowEditor) {
        m_flowEditor->setFlow(nullptr);
        m_flowEditor->refresh();
    }
    m_fileLabel->setText("无项目");
    m_statusLabel->setText("新项目已创建");
}

void MainWindow::onSaveAnnotatedImage() {
    ImageViewWidget* view = m_multiView->viewAt(0);
    if (!view) return;
    const QImage annotated = view->renderAnnotated();
    if (annotated.isNull()) {
        QMessageBox::information(this, "保存画面", "当前没有图像可保存");
        return;
    }
    const QString defPath = QSettings("VisionInspector", "VisionInspector")
                                .value("imageSaveDir",
                                       QCoreApplication::applicationDirPath() + "/images")
                                .toString()
                            + "/annotated_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".png";
    QString path = QFileDialog::getSaveFileName(this, "保存当前画面(含检测标注)", defPath,
                                                "PNG图像 (*.png);;JPEG图像 (*.jpg)");
    if (path.isEmpty()) return;
    if (annotated.save(path))
        m_statusLabel->setText("已保存: " + QFileInfo(path).fileName());
    else
        QMessageBox::warning(this, "错误", "保存失败: " + path);
}

void MainWindow::onNewFromTemplate() {
    // 按真实筛选机的标准工序建流程(蓝本见 docs/CKVision资料分析.md):
    // 采集→预处理→定位→补正→检测组→结束补正→变量→判断→显示
    m_projectMgr->newProject();

    static const QStringList seq = {
        "CaptureImage",        "ImageFilter",   "ShapeMatch",
        "PositionCorrection",  "BlobAnalysis",  "VertexDetection",
        "EdgeDetection",       "DistanceMeasure","LineDetection",
        "CircleDetection",     "Caliper",
        "ThreadInspection",
        "EndCorrection",       "CalculateVariable", "SetVariable",
        "DataJudge",           "DataDisplay",   "UpdateView",
    };

    Flow* flow = new Flow(this);
    flow->setName("主流程");
    m_flowEngine->addFlow(flow);

    QMap<QString, int> counts;
    int created = 0;
    for (const QString& typeName : seq) {
        ITool* tool = ToolRegistry::instance().createTool(typeName);
        if (!tool) {
            m_logPanel->appendLog(QString("模板工具缺失, 已跳过: %1").arg(typeName));
            continue;
        }
        const QString disp = tool->displayName();
        const int n = counts[disp]++;
        tool->setInstanceName(n == 0 ? disp : QString("%1_%2").arg(disp).arg(n + 1));
        flow->addTool(tool);
        ++created;
    }

    if (m_flowEditor) {
        m_flowEditor->setFlow(flow);
        m_flowEditor->refresh();
    }
    m_fileLabel->setText("筛选机模板(未保存)");
    m_statusLabel->setText(QString("已按筛选机模板创建流程 (%1 步)").arg(created));
    m_logPanel->appendLog("已按筛选机模板创建流程: 采集→预处理→定位→补正→检测组→结束补正→变量→判断→显示");
    m_projectMgr->markModified();
}

void MainWindow::onOpenProject() {
    QString path = QFileDialog::getOpenFileName(this, "打开项目", QString(), "*.vipj");
    if (path.isEmpty()) return;
    if (m_projectMgr->loadProject(path)) {
        // 编辑器定位到第一个流程
        if (m_flowEditor && m_flowEngine->flowCount() > 0) {
            m_flowEditor->setFlow(m_flowEngine->flows().first());
            m_flowEditor->refresh();
        }
        m_fileLabel->setText(QFileInfo(path).fileName());
        m_statusLabel->setText("已加载: " + QFileInfo(path).fileName());
        m_logPanel->appendLog("项目已加载: " + path);
    } else {
        QMessageBox::warning(this, "错误", "无法打开项目:\n" + path);
    }
}

void MainWindow::onSaveProject() {
    if (m_projectMgr->currentPath().isEmpty()) { onSaveAsProject(); return; }
    if (m_projectMgr->saveProject(m_projectMgr->currentPath())) {
        m_fileLabel->setText(QFileInfo(m_projectMgr->currentPath()).fileName());
        m_statusLabel->setText("已保存");
        m_logPanel->appendLog("项目已保存: " + m_projectMgr->currentPath());
    } else {
        QMessageBox::warning(this, "错误", "保存失败");
    }
}

void MainWindow::onSaveAsProject() {
    QString path = QFileDialog::getSaveFileName(this, "另存为", QString(), "*.vipj");
    if (path.isEmpty()) return;
    if (!path.endsWith(".vipj")) path += ".vipj";
    if (m_projectMgr->saveProject(path)) {
        m_fileLabel->setText(QFileInfo(path).fileName());
        m_statusLabel->setText("已保存: " + QFileInfo(path).fileName());
        m_logPanel->appendLog("项目已保存: " + path);
    } else {
        QMessageBox::warning(this, "错误", "保存失败");
    }
}

void MainWindow::onExecuteOnce() {
    if (m_flowEngine->isExecuting()) {
        m_statusLabel->setText("正在执行中, 请稍候...");
        return;
    }
    m_statusLabel->setText("执行中...");
    // 异步执行: 算法在工作线程跑, UI不卡
    m_flowEngine->executeOnceAsync(nullptr);
}

void MainWindow::onStartRunning() {
    m_flowEngine->startRunning(nullptr);
    m_statusLabel->setText("运行中...");
}

void MainWindow::onStopRunning() {
    m_flowEngine->stopRunning();
    m_statusLabel->setText("已停止");
}

void MainWindow::onSystemSettings() {
    SystemSettingsDialog dlg(this);
    dlg.exec();
}

void MainWindow::onProjectSettings() {
    ProjectSettingsDialog dlg(m_projectMgr->projectName(), m_projectMgr->projectNote(), this);
    if (dlg.exec() == QDialog::Accepted) {
        m_projectMgr->setProjectName(dlg.projectName());
        m_projectMgr->setProjectNote(dlg.projectNote());
        m_projectMgr->markModified();
        m_statusLabel->setText("项目设置已更新(保存项目后生效)");
    }
}

void MainWindow::onUISettings() {
    QMessageBox::information(this, "界面编辑", "待实现");
}

void MainWindow::onGlobalVariables() {
    GlobalVariablesDialog dlg(m_globalVars, this);
    dlg.exec();
}

void MainWindow::onAnnotationTool() {
    auto* dlg = new AnnotationDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void MainWindow::onPlcSimulator() {
    auto* dlg = PlcSimulatorDialog::instance(this);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void MainWindow::onSwitchUser() {
    LoginDialog dlg(m_userMgr, this);
    if (dlg.exec() == QDialog::Accepted) {
        applyUserRole(m_userMgr->currentRole());
        m_statusLabel->setText("已切换用户: " + roleToString(m_userMgr->currentRole()));
    }
}

void MainWindow::applyUserRole(UserRole role) {
    // 操作员: 只运行; 技术员: 调参数; 管理员: 全部
    const bool admin = (role >= UserRole::Admin);
    if (m_toolbox) m_toolbox->setEnabled(admin);
    if (m_flowEditor) m_flowEditor->setEnabled(admin);
    m_userLabel->setText("用户: " + roleToString(role));
}

// ============================================================
// Camera
// ============================================================

void MainWindow::onScanCameras() {
    if (!m_camera) return;
    m_statusLabel->setText("正在扫描...");
    m_logPanel->appendLog("正在扫描相机...");
    QList<CameraInfo> cameras = m_camera->enumerateCameras();
    if (cameras.isEmpty()) {
        m_statusLabel->setText("未找到相机");
        m_logPanel->appendLog("未找到相机，请检查网络连接");
        m_connectionLabel->setText("无相机");
    } else {
        m_logPanel->appendLog(QString("找到 %1 台相机:").arg(cameras.size()));
        for (const auto& cam : cameras) {
            m_logPanel->appendLog(QString("  - %1 %2 (ID: %3)").arg(cam.vendor).arg(cam.model).arg(cam.id));
        }
        m_connectionLabel->setText(QString("%1 台相机").arg(cameras.size()));
        if (cameras.size() == 1) {
            m_statusLabel->setText("正在打开...");
            if (m_camera->openCamera(cameras[0].id)) {
                m_cameraLabel->setText(QString("相机: %1").arg(cameras[0].model));
                m_connectionLabel->setText("已连接");
                m_logPanel->appendLog("相机已打开");
                if (m_camera->startAcquisition()) {
                    m_statusLabel->setText("采集中");
                    m_logPanel->appendLog("已开始采集");
                }
            } else {
                m_statusLabel->setText("打开失败");
                m_logPanel->appendLog("打开相机失败");
            }
        } else {
            QMessageBox::information(this, "扫描结果", QString("找到 %1 台相机").arg(cameras.size()));
        }
    }
}

void MainWindow::onOpenCamera() {
    if (!m_camera) return;
    if (m_camera->isOpen()) {
        m_camera->closeCamera();
        m_cameraLabel->setText("相机: 无");
        m_connectionLabel->setText("未连接");
        m_statusLabel->setText("已关闭");
        m_logPanel->appendLog("相机已关闭");
    } else {
        QList<CameraInfo> cameras = m_camera->enumerateCameras();
        if (cameras.isEmpty()) {
            QMessageBox::warning(this, "没有相机", "未找到相机");
            return;
        }
        if (m_camera->openCamera(cameras[0].id)) {
            m_cameraLabel->setText(QString("相机: %1").arg(cameras[0].model));
            m_connectionLabel->setText("已连接");
            m_logPanel->appendLog(QString("相机已打开: %1").arg(cameras[0].model));
            if (m_camera->startAcquisition()) {
                m_statusLabel->setText("采集中");
                m_logPanel->appendLog("已开始采集");
            }
        }
    }
}

void MainWindow::setCameraDriver(ICameraDriver* cam)
{
    if (m_camera && m_camera->isOpen()) m_camera->closeCamera();
    m_camera = cam;
    if (m_camera) {
        connect(m_camera, &ICameraDriver::imageReceived,
                this, &MainWindow::onCameraImageReceived);
        connect(m_camera, &ICameraDriver::errorOccurred, this,
                [this](const QString& err) {
                    m_logPanel->appendLog(err);
                });
    }
}

void MainWindow::onUseVirtualCamera()
{
    if (qobject_cast<VirtualCamera*>(m_camera)) {
        m_statusLabel->setText("当前已是虚拟相机");
        return;
    }
    setCameraDriver(new VirtualCamera(this));
    m_cameraLabel->setText("相机: 虚拟相机");
    m_connectionLabel->setText("未连接");
    m_statusLabel->setText("已切换到虚拟相机(回放/合成图案)");
    m_logPanel->appendLog("已切换到虚拟相机。将测试图片放到 testdata/virtual_camera/ 目录可获得回放画面");
}

void MainWindow::onCameraImageReceived(const CvImage& image) {
    if (!m_multiView) return;
#ifdef VI_HAS_OPENCV
    if (!image.empty()) {
        QImage img = cvMatToQImage(image);
        m_multiView->setImage(0, img);
        m_multiView->setStatus(0, "OK");
    }
#else
    if (!image.isNull()) {
        m_multiView->setImage(0, image);
        m_multiView->setStatus(0, "OK");
    }
#endif
}

// ============================================================
// Settings
// ============================================================

void MainWindow::loadSettings() {
    QSettings settings("VisionInspector", "VisionInspector");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

void MainWindow::saveSettings() {
    QSettings settings("VisionInspector", "VisionInspector");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_camera) m_camera->closeCamera();
    ModbusTcpMaster::releaseAll();   // 断开所有Modbus连接
    saveSettings();
    event->accept();
}

} // namespace VisionInspector
