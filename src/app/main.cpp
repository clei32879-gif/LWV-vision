/**
 * @file main.cpp
 * @brief LW Vision 立维视觉 - 程序入口
 */

#include "MainWindow.h"
#include "../utils/Logger.h"
#include <QApplication>
#include <QStyleFactory>
#include <QFile>
#include <QFont>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("LW Vision");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("立维视觉");

    // 设置应用样式
    app.setStyle(QStyleFactory::create("Fusion"));

    // 全局深色主题样式表（参考华晨智能工业风格）
    QString styleSheet = R"(
        QWidget {
            background-color: #2b2b2b;
            color: #e0e0e0;
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
        }
        QMainWindow {
            background-color: #1e1e1e;
        }
        QMenuBar {
            background-color: #333333;
            color: #e0e0e0;
            border-bottom: 1px solid #444;
        }
        QMenuBar::item:selected {
            background-color: #4a9eff;
        }
        QMenu {
            background-color: #3c3c3c;
            color: #e0e0e0;
            border: 1px solid #555;
        }
        QMenu::item:selected {
            background-color: #4a9eff;
        }
        QToolBar {
            background-color: #2d2d2d;
            border-bottom: 1px solid #444;
            spacing: 4px;
            padding: 2px;
        }
        QToolBar QToolButton {
            background-color: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 4px 8px;
            color: #e0e0e0;
        }
        QToolBar QToolButton:hover {
            background-color: #3a3a3a;
            border-color: #555;
        }
        QToolBar QToolButton:pressed {
            background-color: #4a9eff;
        }
        QStatusBar {
            background-color: #252525;
            color: #aaa;
            border-top: 1px solid #444;
        }
        QDockWidget {
            color: #e0e0e0;
            titlebar-close-icon: none;
        }
        QDockWidget::title {
            background-color: #353535;
            padding: 6px;
            border-bottom: 1px solid #444;
        }
        QTabWidget::pane {
            border: 1px solid #444;
        }
        QTabBar::tab {
            background-color: #353535;
            color: #aaa;
            padding: 6px 12px;
            border: 1px solid #444;
            border-bottom: none;
        }
        QTabBar::tab:selected {
            background-color: #4a9eff;
            color: white;
        }
        QScrollBar:vertical {
            background-color: #2b2b2b;
            width: 12px;
        }
        QScrollBar::handle:vertical {
            background-color: #555;
            border-radius: 6px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #777;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar:horizontal {
            background-color: #2b2b2b;
            height: 12px;
        }
        QScrollBar::handle:horizontal {
            background-color: #555;
            border-radius: 6px;
            min-width: 20px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: #777;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
        QPushButton {
            background-color: #3a3a3a;
            border: 1px solid #555;
            border-radius: 4px;
            padding: 6px 12px;
            color: #e0e0e0;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
            border-color: #666;
        }
        QPushButton:pressed {
            background-color: #4a9eff;
        }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: #3c3c3c;
            border: 1px solid #555;
            border-radius: 3px;
            padding: 4px;
            color: #e0e0e0;
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border-color: #4a9eff;
        }
        QTableWidget {
            background-color: #2b2b2b;
            gridline-color: #444;
            color: #e0e0e0;
        }
        QTableWidget::item:selected {
            background-color: #4a9eff;
        }
        QHeaderView::section {
            background-color: #353535;
            color: #e0e0e0;
            padding: 4px;
            border: 1px solid #444;
        }
        QLabel {
            color: #e0e0e0;
        }
        QGroupBox {
            border: 1px solid #555;
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
            background-color: #555;
        }
        QSplitter::handle:hover {
            background-color: #4a9eff;
        }
        QSplitter::handle:pressed {
            background-color: #2a7fff;
        }
    )";
    app.setStyleSheet(styleSheet);

    // 初始化日志系统
    VisionInspector::Logger::instance().init();

    VI_LOG_INFO("=== LW Vision 立维视觉 启动 ===");
    VI_LOG_INFO(QString("Qt 版本: %1").arg(QT_VERSION_STR));
#ifdef VI_HAS_OPENCV
    VI_LOG_INFO(QString("OpenCV 版本: %1").arg(CV_VERSION));
#else
    VI_LOG_INFO("OpenCV: 未启用 (仅UI模式)");
#endif

    // 创建主窗口
    VisionInspector::MainWindow window;
    window.setWindowTitle("LW Vision v1.0.0 - 立维视觉");
    window.show();

    VI_LOG_INFO("主窗口已显示");

    int ret = app.exec();
    VI_LOG_INFO(QString("=== 程序退出 (返回码: %1) ===").arg(ret));
    return ret;
}
