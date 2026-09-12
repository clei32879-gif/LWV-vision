/**
 * @file main.cpp
 * @brief LW Vision - 程序入口
 */

#include "MainWindow.h"
#include "../utils/Logger.h"
#include "../ui/IconHelper.h"
#include <QApplication>
#include <QStyleFactory>
#include <QFile>
#include <QFont>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("LW Vision");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("LW Vision");
    app.setWindowIcon(VisionInspector::IconHelper::appIcon(64));

    // 设置应用样式
    app.setStyle(QStyleFactory::create("Fusion"));

    // 全局淡蓝色主题 (轻松不压抑; 图像区保持中性浅灰便于观察)
    QString styleSheet = R"(
        QWidget {
            background-color: #eef5fc;
            color: #24425f;
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
        }
        QMainWindow {
            background-color: #e6eff9;
        }
        QMenuBar {
            background-color: #d8e8f7;
            color: #24425f;
            border-bottom: 1px solid #b8d2ea;
        }
        QMenuBar::item:selected {
            background-color: #aed0f0;
            border-radius: 3px;
        }
        QMenu {
            background-color: #f4f9ff;
            color: #24425f;
            border: 1px solid #b8d2ea;
        }
        QMenu::item:selected {
            background-color: #aed0f0;
        }
        QToolBar {
            background-color: #dcecfa;
            border-bottom: 1px solid #b8d2ea;
            spacing: 4px;
            padding: 2px;
        }
        QToolBar QToolButton {
            background-color: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 4px 8px;
            color: #24425f;
        }
        QToolBar QToolButton:hover {
            background-color: #c4ddf5;
            border-color: #9cc2e8;
        }
        QToolBar QToolButton:pressed {
            background-color: #8ec2ee;
        }
        QStatusBar {
            background-color: #dcecfa;
            color: #4a6a8c;
            border-top: 1px solid #b8d2ea;
        }
        QDockWidget {
            color: #24425f;
            titlebar-close-icon: none;
        }
        QDockWidget::title {
            background-color: #d0e4f6;
            padding: 6px;
            border-bottom: 1px solid #b8d2ea;
        }
        QTabWidget::pane {
            border: 1px solid #b8d2ea;
        }
        QTabBar::tab {
            background-color: #dcebf8;
            color: #4a6a8c;
            padding: 6px 12px;
            border: 1px solid #b8d2ea;
            border-bottom: none;
            border-top-left-radius: 3px;
            border-top-right-radius: 3px;
        }
        QTabBar::tab:selected {
            background-color: #8ec2ee;
            color: white;
        }
        QScrollBar:vertical {
            background-color: #e6eff9;
            width: 12px;
        }
        QScrollBar::handle:vertical {
            background-color: #aed0f0;
            border-radius: 6px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #8ec2ee;
        }
        QScrollBar::handle:horizontal {
            background-color: #aed0f0;
            border-radius: 6px;
            min-width: 20px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: #8ec2ee;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
        QPushButton {
            background-color: #e8f2fc;
            border: 1px solid #9cc2e8;
            border-radius: 4px;
            padding: 6px 12px;
            color: #24425f;
        }
        QPushButton:hover {
            background-color: #d5e8fa;
            border-color: #6aa8dc;
        }
        QPushButton:pressed {
            background-color: #8ec2ee;
        }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: white;
            border: 1px solid #b8d2ea;
            border-radius: 3px;
            padding: 4px;
            color: #24425f;
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border-color: #4a9eff;
        }
        QTableWidget {
            background-color: white;
            gridline-color: #d5e3f0;
            color: #24425f;
        }
        QTableWidget::item:selected {
            background-color: #aed0f0;
        }
        QHeaderView::section {
            background-color: #d5e6f6;
            color: #24425f;
            padding: 4px;
            border: 1px solid #c3d9ee;
        }
        QLabel {
            background: transparent;
            color: #24425f;
        }
        QGroupBox {
            border: 1px solid #b8d2ea;
            border-radius: 4px;
            margin-top: 8px;
            padding-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 3px;
        }
        QSplitter::handle {
            background-color: #c4ddf5;
        }
        QSplitter::handle:hover {
            background-color: #8ec2ee;
        }
        QSplitter::handle:pressed {
            background-color: #4a9eff;
        }
        QCheckBox, QRadioButton {
            background: transparent;
            color: #24425f;
        }
    )";
    app.setStyleSheet(styleSheet);

    // 初始化日志系统
    VisionInspector::Logger::instance().init();

    VI_LOG_INFO("=== LW Vision 启动 ===");
    VI_LOG_INFO(QString("Qt 版本: %1").arg(QT_VERSION_STR));
#ifdef VI_HAS_OPENCV
    VI_LOG_INFO(QString("OpenCV 版本: %1").arg(CV_VERSION));
#else
    VI_LOG_INFO("OpenCV: 未启用 (仅UI模式)");
#endif

    // 创建主窗口
    VisionInspector::MainWindow window;
    window.setWindowTitle("LW Vision v1.0.0");
    window.show();

    VI_LOG_INFO("主窗口已显示");

    int ret = app.exec();
    VI_LOG_INFO(QString("=== 程序退出 (返回码: %1) ===").arg(ret));
    return ret;
}
