#include "MainWindow.h"
#include "../utils/Logger.h"
#include "../engine/ToolRegistry.h"
#include "../engine/DetectionRecorder.h"
#include "../core/ConfigManager.h"
#include "../core/DeviceTemplates.h"
#include "../core/LicenseManager.h"
#include "../ui/DisplayArea.h"
#include "../ui/FlowEditor.h"
#include "../ui/Toolbox.h"
#include "../ui/DataPanel.h"
#include "../ui/widgets/StatsPanel.h"
#include <QTabWidget>
#include "../ui/LogPanel.h"
#include "../ui/PropertyDialog.h"
#include "../ui/widgets/ImageViewWidget.h"
#include "../ui/PlcSimulatorDialog.h"
#include "../ui/SettingsDialogs.h"
#include "../ui/LoginDialog.h"
#include "../ui/AnnotationDialog.h"
#include "../ui/TemplateDialog.h"
#include "../ui/HistoryDialog.h"
#include "../ui/RuntimeUI.h"
#include "../ui/TeachWizard.h"
#include "../ui/YoloTeachWizard.h"
#include "../ui/UIEditor.h"
#include "../ui/IconHelper.h"
#include "../hal/ModbusTcpMaster.h"
#include "../hal/GigECamera.h"
#include "../hal/VirtualCamera.h"
#include "AppSettings.h"

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QStackedWidget>
#include <QSplitter>
#include <QDockWidget>
#include <QLabel>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QInputDialog>
#include <QRegularExpression>
#include <QCloseEvent>
#include <QSettings>
#include <QApplication>
#include <QTimer>
#include <QStyle>
#include <QIcon>
#include <QStandardPaths>

// 过期水印: 非交互覆盖层, 提醒"未授权"但不打断生产 (阶段6授权策略: 不断产, 打标)
// 全局作用域 — 与 MainWindow.h 的前置声明一致
class LicenseWatermark : public QWidget {
public:
    using QWidget::QWidget;
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        QFont f = p.font();
        f.setPixelSize(26);
        f.setBold(true);
        p.setFont(f);
        p.setPen(QColor(255, 64, 64, 88));
        p.drawText(rect().adjusted(0, 0, 0, -18), Qt::AlignHCenter | Qt::AlignBottom,
                   QStringLiteral("未授权版本 — 试用已结束 (帮助→关于 导入授权)"));
        p.setPen(QPen(QColor(255, 64, 64, 110), 5));
        p.drawRect(rect().adjusted(2, 2, -2, -2));
    }
};

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

    // 相机驱动: 默认GigE通用, 用户在相机管理对话框中可选BaslerCamera
    setCameraDriver(new GigECamera(this));

    qDebug() << "MainWindow: About to call setupUI...";
    setupUI();
    qDebug() << "MainWindow: setupUI completed";

    loadSettings();
    resetUndoState(); // 撤销基线: 启动时的空/默认工程状态

    // 阶段6: 检测记录SQLite持久化 (打开失败自动降级为仅内存)
    m_recorder = new DetectionRecorder(this);
    if (m_recorder->open())
        m_logPanel->appendLog(QStringLiteral("检测记录数据库已打开: data/records.db"));
    else
        m_logPanel->appendLog(QStringLiteral("检测记录数据库打开失败, 历史记录不可用"));

    // 阶段6: 授权初始化 (试用期/授权文件)
    LicenseManager::instance().initialize();
    m_licenseLabel->setText(LicenseManager::instance().statusText());
    if (LicenseManager::instance().state() == LicenseManager::State::Expired)
        showLicenseWatermark();

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

            // 统计与记录: 喂给 GlobalStats (状态栏计数/数据面板/CSV报表)
            InspectionRecord rec;
            rec.index = m_stats->totalCount() + 1;
            rec.timestamp = QDateTime::currentDateTime();
            rec.overallOk = allOk;
            rec.isRetest = false;
            const auto toolStates = m_flowEngine->lastToolStates();
            for (const auto& s : toolStates)
                rec.itemResults[s.first] = s.second;
            const DataMap resultData = m_flowEngine->lastResultData();
            for (auto it = resultData.begin(); it != resultData.end(); ++it) {
                bool okNum = false;
                const double v = it.value().toDouble(&okNum);
                if (okNum)
                    rec.values[it.key()] = v;
            }
            m_stats->addRecord(rec);

            // 阶段6: 检测记录落库 (data/records.db, 关机不丢)
            if (m_recorder && m_recorder->isOpen())
                m_recorder->record(rec);

            // UI排查 P0-6: 底部检测项目表按工位显示当前结果
            if (m_dataPanel) {
                int ccd = 0;
                for (const auto& s : toolStates) {
                    if (ccd >= m_dataPanel->rowCount()) break;
                    m_dataPanel->setStationStatus(ccd, s.second, s.first);
                    ++ccd;
                }
            }

            // 阶段6: DIY运行界面同步 (图像/结果表/数值/工位灯)
            if (m_runtimeUI) {
#ifdef VI_HAS_OPENCV
                if (img && !img->empty())
                    m_runtimeUI->setRuntimeImage(cvMatToQImage(*img));
#endif
                m_runtimeUI->updateResults(toolStates, resultData);
                int rtCcd = 0;
                for (const auto& s : toolStates) {
                    m_runtimeUI->setStationStatus(rtCcd, s.second, s.first);
                    if (++rtCcd >= 8) break;
                }
            }

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

    setWindowTitle("LW Vision v1.0.0");
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
    const int camN = ConfigManager::instance().cameraCount();
    const int camCols = camN > 4 ? 4 : camN;
    const int camRows = (camN + camCols - 1) / camCols;
    m_multiView->setLayout(camRows, camCols);
    // 每个图像格的鼠标移动 → 状态栏实时像素坐标 (此前信号存在但从未接线)
    for (int i = 0; i < camRows * camCols; ++i) {
        if (auto* v = m_multiView->viewAt(i)) {
            connect(v, &ImageViewWidget::cursorImagePos, this, [this](const QPointF& pos) {
                if (pos.x() < 0 || pos.y() < 0)
                    m_posLabel->setText(" X: -  Y: - ");
                else
                    m_posLabel->setText(QString(" X: %1  Y: %2 ")
                                            .arg((int)pos.x()).arg((int)pos.y()));
            });
        }
    }
    
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setHandleWidth(6);
    m_mainSplitter->addWidget(m_toolbox);
    m_mainSplitter->addWidget(m_flowEditor);
    m_mainSplitter->addWidget(m_multiView);
    m_mainSplitter->setStretchFactor(0, 1);  // 工具箱
    m_mainSplitter->setStretchFactor(1, 0);  // 流程栏不参与拉伸
    m_mainSplitter->setStretchFactor(2, 1);  // 图像栏拉伸

    // 底部CCD检测项目表 + 良率统计面板(趋势/NG分布/CSV导出)
    m_dataPanel = new DataPanel(this);
    m_dataPanel->setStats(m_stats);
    m_statsPanel = new StatsPanel(this);
    m_statsPanel->setStats(m_stats);

    // 整体垂直分割：上部(工具箱+流程+图像) | 下部(检测项目表)
    m_vSplitter = new QSplitter(Qt::Vertical, this);
    m_vSplitter->setHandleWidth(6);
    m_vSplitter->addWidget(m_mainSplitter);
    // 底部Tab容器: 检测项目表 | 良率统计(趋势/NG分布)
    auto* bottomTabs = new QTabWidget(this);
    bottomTabs->setObjectName("BottomTabs");
    bottomTabs->addTab(m_dataPanel, QString::fromUtf8("检测项目"));
    bottomTabs->addTab(m_statsPanel, QString::fromUtf8("良率统计"));
    m_vSplitter->addWidget(bottomTabs);
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
    connect(m_flowEditor, &FlowEditor::toolDropped, this, &MainWindow::onToolAdded);
    connect(m_flowEditor, &FlowEditor::toolEditProperties, this, &MainWindow::onEditToolProperties);
    // UI排查 P0-2/P0-1: 双击编辑 + 右键7项全接线 (此前仅属性一项有效)
    connect(m_flowEditor, &FlowEditor::toolDoubleClicked, this, &MainWindow::onEditToolProperties);
    connect(m_flowEditor, &FlowEditor::toolToggleActive, this, &MainWindow::onToolToggleActive);
    connect(m_flowEditor, &FlowEditor::toolDelete, this, &MainWindow::onToolDelete);
    connect(m_flowEditor, &FlowEditor::toolMoveUp, this, &MainWindow::onToolMoveUp);
    connect(m_flowEditor, &FlowEditor::toolMoveDown, this, &MainWindow::onToolMoveDown);
    connect(m_flowEditor, &FlowEditor::toolRename, this, &MainWindow::onToolRename);
    connect(m_flowEditor, &FlowEditor::toolCopy, this, &MainWindow::onToolCopy);
    connect(m_flowEditor, &FlowEditor::toolPaste, this, &MainWindow::onToolPaste);
    
    setWindowTitle("LW Vision v1.0.0");
    resize(1400, 900);

    // 延迟设置分割器尺寸，确保布局已完成
    QTimer::singleShot(0, this, [this]() {
        m_mainSplitter->setSizes({200, 160, 1000});
    });
}

