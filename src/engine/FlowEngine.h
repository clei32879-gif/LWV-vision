/**
 * @file FlowEngine.h
 * @brief 流程执行引擎 (多线程版)
 *
 * 管理和执行检测流程。一个项目可以有多个流程, 每个流程包含多个工具, 按顺序执行。
 *
 * 线程模型 (阶段1架构改造):
 *   - 单次执行 executeOnceAsync() / 连续运行 startRunning() 都在 QtConcurrent
 *     线程池的工作线程中执行, UI 线程只通过信号接收结果, 不会被算法阻塞。
 *   - stopRunning() 置中止标志, 当前工具执行完即在工具间隙安全退出。
 *   - Flow 的工具列表变更(QMutex保护)与工作线程的快照迭代互不干扰。
 *
 * 数据流 (对应CKVision的数据关联):
 *   - 每个工具执行完, 其 resultData 以 "工具名.键" 写入上下文全局命名空间,
 *     后续工具可直接读取 (如 数据判断 的 sourceDataKey 填 "找圆_1.radius")。
 *   - 任意字符串属性支持引用语法 "$(工具名.键)", 执行时自动替换为实际值。
 *
 * 对应CKVisionBuilder的流程栏: 流程选项卡=多流程, 执行/运行=单次/循环执行
 */

#pragma once

#include "../utils/Common.h"
#include "ITool.h"
#include "ToolContext.h"
#include <QObject>
#include <QList>
#include <QString>
#include <QMutex>
#include <atomic>
#include <memory>
#include <functional>

namespace VisionInspector {

/**
 * 单个流程 (线程安全: UI线程改工具列表, 工作线程快照执行)
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

    // 工具管理 (线程安全)
    void addTool(ITool* tool);
    void insertTool(int index, ITool* tool);
    void removeTool(int index);
    void removeTool(ITool* tool);
    void moveTool(int from, int to);
    ITool* toolAt(int index) const;
    int toolCount() const;
    /** 当前工具列表快照 (工作线程用副本迭代, 与UI修改互不干扰) */
    QList<ITool*> snapshotTools() const;

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
    mutable QMutex m_mutex;      // 保护 m_tools
};

/**
 * 流程执行引擎 (多线程)
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
    // 执行控制 (异步, UI不被阻塞)
    // --------------------------------------------------------

    /**
     * 单次执行(异步, 提交到工作线程), 结果经 flowExecuted / executionFinished 信号返回。
     * @param flow 要执行的流程, nullptr则执行第一个自动执行流程
     */
    void executeOnceAsync(Flow* flow);

    /**
     * 同步执行 (阻塞调用线程!) — 仅供测试与特殊场景。
     * @return true=全部工具OK, false=有NG或错误
     */
    bool executeOnce(Flow* flow, ToolContext& context);

    /**
     * 开始连续运行 (后台工作线程循环执行)
     */
    void startRunning(Flow* flow);

    /**
     * 停止连续运行 (对齐CKVision双停止模式)
     * @param force true=强制停止: 当前工具执行完立即返回(可打断死循环)
     *              false=普通停止: 执行完本轮整个流程后再停止
     */
    void stopRunning(bool force = true);

    /** 是否连续运行中 */
    bool isRunning() const { return m_running; }

    /** 是否有流程正在执行 (单次或连续运行的某一轮) */
    bool isExecuting() const { return m_executing; }

    /** 最近一次执行的最终图像 (线程安全, 用于结果显示) */
    CvImagePtr lastImage() const;

    /** 最近一次执行的全部结果叠加图形 (线程安全, 用于结果显示) */
    QVariantList lastOverlays() const;

    // --------------------------------------------------------
    // 硬件接口设置 (注入到ToolContext)
    // --------------------------------------------------------
    void setCameraDriver(ICameraDriver* camera) { m_camera = camera; }
    void setPLCDriver(IPLCDriver* plc) { m_plc = plc; }
    void setServoDriver(IServoDriver* servo) { m_servo = servo; }
    void setGlobalVariables(GlobalVariables* gv) { m_globalVars = gv; }

signals:
    // 工具执行状态变化 (状态机: Running -> OK/NG/Disabled)
    void toolStatusChanged(Flow* flow, int toolIndex, ToolStatus status);

    // 单个工具执行完毕 (含耗时, 毫秒)
    void toolExecuted(Flow* flow, int toolIndex, ToolStatus status, qint64 elapsedMs);

    // 流程执行完成 (每轮都发, allOk=全部OK)
    void flowExecuted(Flow* flow, bool allOk);

    // 连续运行状态切换
    void runStateChanged(bool running);

    // 日志
    void logMessage(const QString& message);

private:
    /** 执行主体 (可从任意线程调用) */
    bool doExecute(Flow* flow, ToolContext& context);

    /** 连续运行循环 (线程池线程) */
    void runLoop(Flow* flow);

    QList<Flow*> m_flows;

    std::atomic_bool m_running{false};    // 连续运行中
    std::atomic_bool m_executing{false};  // 某轮执行中
    std::atomic_bool m_abort{false};      // 强制中止请求 (工具间隙检查)
    std::atomic_bool m_stopPending{false}; // 普通停止请求 (本轮流程结束后生效)
    std::atomic_int m_runIndex{0};

    // 最近一次执行的最终图像
    mutable QMutex m_lastImageMutex;
    CvImagePtr m_lastImage;
    QVariantList m_lastOverlays;

    // 硬件接口 (注入到ToolContext, 只读指针)
    ICameraDriver* m_camera = nullptr;
    IPLCDriver* m_plc = nullptr;
    IServoDriver* m_servo = nullptr;
    GlobalVariables* m_globalVars = nullptr;
};

} // namespace VisionInspector
