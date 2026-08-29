#include <QApplication>
#include <QMainWindow>
#include <QDockWidget>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    
    QMainWindow window;
    window.setWindowTitle("Test DockWidgets");
    window.resize(800, 600);
    
    // 测试 dock widget
    auto* dock1 = new QDockWidget("Toolbox", &window);
    dock1->setWidget(new QLabel("Toolbox"));
    window.addDockWidget(Qt::LeftDockWidgetArea, dock1);
    
    auto* dock2 = new QDockWidget("Flow", &window);
    dock2->setWidget(new QLabel("Flow"));
    window.addDockWidget(Qt::RightDockWidgetArea, dock2);
    
    // 测试菜单
    auto* menu = window.menuBar()->addMenu("View");
    menu->addAction(dock1->toggleViewAction());
    menu->addAction(dock2->toggleViewAction());
    
    window.show();
    return app.exec();
}
