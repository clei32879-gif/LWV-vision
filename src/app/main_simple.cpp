// 简单测试版本 - 排查崩溃问题
#include <QApplication>
#include <QMainWindow>
#include <QLabel>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    
    QMainWindow window;
    window.setWindowTitle("Test Window");
    window.resize(800, 600);
    
    QLabel* label = new QLabel("Hello LW Vision!");
    window.setCentralWidget(label);
    
    window.show();
    return app.exec();
}