void MainWindow::createMenus() {
    QMenu* fileMenu = menuBar()->addMenu(QString::fromUtf8("\u6587\u4ef6(&F)"));
    // UI排查 P0-5: 全应用此前无任何快捷键, 补高频快捷键
    fileMenu->addAction(QString::fromUtf8("\u65b0\u5efa\u9879\u76ee(Ctrl+N)"), this, &MainWindow::onNewProject,
                        QKeySequence::New);
    fileMenu->addAction(QString::fromUtf8("\u4ece\u6a21\u677f\u65b0\u5efa..."), this, &MainWindow::onOpenTemplateGallery);
    fileMenu->addAction(QString::fromUtf8("\u4fdd\u5b58\u4e3a\u6a21\u677f..."), this, &MainWindow::onSaveAsTemplate);
    fileMenu->addAction(QString::fromUtf8("\u6253\u5f00\u9879\u76ee(Ctrl+O)"), this, &MainWindow::onOpenProject,
                        QKeySequence::Open);
    fileMenu->addAction(QString::fromUtf8("\u4fdd\u5b58\u9879\u76ee(Ctrl+S)"), this, &MainWindow::onSaveProject,
                        QKeySequence::Save);
    fileMenu->addAction(QString::fromUtf8("\u53e6\u5b58\u4e3a...(Ctrl+Shift+S)"), this, &MainWindow::onSaveAsProject,
                        QKeySequence::SaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction(QString::fromUtf8("\u4fdd\u5b58\u5f53\u524d\u753b\u9762(\u542b\u6807\u6ce8)"), this, &MainWindow::onSaveAnnotatedImage);
    fileMenu->addSeparator();
    fileMenu->addAction(QString::fromUtf8("\u9000\u51fa"), qApp, &QApplication::quit);

    QMenu* setMenu = menuBar()->addMenu(QString::fromUtf8("\u8bbe\u7f6e(&S)"));
    setMenu->addAction(QString::fromUtf8("\u7cfb\u7edf\u8bbe\u7f6e"), this, &MainWindow::onSystemSettings);
    setMenu->addAction(QString::fromUtf8("\u9879\u76ee\u8bbe\u7f6e"), this, &MainWindow::onProjectSettings);
    setMenu->addAction(QString::fromUtf8("\u5168\u5c40\u53d8\u91cf"), this, &MainWindow::onGlobalVariables);
    setMenu->addAction(QString::fromUtf8("界面设置(DIY)"), this, &MainWindow::onUISettings);
    setMenu->addSeparator();
    setMenu->addAction(QString::fromUtf8("PLC\u6a21\u62df\u5668..."), this, &MainWindow::onPlcSimulator);

    QMenu* annoMenu = menuBar()->addMenu(QString::fromUtf8("\u6807\u6ce8(&A)"));
    annoMenu->addAction(QString::fromUtf8("\u7f3a\u9677\u6807\u6ce8\u5de5\u5177..."), this, &MainWindow::onAnnotationTool);

    QMenu* aiMenu = menuBar()->addMenu(QString::fromUtf8("AI(&I)"));
    aiMenu->addAction(QString::fromUtf8("教导向导(多工位AI缺陷检测)..."), this, &MainWindow::onTeachWizard);
    aiMenu->addAction(QString::fromUtf8("YOLO检测教导向导..."), this, &MainWindow::onYoloTeachWizard);

    QMenu* camMenu = menuBar()->addMenu(QString::fromUtf8("\u76f8\u673a(&C)"));
    camMenu->addAction(QString::fromUtf8("\u76f8\u673a\u7ba1\u7406 (CCD1~8)"), this, &MainWindow::onScanCameras);
    camMenu->addAction(QString::fromUtf8("\u4f7f\u7528\u865a\u62df\u76f8\u673a"), this, &MainWindow::onUseVirtualCamera);

    QMenu* opMenu = menuBar()->addMenu(QString::fromUtf8("\u64cd\u4f5c(&O)"));
    // 撤销/重做: 工程级快照 (误删工具/调错参数一键回退)
    opMenu->addAction(QStringLiteral("撤销(Ctrl+Z)"), this, &MainWindow::onUndo,
                      QKeySequence::Undo);
    opMenu->addAction(QStringLiteral("重做(Ctrl+Y)"), this, &MainWindow::onRedo,
                      QKeySequence::Redo);
    opMenu->addSeparator();
    // UI排查 P0-5: F5单次执行/F6连续运行/F7停止
    opMenu->addAction(QString::fromUtf8("\u6267\u884c\u7a0b\u5e8f(F5)"), this, &MainWindow::onExecuteOnce,
                      QKeySequence(Qt::Key_F5));
    opMenu->addAction(QString::fromUtf8("\u8fd0\u884c\u7a0b\u5e8f(F6)"), this, &MainWindow::onStartRunning,
                      QKeySequence(Qt::Key_F6));
    opMenu->addAction(QString::fromUtf8("\u505c\u6b62\u7a0b\u5e8f(F7)"), this, &MainWindow::onStopRunning,
                      QKeySequence(Qt::Key_F7));

    QMenu* dataMenu = menuBar()->addMenu(QString::fromUtf8("\u6570\u636e(&D)"));
    dataMenu->addAction(QString::fromUtf8("\u5bfc\u51fa\u68c0\u6d4b\u8bb0\u5f55(CSV)..."), this, &MainWindow::onExportCsv);
    // 阶段6: SQLite持久化历史记录查询
    dataMenu->addAction(QStringLiteral("历史记录查询..."), this, &MainWindow::onHistoryQuery);

    // View菜单会在createDockWidgets之后添加
}

void MainWindow::createViewMenu() {
    QMenu* viewMenu = menuBar()->addMenu(QString::fromUtf8("\u89c6\u56fe(&V)"));
    viewMenu->addAction(m_mainToolBar->toggleViewAction());
    viewMenu->addAction(m_logDock->toggleViewAction());
    viewMenu->addSeparator();
    // UI排查 P0-4: 缩放入口 (此前缩放槽无任何UI连接, 完全失效)
    viewMenu->addAction(QString::fromUtf8("\u653e\u5927(Ctrl++)"), this, &MainWindow::onZoomIn,
                        QKeySequence::ZoomIn);
    viewMenu->addAction(QString::fromUtf8("\u7f29\u5c0f(Ctrl+-)"), this, &MainWindow::onZoomOut,
                        QKeySequence::ZoomOut);
    viewMenu->addAction(QString::fromUtf8("\u9002\u5e94\u7a97\u53e3"), this, &MainWindow::onZoomFit);
    viewMenu->addAction(QString::fromUtf8("1:1 \u539f\u5927"), this, &MainWindow::onZoom1x1);
    viewMenu->addSeparator();
    viewMenu->addAction(QString::fromUtf8("\u5168\u5c4f\u663e\u793a"), this, &MainWindow::toggleFullscreen);

    // 阶段6: DIY运行界面 (布局在 界面设置(DIY) 编辑, 默认存 config/runtime_ui.json)
    QAction* runtimeAct = viewMenu->addAction(QStringLiteral("运行界面(DIY布局)"));
    runtimeAct->setCheckable(true);
    connect(runtimeAct, &QAction::toggled, this, &MainWindow::onToggleRuntimeUI);

    // 帮助菜单 (放在最后)
    QMenu* helpMenu = menuBar()->addMenu(QString::fromUtf8("\u5e2e\u52a9(&H)"));
    helpMenu->addAction(QString::fromUtf8("\u5173\u4e8e(&A)..."), this, &MainWindow::onAbout);
}

void MainWindow::onAbout() {
    QString cvVer = QStringLiteral("未启用");
#ifdef VI_HAS_OPENCV
    cvVer = QString::fromLatin1(CV_VERSION);
#endif
    const LicenseManager& lic = LicenseManager::instance();
    const QString html = QStringLiteral(
        "<h3 style='margin-bottom:2px;'>LW Vision</h3>"
        "<p style='margin-top:2px;'>通用工业机器视觉检测平台<br/>"
        "传统视觉算法 + AI 深度学习相结合</p>"
        "<hr/>"
        "<p>"
        "版本号：v%1<br/>"
        "开发语言：C++17<br/>"
        "界面框架：Qt %2（MinGW）<br/>"
        "视觉算法库：OpenCV %3<br/>"
        "AI 推理引擎：ONNX Runtime 1.18.1（CPU）"
        "</p>"
        "<hr/>"
        "<p style='color:#4a9eff;'>授权状态：%4</p>"
        "<p style='color:#888;'>LW Vision · 学习研究用途</p>")
        .arg(versionString())
        .arg(QStringLiteral(QT_VERSION_STR))
        .arg(cvVer)
        .arg(lic.statusText().toHtmlEscaped());

    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("关于 LW Vision"));
    box.setTextFormat(Qt::RichText);
    box.setText(html);
    QPushButton* importBtn = box.addButton(QStringLiteral("导入授权文件..."),
                                           QMessageBox::ActionRole);
    box.addButton(QStringLiteral("确定"), QMessageBox::AcceptRole);
    box.exec();

    if (box.clickedButton() == importBtn) {
        const QString f = QFileDialog::getOpenFileName(
            this, QStringLiteral("导入授权文件"), QString(),
            QStringLiteral("授权文件 (*.lic);;所有文件 (*)"));
        if (f.isEmpty()) return;
        const QString err = LicenseManager::instance().importLicense(f);
        if (err.isEmpty()) {
            m_licenseLabel->setText(LicenseManager::instance().statusText());
            if (LicenseManager::instance().state() == LicenseManager::State::Licensed
                && m_licenseWatermark) {
                m_licenseWatermark->deleteLater();
                m_licenseWatermark = nullptr;
            }
            m_logPanel->appendLog(QStringLiteral("授权已更新: %1")
                                      .arg(LicenseManager::instance().statusText()));
            QMessageBox::information(this, QStringLiteral("导入授权"),
                                     QStringLiteral("授权导入成功！\n%1")
                                         .arg(LicenseManager::instance().statusText()));
        } else {
            QMessageBox::warning(this, QStringLiteral("导入授权"), err);
        }
    }
}

