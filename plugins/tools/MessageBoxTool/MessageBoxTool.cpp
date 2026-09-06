/** @file MessageBoxTool.cpp - 提示对话框工具实现
 *
 *  执行时向 UI 线程投递 QMetaObject::invokeMethod 弹窗; 后台线程不阻塞等待,
 *  支持 autoCloseMs 自动关闭 (QTimer), 连续运行高频触发时跳过 (对话框已开则不重复弹)。
 */
#include "MessageBoxTool.h"
#include "../../../src/engine/ToolRegistry.h"
#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QPushButton>
#include <QPointer>

namespace VisionInspector {

namespace {
// 同文本对话框去重句柄: 连续运行时每帧 execute, 已开的框不重复弹
QPointer<QMessageBox> g_activeBox;
}

PropertyDefList MessageBoxTool::propertyDefs() const {
    return {
        PropertyDef::enumProp("icon", "图标", {"信息", "警告", "错误"}, 0, "显示"),
        PropertyDef::stringProp("title", "标题", "提示", "显示"),
        PropertyDef::stringProp("message", "内容", "", "显示"),
        PropertyDef::intProp("autoCloseMs", "自动关闭(ms, 0=手动)", 0, 0, 600000, "显示"),
        PropertyDef::boolProp("modal", "模态(阻塞流程)", false, "行为"),
    };
}

bool MessageBoxTool::execute(ToolContext& context) {
#ifndef VI_HAS_OPENCV
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#else
    const int icon = propertyValue("icon").toInt();
    const QString title = propertyValue("title").toString();
    const QString message = propertyValue("message").toString();
    const int autoCloseMs = propertyValue("autoCloseMs").toInt();
    const bool modal = propertyValue("modal").toBool();

    // 已有同款对话框在弹 → 跳过 (连续运行防炸屏)
    if (g_activeBox) {
        setResultData("shown", false);
        setResultData("skipped", true);
        setStatus(ToolStatus::OK);
        return true;
    }

    // 捕获局部值 → UI线程构造对话框
    QMetaObject::invokeMethod(QApplication::instance(), [icon, title, message,
                                                          autoCloseMs, modal]() {
        QMessageBox* box = new QMessageBox(nullptr);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->setWindowTitle(title);
        box->setText(message);
        switch (icon) {
        case 1: box->setIcon(QMessageBox::Warning); break;
        case 2: box->setIcon(QMessageBox::Critical); break;
        default: box->setIcon(QMessageBox::Information); break;
        }
        box->setStandardButtons(QMessageBox::Ok);

        if (autoCloseMs > 0) {
            auto* timer = new QTimer(box);
            timer->setSingleShot(true);
            QObject::connect(timer, &QTimer::timeout, box, &QMessageBox::close);
            timer->start(autoCloseMs);
        }
        g_activeBox = box;
        if (modal)
            box->exec();          // 阻塞 (流程线程 waitForDone 语义由调用方决定)
        else
            box->show();          // 非模态: 不打断执行
    }, Qt::QueuedConnection);

    setResultData("shown", true);
    setResultData("skipped", false);
    setResultData("modal", modal);
    setStatus(ToolStatus::OK);
    return true;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(MessageBoxTool, "提示对话框", VisionInspector::ToolCategory::Logic)
