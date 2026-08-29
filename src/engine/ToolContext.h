/**
 * @file ToolContext.h
 * @brief 工具执行上下文
 *
 * ToolContext是工具执行时的"环境"。
 * 包含: 输入数据(图像/数值)、输出数据、全局变量、硬件接口等。
 *
 * 对应CKVision中工具之间的数据传递:
 *   上一个工具的输出 → 上下文 → 下一个工具的输入
 */

#pragma once

#include "../utils/Common.h"
#include <QVariant>
#include <vector>
#include <QMap>
#include <QString>
#include <memory>
#include <chrono>

namespace VisionInspector {

class ICameraDriver;
class IPLCDriver;
class IServoDriver;
class GlobalVariables;

/**
 * 工具执行上下文
 */
class ToolContext {
public:
    ToolContext() = default;

    // --------------------------------------------------------
    // 图像数据 (主要数据流)
    // --------------------------------------------------------

    /** 设置命名图像 */
    void setImage(const QString& name, const CvImagePtr& img) {
        m_images[name] = img;
    }

    /** 获取命名图像 */
    CvImagePtr getImage(const QString& name) const {
        auto it = m_images.find(name);
        if (it != m_images.end())
            return it.value();
        return nullptr;
    }

    /** 获取当前图像 (快捷方式, 等价于 getImage("Current") ) */
    CvImagePtr currentImage() const { return getImage("Current"); }
    void setCurrentImage(const CvImagePtr& img) { setImage("Current", img); }

    /** 获取所有图像名 */
    QStringList imageNames() const { return m_images.keys(); }

    // --------------------------------------------------------
    // 通用数据 (数值、字符串等)
    // --------------------------------------------------------

    /** 设置数据 */
    void setData(const QString& key, const QVariant& value) {
        m_data[key] = value;
    }

    /** 获取数据 */
    QVariant getData(const QString& key, const QVariant& def = QVariant()) const {
        return m_data.value(key, def);
    }

    /** 获取数据(double类型) */
    double getDouble(const QString& key, double def = 0.0) const {
        return getData(key, def).toDouble();
    }

    /** 获取数据(int类型) */
    int getInt(const QString& key, int def = 0) const {
        return getData(key, def).toInt();
    }

    /** 获取数据(bool类型) */
    bool getBool(const QString& key, bool def = false) const {
        return getData(key, def).toBool();
    }

    /** 获取数据(字符串类型) */
    QString getString(const QString& key, const QString& def = QString()) const {
        return getData(key, def).toString();
    }

    /** 是否包含某个key */
    bool hasData(const QString& key) const { return m_data.contains(key); }

    /** 获取所有数据 */
    const DataMap& allData() const { return m_data; }

    // --------------------------------------------------------
    // 工具结果聚合 (按工具名归档, 支持工具间引用)
    // --------------------------------------------------------

    /** 归档某工具的执行结果 (FlowEngine在工具执行完调用) */
    void setToolResult(const QString& toolName, const DataMap& result) {
        m_toolResults[toolName] = result;
    }

    /** 获取某工具的结果数据 */
    DataMap toolResult(const QString& toolName) const {
        return m_toolResults.value(toolName);
    }

    /** 已产生结果的工具名列表 (供属性引用提示) */
    QStringList toolNames() const { return m_toolResults.keys(); }

    // --------------------------------------------------------
    // 结果叠加层 (工具绘制的图形描述, 图像坐标系)
    // --------------------------------------------------------

    /** 追加某工具的叠加图形 (overlays()输出) */
    void addOverlays(const std::vector<QVariant>& shapes) {
        for (const QVariant& v : shapes) m_overlays.append(v);
    }

    /** 全部叠加图形 */
    const QVariantList& overlays() const { return m_overlays; }

    // --------------------------------------------------------
    // 消息值 (流程控制用, 对应CKVision的消息机制)
    // --------------------------------------------------------

    int message() const { return m_message; }
    void setMessage(int msg) { m_message = msg; }

    // --------------------------------------------------------
    // 硬件接口 (供通讯类工具使用)
    // --------------------------------------------------------

    void setCameraDriver(ICameraDriver* camera) { m_camera = camera; }
    ICameraDriver* cameraDriver() { return m_camera; }

    void setPLCDriver(IPLCDriver* plc) { m_plc = plc; }
    IPLCDriver* plcDriver() { return m_plc; }

    void setServoDriver(IServoDriver* servo) { m_servo = servo; }
    IServoDriver* servoDriver() { return m_servo; }

    // --------------------------------------------------------
    // 全局变量 (跨流程共享)
    // --------------------------------------------------------

    void setGlobalVariables(GlobalVariables* gv) { m_globalVars = gv; }
    GlobalVariables* globalVariables() { return m_globalVars; }

    // --------------------------------------------------------
    // 执行信息
    // --------------------------------------------------------

    /** 执行序号 (第几次执行) */
    void setRunIndex(int idx) { m_runIndex = idx; }
    int runIndex() const { return m_runIndex; }

    /** 清空所有数据 (每次流程执行前调用) */
    void clear() {
        m_images.clear();
        m_data.clear();
        m_toolResults.clear();
        m_overlays.clear();
        m_message = 0;
        // 不清空硬件接口和全局变量
    }

private:
    QMap<QString, CvImagePtr> m_images;      // 命名图像
    DataMap m_data;                          // 通用数据 (含 "工具名.键" 聚合)
    QMap<QString, DataMap> m_toolResults;    // 各工具的结果归档
    QVariantList m_overlays;                 // 结果叠加图形
    int m_message = 0;                   // 消息值

    // 硬件接口 (非拥有, 由外部设置)
    ICameraDriver* m_camera = nullptr;
    IPLCDriver* m_plc = nullptr;
    IServoDriver* m_servo = nullptr;
    GlobalVariables* m_globalVars = nullptr;

    int m_runIndex = 0;  // 执行序号
};

} // namespace VisionInspector