void MainWindow::onExportCsv() {
    if (m_stats->historyCount() == 0) {
        QMessageBox::information(this, QStringLiteral("导出检测记录"),
                                 QStringLiteral("暂无检测记录可导出。\n请先执行一次检测(操作→执行程序)。"));
        return;
    }
    const QString def = QStringLiteral("%1/检测记录_%2.csv")
        .arg(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出检测记录"), def, QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;

    QString err;
    if (m_stats->exportCsv(path, &err)) {
        m_logPanel->appendLog(QStringLiteral("检测记录已导出: ") + path);
        QMessageBox::information(this, QStringLiteral("导出检测记录"),
                                 QStringLiteral("已导出 %1 条记录到:\n%2")
                                     .arg(m_stats->historyCount())
                                     .arg(path));
    } else {
        QMessageBox::warning(this, QStringLiteral("导出检测记录"), err);
    }
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

    act = m_mainToolBar->addAction(IconHelper::cameraIcon(), QString::fromUtf8("\u76f8\u673a\u7ba1\u7406"));
    connect(act, &QAction::triggered, this, &MainWindow::onScanCameras);

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
    m_posLabel = new QLabel(" X: -  Y: - ");
    m_licenseLabel = new QLabel("");

    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_posLabel);
    statusBar()->addPermanentWidget(m_licenseLabel);
    statusBar()->addPermanentWidget(m_userLabel);
    statusBar()->addPermanentWidget(m_connectionLabel);
    statusBar()->addPermanentWidget(m_fileLabel);
    statusBar()->addPermanentWidget(m_okCountLabel);
    statusBar()->addPermanentWidget(m_ngCountLabel);
    statusBar()->addPermanentWidget(m_yieldLabel);
    statusBar()->addPermanentWidget(m_cameraLabel);

    // 状态栏计数随统计更新 (修复: 之前计数从不更新)
    connect(m_stats, &GlobalStats::statsChanged, this, [this]() {
        m_okCountLabel->setText(QString("OK: %1").arg(m_stats->passCount()));
        m_ngCountLabel->setText(QString("NG: %1").arg(m_stats->failCount()));
        m_yieldLabel->setText(QStringLiteral("良率: %1%").arg(m_stats->yieldRate(), 0, 'f', 1));
    });
}

