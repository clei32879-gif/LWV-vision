/**
 * @file FlowEngine.h
 * @brief 流程执行引擎
 *
 * 管理和执行检测流程。
 * 一个项目可以有多个流程, 每个流程包含多个工具, 按顺序执行。
 *
 * 对应CKVisionBuilder的流程栏:
 *   - 流程选项卡 = 多个流程
 *   - 编辑区域 = 工具列表(按顺序)
 *   - 执行/运行 = 单次/循环执行流程
 */

#pragma once

#include "../utils/Common.h"
#include "ITool.h"
#include "ToolContext.h"
#include <QObject>
#include <QList>
#include <QString>
#include <memory>
#include <functional>

namespace VisionInspector {

/**
 * 单个流程
 */
class Flow : public QObject {
    Q_OBJECT

public:
    Flow(QObject* parent = nullptr) : QObject(parent) {}

    // 流程信息
    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    // 是否自动执行
    bool autoExecute() const { return m_autoExecute; }
    void setAutoExecute(bool auto_) { m_autoExecute = auto_; }

    // 执行延迟(毫秒)
    int delayMs() const { return m_delayMs; }
    void setDelayMs(int ms) { m_delayMs = ms; }

    // 工具管理
    void addTool(ITool* tool);
    void insertTool(int index, ITool* tool);
    void removeTool(int index);
    void removeTool(ITool* tool);
    void moveTool(int from, int to);
    ITool* toolAt(int index) const;
    int toolCount() const { return m_tools.size(); }
    QList<ITool*> tools() const { return m_tools; }

    // 清空
    void clear();

    // 序列化
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);

private:
    QString m_name;
    bool m_autoExecute = true;   // 自动执行
    int m_delayMs = 0;           // 执行间隔(毫秒)
    QList<ITool*> m_tools;       // 工具列表 (非拥有, 外部管理生命周期)
};

/**
 * 流程执行引擎
 */
class FlowEngine : public QObject {
    Q_OBJECT

public:
    explicit FlowEngine(QObject* parent = nullptr);

    // --------------------------------------------------------
    // 流程管理
    // --------------------------------------------------------

    /** 添加流程 */
    void addFlow(Flow* flow);

    /** 移除流程 */
    void removeFlow(Flow* flow);

    /** 获取所有流程 */
    QList<Flow*> flows() const { return m_flows; }

    /** 流程数量 */
    int flowCount() const { return m_flows.size(); }

    /** 按名称查找流程 */
    Flow* findFlow(const QString& name) const;

    // --------------------------------------------------------
    // 执行控制
    // --------------------------------------------------------

    /**
     * 单次执行指定流程
     * @param flow 要执行的流程, nullptr则执行第一个自动执行流程
     * @param context 执行上下文
     * @return true=全部工具OK, false=有NG或错误
     */
    bool executeOnce(Flow* flow, ToolContext& context);

    /**
     * 开始循环执行
     * @param flow 要循环执行的流程
     */
    void startRunning(Flow* flow);

    /** 停止循环执行 */
    void stopRunning();

    /** 是否正在运行 */
    bool isRunning() const { return m_running; }

    // --------------------------------------------------------
    // 硬件接口设置 (注入到ToolContext)
    // --------------------------------------------------------
    void setCameraDriver(ICameraDriver* camera) { m_camera = camera; }
    void setPLCDriver(IPLCDriver* plc) { m_plc = plc; }
    void setServoDriver(IServoDriver* servo) { m_servo = servo; }
    void setGlobalVariables(GlobalVariables* gv) { m_globalVars = gv; }

signals:
    // 工具执行状态变化
    void toolStatusChanged(Flow* flow, int toolIndex, ToolStatus status);

    // 流程执行完成
    void flowExecuted(Flow* flow, bool allOk);

    // 日志
    void logMessage(const QString& message);

private:
    QList<Flow*> m_flows;

    bool m_running = false;
    int m_runIndex = 0;

    // 硬件接口 (注入到ToolContext)
    ICameraDriver* m_camera = nullptr;
    IPLCDriver* m_plc = nullptr;
    IServoDriver* m_servo = nullptr;
    GlobalVariables* m_globalVars = nullptr;
};

} // namespace VisionInspector
