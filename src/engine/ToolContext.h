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
#include <QRectF>
#include <QPointF>
#include <cmath>
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
class FlowEngine;

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
    // 坐标系服务 (位置补正→ROI自动跟随, 对齐CKVision流程灵魂)
    // --------------------------------------------------------

    /**
     * 设置当前补正坐标系 (由 PositionCorrection 调用):
     *   图像点 p 在补正坐标系下的定义: p_img = R(θ)·p_local + origin
     * 即: 模板教导时工件在"标准位置", 实际工件平移了origin、旋转了θ.
     * 后续工具用 transformPoint/transformRect 把教导时的ROI变换到当前图像坐标.
     */
    void setCoordinateFrame(double cosT, double sinT, double originX, double originY) {
        m_cfActive = true;
        m_cfCos = cosT; m_cfSin = sinT;
        m_cfOriginX = originX; m_cfOriginY = originY;
    }

    /** 清除坐标系 (由 EndCorrection 调用, 恢复模板坐标=图像坐标) */
    void clearCoordinateFrame() { m_cfActive = false; }

    bool hasCoordinateFrame() const { return m_cfActive; }

    /**
     * 把"教导时的点"变换到"当前图像坐标":
     *   实际位置 = 教导位置相对标准位置的偏移 施加到教导点
     *   p_img = p_teach + (origin - teachOrigin), 再绕origin旋转θ
     * 简化实现(与CKVision一致的工程语义):
     *   d = (origin - 教导标准原点) 不需要 — 直接用: 先绕原点旋转, 再平移
     *   p_img = R(θ)·(p_teach - p_ref) + origin + p_ref - p_ref
     * 其中 p_ref = 教导时标准参考点(即补正前定位位置). 由于 PositionCorrection
     * 已经把 origin 设为"当前实际定位点", 语义为:
     *   p_img = R(θ)·(p_teach - teachRef) + actualRef
     * teachRef 由首次 setCoordinateFrame 时记录的参考点给出 — 这里用最简模型:
     * 直接旋转+平移: p_img = R(θ)·p_teach_offset + actual
     * 工程上等价于: 工具教导ROI以"标准位置工件中心"为基准, 运行时跟随.
     */
    QPointF transformPoint(const QPointF& teachPt) const {
        if (!m_cfActive) return teachPt;
        const double dx = teachPt.x();  // 相对标准位置的量(工具的ROI参数即相对量)
        const double dy = teachPt.y();
        return QPointF(m_cfCos * dx - m_cfSin * dy + m_cfOriginX,
                       m_cfSin * dx + m_cfCos * dy + m_cfOriginY);
    }

    /** 变换矩形ROI: 中心按坐标系变换, 尺寸不变, 返回轴对齐外接矩形 */
    QRectF transformRect(const QRectF& teachRect) const {
        if (!m_cfActive) return teachRect;
        const QPointF c = transformPoint(teachRect.center());
        return QRectF(c.x() - teachRect.width() / 2,
                      c.y() - teachRect.height() / 2,
                      teachRect.width(), teachRect.height());
    }

    /** 变换角度 (工具的扫描角度叠加补正角) */
    double transformAngle(double teachAngleDeg) const {
        if (!m_cfActive) return teachAngleDeg;
        return teachAngleDeg + std::atan2(m_cfSin, m_cfCos) * 180.0 / M_PI;
    }

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

    // --------------------------------------------------------
    // 多相机支持 (按相机别名取驱动, 供 CaptureImage 指定 CCD1~CCD8)
    // --------------------------------------------------------
    /** 注册/更新命名相机 (FlowEngine执行前由 MainWindow 按相机管理器槽位注册) */
    void setNamedCamera(const QString& name, ICameraDriver* cam) {
        if (!name.isEmpty()) m_namedCameras[name] = cam;
    }
    /** 按别名取相机 (如 "CCD1"/"CCD2-正面"); 未找到返回 nullptr */
    ICameraDriver* namedCamera(const QString& name) const {
        // 精确匹配优先
        auto it = m_namedCameras.find(name);
        if (it != m_namedCameras.end()) return it.value();
        // 前缀匹配: "CCD1" 可匹配 "CCD1-正面检测" (工具里填短名即可)
        for (auto jt = m_namedCameras.begin(); jt != m_namedCameras.end(); ++jt) {
            if (jt.key().startsWith(name)) return jt.value();
        }
        return nullptr;
    }
    /** 命名相机别名列表 (属性对话框下拉用) */
    QStringList namedCameraNames() const { return m_namedCameras.keys(); }

    void setPLCDriver(IPLCDriver* plc) { m_plc = plc; }
    IPLCDriver* plcDriver() { return m_plc; }

    void setServoDriver(IServoDriver* servo) { m_servo = servo; }
    IServoDriver* servoDriver() { return m_servo; }

    // --------------------------------------------------------
    // 流程引擎 (执行流程工具需要按名调用其他流程)
    // --------------------------------------------------------

    void setFlowEngine(FlowEngine* engine) { m_flowEngine = engine; }
    FlowEngine* flowEngine() { return m_flowEngine; }

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
        clearCoordinateFrame();
        m_message = 0;
        // 不清空硬件接口和全局变量
    }

private:
    QMap<QString, CvImagePtr> m_images;      // 命名图像
    DataMap m_data;                          // 通用数据 (含 "工具名.键" 聚合)
    QMap<QString, DataMap> m_toolResults;    // 各工具的结果归档
    QVariantList m_overlays;                 // 结果叠加图形
    int m_message = 0;                   // 消息值

    // 坐标系服务 (PositionCorrection写入 / EndCorrection清除)
    bool m_cfActive = false;
    double m_cfCos = 1.0, m_cfSin = 0.0;
    double m_cfOriginX = 0.0, m_cfOriginY = 0.0;

    // 硬件接口 (非拥有, 由外部设置)
    ICameraDriver* m_camera = nullptr;
    QMap<QString, ICameraDriver*> m_namedCameras;   // 多相机: 别名→驱动 (非拥有)
    IPLCDriver* m_plc = nullptr;
    IServoDriver* m_servo = nullptr;
    FlowEngine* m_flowEngine = nullptr;
    GlobalVariables* m_globalVars = nullptr;

    int m_runIndex = 0;  // 执行序号
};

} // namespace VisionInspector