void MainWindow::onToolAdded(const QString& typeName) {
    ITool* tool = ToolRegistry::instance().createTool(typeName);
    if (!tool) {
        m_logPanel->appendLog(QString("无法创建工具: %1").arg(typeName));
        return;
    }
    
    Flow* flow = nullptr;
    if (m_flowEngine->flowCount() > 0) {
        flow = m_flowEngine->flows().first();
    } else {
        // H-6: 所有权统一归引擎 (与 ProjectManager 一致), 避免关窗双重释放
        flow = new Flow(m_flowEngine);
        flow->setName("主流程");
        m_flowEngine->addFlow(flow);
    }
    
    // 空流程首次添加"需要图像"的工具时, 自动在最前面补一个"采集图像"
    // (对齐CKVision: 流程总是从图像源开始, 避免"检测直线"这类工具因无输入图像用不了)
    if (flow->toolCount() == 0 && typeName != "CaptureImage"
        && ITool::needsInputImage(typeName)) {
        ITool* cap = ToolRegistry::instance().createTool("CaptureImage");
        if (cap) {
            cap->setInstanceName(cap->displayName());
            flow->addTool(cap);
            m_logPanel->appendLog("已自动添加图像源: 采集图像 (检测/测量工具需要输入图像)");
        }
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
    const int newIndex = flow->toolCount() - 1;
    
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
    pushUndoState();

    // 添加后自动打开属性框, 让用户立刻知道怎么配置(双击节点也可再次打开)
    if (newIndex >= 0 && newIndex < flow->toolCount()) {
        onEditToolProperties(newIndex);
    }
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
        pushUndoState();
    }
}

