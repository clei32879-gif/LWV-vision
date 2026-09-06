/** @file DeviceTemplates.h - 内置设备模板定义 (阶段6: 设备模板系统)
 *
 *  四类设备模板: 视觉筛选 / 视觉贴标 / 视觉计数 / 木业检测。
 *  定义与构建集中在这一处 — 模板库画廊、主窗口、回归测试共用,
 *  新增内置模板只需在 all() 里加一条。
 */
#pragma once
#include <QString>
#include <QStringList>
#include <QList>

namespace VisionInspector {

class FlowEngine;

struct DeviceTemplate {
    QString id;          // 唯一标识 ("sifter"/"labeling"/"counting"/"wood")
    QString name;        // 显示名
    QString note;        // 工序说明 (画廊展示)
    QStringList tools;   // 工具类型名序列 (按流程顺序)
};

class DeviceTemplates {
public:
    /** 全部内置模板 */
    static const QList<DeviceTemplate>& all();

    /** 按 id 取模板, 不存在返回 nullptr */
    static const DeviceTemplate* byId(const QString& id);

    /** 构建模板流程到 engine (新建"主流程"并按序添加工具, 重名自动 _N);
     *  成功返回 true, createdCount 输出实际创建的工具数 (含自动配对工具) */
    static bool build(const QString& id, FlowEngine* engine, int* createdCount = nullptr);
};

} // namespace VisionInspector