// ============================================================
// 流程编辑器右键/双击/Delete 信号接线 (UI排查 P0-1~P0-3)
// ============================================================

void MainWindow::onToolToggleActive(int index) {
    if (m_flowEngine->flowCount() == 0) return;
    Flow* flow = m_flowEngine->flows().first();
    ITool* tool = flow->toolAt(index);
    if (!tool) return;
    tool->setActive(!tool->isActive());
    if (m_flowEditor) {
        m_flowEditor->updateToolStatus(index, tool->isActive() ? ToolStatus::Idle : ToolStatus::Disabled);
        m_flowEditor->refresh();
    }
    m_projectMgr->markModified();
    pushUndoState();
    m_statusLabel->setText(QString("%1: %2").arg(tool->instanceName())
                               .arg(tool->isActive() ? "已启用" : "已禁用"));
}

void MainWindow::onToolDelete(int index) {
    if (m_flowEngine->flowCount() == 0) return;
    Flow* flow = m_flowEngine->flows().first();
    if (index < 0 || index >= flow->toolCount()) return;
    // 引擎已在 UI 线程 (不连续运行时); 安全起见若在运行先停
    if (m_flowEngine->isRunning()) m_flowEngine->stopRunning(true);
    ITool* tool = flow->toolAt(index);
    const QString name = tool ? tool->instanceName() : QString::number(index);
    flow->removeTool(index);   // Flow 拥有工具, removeTool 即删除
    if (m_flowEditor) m_flowEditor->refresh();
    m_projectMgr->markModified();
    pushUndoState();
    m_statusLabel->setText(QString("已删除: %1").arg(name));
}

void MainWindow::onToolMoveUp(int index) {
    if (index <= 0) return;
    if (m_flowEngine->flowCount() == 0) return;
    Flow* flow = m_flowEngine->flows().first();
    flow->moveTool(index, index - 1);
    if (m_flowEditor) m_flowEditor->refresh();
    m_projectMgr->markModified();
    pushUndoState();
}

void MainWindow::onToolMoveDown(int index) {
    if (m_flowEngine->flowCount() == 0) return;
    Flow* flow = m_flowEngine->flows().first();
    if (index < 0 || index >= flow->toolCount() - 1) return;
    flow->moveTool(index, index + 1);
    if (m_flowEditor) m_flowEditor->refresh();
    m_projectMgr->markModified();
    pushUndoState();
}

void MainWindow::onToolRename(int index, const QString& newName) {
    if (m_flowEngine->flowCount() == 0) return;
    Flow* flow = m_flowEngine->flows().first();
    ITool* tool = flow->toolAt(index);
    if (!tool) return;
    tool->setInstanceName(newName);
    if (m_flowEditor) m_flowEditor->refresh();
    m_projectMgr->markModified();
    pushUndoState();
    m_statusLabel->setText(QString("已重命名为: %1").arg(newName));
}

void MainWindow::onToolCopy(int index) {
    if (m_flowEngine->flowCount() == 0) return;
    Flow* flow = m_flowEngine->flows().first();
    ITool* tool = flow->toolAt(index);
    if (!tool) return;
    // 复制完整工具(类型+参数+判定), 粘贴时还原 — 调好参数再复制是高频操作
    if (m_flowEditor) m_flowEditor->setClipboardTool(tool->typeName(), tool->toJson());
    m_statusLabel->setText(QString("已复制: %1 (含参数)").arg(tool->instanceName()));
}

void MainWindow::onToolPaste(int index) {
    if (!m_flowEditor || m_flowEditor->clipboardTypeName().isEmpty()) return;
    const QString typeName = m_flowEditor->clipboardTypeName();
    ITool* tool = ToolRegistry::instance().createTool(typeName);
    if (!tool) {
        m_logPanel->appendLog(QString("无法创建工具: %1").arg(typeName));
        return;
    }
    // 还原剪贴板中的参数/判定/注释 (空JSON则保持默认参数)
    const QJsonObject clipJson = m_flowEditor->clipboardToolJson();
    if (!clipJson.isEmpty())
        tool->fromJson(clipJson);

    Flow* flow = nullptr;
    if (m_flowEngine->flowCount() > 0) {
        flow = m_flowEngine->flows().first();
    } else {
        flow = new Flow(m_flowEngine);
        flow->setName("主流程");
        m_flowEngine->addFlow(flow);
    }

    // 粘贴到指定位置 (index<0 表示末尾)
    if (index >= 0 && index <= flow->toolCount()) {
        flow->insertTool(index, tool);
    } else {
        flow->addTool(tool);
    }

    // 命名: 沿用复制的名字(fromJson已还原), 与流程内既有工具重名时加 _N 后缀
    QString baseName = tool->instanceName().isEmpty() ? tool->displayName()
                                                      : tool->instanceName();
    auto nameExists = [&](const QString& n) {
        for (int i = 0; i < flow->toolCount(); ++i) {
            ITool* t = flow->toolAt(i);
            if (t && t != tool && t->instanceName() == n) return true;
        }
        return false;
    };
    if (nameExists(baseName)) {
        int n = 2;
        while (nameExists(QString("%1_%2").arg(baseName).arg(n))) ++n;
        baseName = QString("%1_%2").arg(baseName).arg(n);
    }
    tool->setInstanceName(baseName);

    m_flowEditor->setFlow(flow);
    m_flowEditor->refresh();
    m_projectMgr->markModified();
    pushUndoState();
    m_logPanel->appendLog(QString("已粘贴工具: %1").arg(tool->instanceName()));
}

// ============================================================
// 撤销/重做 (工程级快照: 误删工具/调错参数一键回退)
// ============================================================

void MainWindow::resetUndoState() {
    m_undoStates.clear();
    m_undoStates.append(m_projectMgr ? m_projectMgr->currentStateJson() : QJsonObject());
    m_undoIndex = 0;
}

void MainWindow::pushUndoState() {
    if (!m_projectMgr) return;
    const QJsonObject st = m_projectMgr->currentStateJson();
    if (m_undoIndex >= 0 && m_undoIndex < m_undoStates.size() &&
        m_undoStates[m_undoIndex] == st)
        return; // 状态无变化不重复入栈
    m_undoStates.resize(m_undoIndex + 1); // 丢弃重做分支
    m_undoStates.append(st);
    if (m_undoStates.size() > 50) {
        m_undoStates.removeFirst(); // 上限50步, 防大工程撑内存
        --m_undoIndex;
    }
    m_undoIndex = m_undoStates.size() - 1;
}

bool MainWindow::applyUndoState(const QJsonObject& st) {
    if (!m_projectMgr->restoreStateJson(st)) return false;
    if (m_flowEditor) {
        m_flowEditor->setFlow(m_flowEngine->flowCount() > 0
                                  ? m_flowEngine->flows().first() : nullptr);
        m_flowEditor->refresh();
    }
    return true;
}

void MainWindow::onUndo() {
    if (m_flowEngine->isRunning()) {
        m_statusLabel->setText(QStringLiteral("运行中不能撤销, 请先停止(F7)"));
        return;
    }
    if (m_undoIndex <= 0) {
        m_statusLabel->setText(QStringLiteral("没有可撤销的操作"));
        return;
    }
    --m_undoIndex;
    if (applyUndoState(m_undoStates[m_undoIndex]))
        m_statusLabel->setText(QStringLiteral("已撤销 (%1/%2)")
                                   .arg(m_undoIndex).arg(m_undoStates.size() - 1));
}

void MainWindow::onRedo() {
    if (m_flowEngine->isRunning()) return;
    if (m_undoIndex >= m_undoStates.size() - 1) {
        m_statusLabel->setText(QStringLiteral("没有可重做的操作"));
        return;
    }
    ++m_undoIndex;
    if (applyUndoState(m_undoStates[m_undoIndex]))
        m_statusLabel->setText(QStringLiteral("已重做 (%1/%2)")
                                   .arg(m_undoIndex).arg(m_undoStates.size() - 1));
}

void MainWindow::toggleFullscreen() {
    if (isFullScreen()) showNormal(); else showFullScreen();
}

// ============================================================
// Zoom Controls（参考CKvsRUNCtrl）
// ============================================================

void MainWindow::onZoomIn() {
    // UI排查 P0-4: m_displayArea 从未实例化(死成员), 改作用于实际图像控件
    if (m_multiView) {
        auto* v = m_multiView->viewAt(0);
        if (v) v->zoomIn();
    }
}

void MainWindow::onZoomOut() {
    if (m_multiView) {
        auto* v = m_multiView->viewAt(0);
        if (v) v->zoomOut();
    }
}

void MainWindow::onZoomFit() {
    if (m_multiView) {
        auto* v = m_multiView->viewAt(0);
        if (v) v->zoomFit();
    }
}

void MainWindow::onZoom1x1() {
    if (m_multiView) {
        auto* v = m_multiView->viewAt(0);
        if (v) v->zoom1x1();
    }
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
    resetUndoState();
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
    // 按真实筛选机的标准工序建流程(蓝本见 docs/CKVision资料分析.md), 定义在 DeviceTemplates
    m_projectMgr->newProject();

    int created = 0;
    if (!DeviceTemplates::build(QStringLiteral("sifter"), m_flowEngine, &created)) {
        m_logPanel->appendLog("筛选机模板构建失败");
        return;
    }

    if (m_flowEditor) {
        m_flowEditor->setFlow(m_flowEngine->flowCount() > 0 ? m_flowEngine->flows().first() : nullptr);
        m_flowEditor->refresh();
    }
    m_fileLabel->setText("筛选机模板(未保存)");
    m_statusLabel->setText(QString("已按筛选机模板创建流程 (%1 步)").arg(created));
    m_logPanel->appendLog("已按筛选机模板创建流程: 采集→预处理→定位→补正→检测组→结束补正→变量→判断→显示");
    m_projectMgr->markModified();
    resetUndoState();
}

// 阶段6 设备模板系统: 模板库画廊 + 存为模板
void MainWindow::onOpenTemplateGallery() {
    TemplateDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    const TemplateDialog::Entry entry = dlg.selected();
    if (entry.title.isEmpty()) return;

    if (entry.path.startsWith(QStringLiteral("builtin:"))) {
        // 内置设备模板 (筛选机/贴标/计数/木业)
        const QString tplId = entry.path.mid(QStringLiteral("builtin:").size());
        int created = 0;
        m_projectMgr->newProject();
        if (!DeviceTemplates::build(tplId, m_flowEngine, &created)) {
            QMessageBox::warning(this, QStringLiteral("模板"), QStringLiteral("内置模板构建失败"));
            return;
        }
        if (m_flowEditor) {
            m_flowEditor->setFlow(m_flowEngine->flowCount() > 0 ? m_flowEngine->flows().first() : nullptr);
            m_flowEditor->refresh();
        }
        resetUndoState();
        m_projectMgr->markModified();
        m_fileLabel->setText(QStringLiteral("模板新建(未保存)"));
        m_statusLabel->setText(QStringLiteral("已从模板创建: %1 (%2个工具)").arg(entry.title).arg(created));
        m_logPanel->appendLog(QStringLiteral("已从内置模板[%1]创建工程 (%2个工具)")
                                  .arg(entry.title).arg(created));
        return;
    }
    if (!m_projectMgr->loadTemplate(entry.path)) {
        QMessageBox::warning(this, QStringLiteral("模板"), QStringLiteral("模板文件无法加载:\n%1").arg(entry.path));
        return;
    }
    if (m_flowEditor) {
        m_flowEditor->setFlow(m_flowEngine->flowCount() > 0 ? m_flowEngine->flows().first() : nullptr);
        m_flowEditor->refresh();
    }
    resetUndoState();
    m_fileLabel->setText(QStringLiteral("模板新建(未保存)"));
    m_statusLabel->setText(QStringLiteral("已从模板创建: %1").arg(entry.title));
    m_logPanel->appendLog(QStringLiteral("已从模板[%1]创建工程 (%2个工具)")
                              .arg(entry.title).arg(entry.toolCount));
}

void MainWindow::onSaveAsTemplate() {
    if (m_flowEngine->flowCount() == 0 || m_flowEngine->flows().first()->toolCount() == 0) {
        QMessageBox::information(this, QStringLiteral("保存为模板"),
                                 QStringLiteral("当前工程没有工具, 没有可保存的模板内容。"));
        return;
    }
    const QString defName = m_projectMgr->projectName().isEmpty()
                                ? QStringLiteral("我的模板") : m_projectMgr->projectName();
    QString name = QInputDialog::getText(this, QStringLiteral("保存为模板"),
        QStringLiteral("模板名称 (将保存到 templates/ 目录):"), QLineEdit::Normal, defName);
    name = name.trimmed();
    if (name.isEmpty()) return;
    // 文件名安全: 替换路径非法字符
    name.replace(QRegularExpression(R"([\\/:*?"<>|])"), QStringLiteral("_"));

    QDir().mkpath(ProjectManager::templateDir());
    const QString path = ProjectManager::templateDir() + QStringLiteral("/%1.vipj").arg(name);
    if (QFileInfo::exists(path)) {
        if (QMessageBox::question(this, QStringLiteral("保存为模板"),
                QStringLiteral("模板[%1]已存在, 覆盖?").arg(name)) != QMessageBox::Yes)
            return;
    }
    if (m_projectMgr->saveTemplate(path)) {
        m_statusLabel->setText(QStringLiteral("已保存模板: %1").arg(name));
        m_logPanel->appendLog(QStringLiteral("工程已存为模板: %1").arg(path));
    } else {
        QMessageBox::warning(this, QStringLiteral("保存为模板"), QStringLiteral("保存失败: %1").arg(path));
    }
}

// 阶段6: 检测记录历史查询 (SQLite持久化)
void MainWindow::onHistoryQuery() {
    HistoryDialog dlg(m_recorder, this);
    dlg.exec();
}

// ── 授权: 过期水印 + 状态同步 ──

void MainWindow::showLicenseWatermark() {
    if (!m_licenseWatermark && centralWidget()) {
        m_licenseWatermark = new LicenseWatermark(centralWidget());
        m_licenseWatermark->setAttribute(Qt::WA_TransparentForMouseEvents);
        centralWidget()->installEventFilter(this);
        m_licenseWatermark->setGeometry(centralWidget()->rect());
        m_licenseWatermark->raise();
        m_licenseWatermark->show();
    }
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    // 水印跟随中央控件尺寸 (窗口缩放/分栏拖动/界面切换)
    if (m_licenseWatermark && obj == centralWidget() && event->type() == QEvent::Resize)
        m_licenseWatermark->setGeometry(centralWidget()->rect());
    return QMainWindow::eventFilter(obj, event);
}

// 阶段6: DIY运行界面 (标准界面 ↔ 编辑器保存的布局 切换)
void MainWindow::onToggleRuntimeUI(bool on) {
    // 首次切换: 用 QStackedWidget 包住现有中央控件, 标准界面保持原样可随时切回
    if (!m_centralStack) {
        m_centralStack = new QStackedWidget(this);
        m_centralStack->addWidget(m_vSplitter); // index 0 = 标准界面
        setCentralWidget(m_centralStack);
    }
    if (!on) {
        m_centralStack->setCurrentIndex(0);
        m_statusLabel->setText(QStringLiteral("已切回标准界面"));
        return;
    }

    const QString layoutPath = QCoreApplication::applicationDirPath()
                               + QStringLiteral("/config/runtime_ui.json");
    if (!QFileInfo::exists(layoutPath)) {
        QMessageBox::information(this, QStringLiteral("运行界面"),
            QStringLiteral("尚未保存DIY布局。\n\n请先通过 设置→界面设置(DIY) 拖拽控件,"
                           "保存时选择默认路径 (config/runtime_ui.json), 再切换到运行界面。"));
        if (auto* act = qobject_cast<QAction*>(sender()))
            act->setChecked(false);
        return;
    }

    if (m_runtimeUI) {
        m_runtimeUI->deleteLater();
        m_runtimeUI = nullptr;
    }
    m_runtimeUI = new RuntimeUI(this);
    if (!m_runtimeUI->loadLayout(layoutPath)) {
        QMessageBox::warning(this, QStringLiteral("运行界面"),
                             QStringLiteral("布局文件解析失败:\n%1").arg(layoutPath));
        m_runtimeUI->deleteLater();
        m_runtimeUI = nullptr;
        if (sender()) static_cast<QAction*>(sender())->setChecked(false);
        return;
    }
    connect(m_runtimeUI, &RuntimeUI::actionRequested, this,
            [this](const QString& action) {
        if (action == QStringLiteral("start")) onStartRunning();
        else if (action == QStringLiteral("stop")) onStopRunning();
        else onExecuteOnce(); // run
    });
    // 立即喂最近一次结果, 避免切过来一片空白
    m_runtimeUI->updateResults(m_flowEngine->lastToolStates(), m_flowEngine->lastResultData());
#ifdef VI_HAS_OPENCV
    CvImagePtr lastImg = m_flowEngine->lastImage();
    if (lastImg && !lastImg->empty())
        m_runtimeUI->setRuntimeImage(cvMatToQImage(*lastImg));
#endif

    // 已存在则先移除旧页
    if (m_centralStack->indexOf(m_runtimeUI) < 0)
        m_centralStack->addWidget(m_runtimeUI);
    m_centralStack->setCurrentWidget(m_runtimeUI);
    m_statusLabel->setText(QStringLiteral("已切换到运行界面(DIY布局)"));
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
        resetUndoState();
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
    // 无自动执行流程时直接提示, 避免点击后状态栏卡在"执行中..."
    bool hasExecutable = false;
    for (const Flow* f : m_flowEngine->flows()) {
        if (f->autoExecute()) { hasExecutable = true; break; }
    }
    if (!hasExecutable) {
        m_statusLabel->setText("没有可执行的流程");
        QMessageBox::information(this, QStringLiteral("执行程序"),
            QStringLiteral("没有可执行的流程。\n请先[文件→从模板新建]或[文件→新建项目]后在流程中添加工具。"));
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
    // DIY 界面编辑器: 拖拽控件(图像/数据表/按钮/状态面板/数值显示)自定义运行界面, 支持 JSON 存盘
    auto* editor = new UIEditor(this);
    editor->setWindowTitle(QString::fromUtf8("界面设置 — DIY 运行界面编辑器"));
    editor->setAttribute(Qt::WA_DeleteOnClose);
    editor->resize(1280, 820);
    editor->show();
    editor->raise();
    editor->activateWindow();
    m_statusLabel->setText("已打开界面编辑器(DIY)");
}

void MainWindow::onGlobalVariables() {
    GlobalVariablesDialog dlg(m_globalVars, this);
    dlg.exec();
}

void MainWindow::onTeachWizard() {
    auto* dlg = new TeachWizard(m_camera, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void MainWindow::onYoloTeachWizard() {
    auto* dlg = new YoloTeachWizard(m_camera, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
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
    // 弹出相机管理对话框 (CCD1~CCD8 手动IP配置)
    if (!m_cameraManager) {
        m_cameraManager = new CameraManagerDialog(this);
        connect(m_cameraManager, &CameraManagerDialog::cameraStatusChanged,
                this, [this](int index, bool connected, const QString& info) {
            Q_UNUSED(index);
            syncCamerasToEngine();
            if (connected) {
                // 主状态栏显示第一个连接的相机
                const auto cams = m_cameraManager->connectedCameras();
                if (!cams.isEmpty()) {
                    ICameraDriver* cam = cams.first();
                    const QString name = cams.firstKey();
                    if (m_camera != cam) {
                        setCameraDriver(cam);
                    }
                    m_cameraLabel->setText(QString("%1: %2").arg(name, info));
                    m_connectionLabel->setText("已连接");
                    if (!cam->isAcquiring()) cam->startAcquisition();
                }
            } else {
                const auto cams = m_cameraManager->connectedCameras();
                if (cams.isEmpty()) {
                    m_cameraLabel->setText("相机: 无");
                    m_connectionLabel->setText("无相机");
                }
            }
        });
    }
    m_cameraManager->show();
    m_cameraManager->raise();
    m_cameraManager->activateWindow();
}

/** 把相机管理器中全部已连接相机注册到流程引擎多相机表 (CaptureImage按别名取用) */
void MainWindow::syncCamerasToEngine()
{
    if (!m_flowEngine || !m_cameraManager) return;
    m_flowEngine->clearNamedCameras();
    const auto cams = m_cameraManager->connectedCameras();
    for (auto it = cams.begin(); it != cams.end(); ++it)
        m_flowEngine->setNamedCamera(it.key(), it.value());
    if (!cams.isEmpty()) {
        QStringList names = cams.keys();
        m_logPanel->appendLog(QString("多相机注册: %1").arg(names.join(", ")));
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
    // 1) 先停流程线程并等待结束, 再释放硬件/连接池 (防工作线程 use-after-free)
    if (m_flowEngine) {
        if (m_flowEngine->isRunning() || m_flowEngine->isExecuting()) {
            m_flowEngine->stopRunning(true);
            m_flowEngine->shutdownAndWait(3000);
        }
    }

    // 2) 未保存提示 (工业软件防配置丢失)
    if (m_projectMgr && m_projectMgr->isModified()) {
        const auto ret = QMessageBox::warning(
            this, QStringLiteral("未保存的修改"),
            QStringLiteral("当前项目有未保存的修改，是否保存？"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);
        if (ret == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        if (ret == QMessageBox::Save) onSaveProject();
    }

    // 3) 释放资源
    if (m_camera) m_camera->closeCamera();
    ModbusTcpMaster::releaseAll();   // 断开所有Modbus连接 (工作线程已停, 安全)
    saveSettings();
    event->accept();
}

} // namespace VisionInspector
