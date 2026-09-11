/**
 * @file FlowEngine.cpp
 * @brief 流程执行引擎实现 (多线程版)
 */

#include "FlowEngine.h"
#include "../utils/Logger.h"
#include "../hal/ICameraDriver.h"
#include <QElapsedTimer>
#include <QThread>
#include <QDateTime>
#include <QtConcurrent/QtConcurrentRun>
#include <QRegularExpression>

namespace VisionInspector {

// ============================================================
// 工具属性引用解析
//   字符串属性里的 "$(工具名.键)" 在执行时替换为该工具的结果值。
//   例: "$(找圆_1.radius)" -> 128.35
// ============================================================
namespace {

QVariant resolvePropertyRefs(const QVariant& value, const ToolContext& ctx) {
    if (value.typeId() != QMetaType::QString) return value;
    const QString src = value.toString();
    if (!src.contains(QStringLiteral("$("))) return value;

    static const QRegularExpression rx(
        QStringLiteral("\\$\\(([^)]+)\\)"));
    QString out = src;
    auto it = rx.globalMatch(src);
    bool changed = false;
    while (it.hasNext()) {
        auto m = it.next();
        const QString ref = m.captured(1);       // "工具名.键"
        const int dot = ref.lastIndexOf('.');
        if (dot <= 0) continue;
        const QString toolName = ref.left(dot);
        const QString key = ref.mid(dot + 1);
        QVariant v = ctx.getData(toolName + QLatin1Char('.') + key);
        if (!v.isValid())
            v = ctx.toolResult(toolName).value(key);
        if (v.isValid()) {
            out.replace(m.captured(0), v.toString());
            changed = true;
        }
    }
    return changed ? QVariant(out) : value;
}

/**
 * RAII: 执行前把含引用的属性替换为解析值, 执行后恢复原值。
 * 这样不破坏用户配置的模板, 也无需每个工具感知引用机制。
 */
struct PropertyResolveGuard {
    ITool* tool;
    QMap<QString, QVariant> saved;

    PropertyResolveGuard(ITool* t, const ToolContext& ctx) : tool(t) {
        const auto defs = t->propertyDefs();
        for (const auto& def : defs) {
            const QVariant v = t->propertyValue(def.name);
            if (v.typeId() == QMetaType::QString &&
                v.toString().contains(QStringLiteral("$("))) {
                saved[def.name] = v;
                t->setProperty(def.name, resolvePropertyRefs(v, ctx));
            }
        }
    }
    ~PropertyResolveGuard() {
        for (auto it = saved.begin(); it != saved.end(); ++it)
            tool->setProperty(it.key(), it.value());
    }
    Q_DISABLE_COPY_MOVE(PropertyResolveGuard)
};

} // anonymous namespace

// ============================================================
// Flow 实现 (线程安全)
// ============================================================

void Flow::addTool(ITool* tool) {
    if (!tool) return;
    QMutexLocker locker(&m_mutex);
    m_tools.append(tool);
}

void Flow::insertTool(int index, ITool* tool) {
    if (!tool) return;
    QMutexLocker locker(&m_mutex);
    if (index < 0 || index >= m_tools.size())
        m_tools.append(tool);
    else
        m_tools.insert(index, tool);
}

void Flow::removeTool(int index) {
    QMutexLocker locker(&m_mutex);
    if (index >= 0 && index < m_tools.size()) {
        ITool* tool = m_tools.takeAt(index);
        // Flow 拥有工具 (H-1): 移除即删除; 调用方须确保工作线程已停
        delete tool;
    }
}

void Flow::removeTool(ITool* tool) {
    if (!tool) return;
    QMutexLocker locker(&m_mutex);
    m_tools.removeAll(tool);
    delete tool;
}

void Flow::moveTool(int from, int to) {
    QMutexLocker locker(&m_mutex);
    if (from < 0 || from >= m_tools.size()) return;
    if (to < 0 || to >= m_tools.size()) return;
    m_tools.move(from, to);
}

ITool* Flow::toolAt(int index) const {
    QMutexLocker locker(&m_mutex);
    if (index >= 0 && index < m_tools.size())
        return m_tools[index];
    return nullptr;
}

int Flow::toolCount() const {
    QMutexLocker locker(&m_mutex);
    return m_tools.size();
}

QList<ITool*> Flow::snapshotTools() const {
    QMutexLocker locker(&m_mutex);
    return m_tools;   // QList隐式共享, 迭代副本与后续修改安全隔离
}

void Flow::clear() {
    QMutexLocker locker(&m_mutex);
    m_tools.clear();
}

QJsonObject Flow::toJson() const {
    QMutexLocker locker(&m_mutex);
    QJsonObject json;
    json["name"] = m_name;
    json["autoExecute"] = m_autoExecute;
    json["delayMs"] = m_delayMs;

    QJsonArray toolsArray;
    for (ITool* tool : m_tools) {
        toolsArray.append(tool->toJson());
    }
    json["tools"] = toolsArray;
    return json;
}

void Flow::fromJson(const QJsonObject& json) {
    {
        QMutexLocker locker(&m_mutex);
        m_name = json.value("name").toString();
        m_autoExecute = json.value("autoExecute").toBool(true);
        m_delayMs = json.value("delayMs").toInt(0);
        m_tools.clear();
    }
    // 工具的重建需要ToolRegistry, 由ProjectManager负责
}

// ============================================================
// FlowEngine 实现
// ============================================================

FlowEngine::FlowEngine(QObject* parent)
    : QObject(parent)
{
    // 跨线程 queued 信号注册 (工作线程emit ToolStatus需已注册, 否则被静默丢弃)
    qRegisterMetaType<ToolStatus>("ToolStatus");
}

void FlowEngine::addFlow(Flow* flow) {
    if (flow && !m_flows.contains(flow)) {
        m_flows.append(flow);
    }
}

void FlowEngine::removeFlow(Flow* flow) {
    m_flows.removeAll(flow);
}

Flow* FlowEngine::findFlow(const QString& name) const {
    for (Flow* flow : m_flows) {
        if (flow->name() == name)
            return flow;
    }
    return nullptr;
}

CvImagePtr FlowEngine::lastImage() const {
    QMutexLocker locker(&m_lastImageMutex);
    return m_lastImage;
}

QVariantList FlowEngine::lastOverlays() const {
    QMutexLocker locker(&m_lastImageMutex);
    return m_lastOverlays;
}

DataMap FlowEngine::lastResultData() const {
    QMutexLocker locker(&m_lastResultMutex);
    return m_lastResultData;
}

QList<QPair<QString, bool>> FlowEngine::lastToolStates() const {
    QMutexLocker locker(&m_lastResultMutex);
    return m_lastToolStates;
}

bool FlowEngine::doExecute(Flow* flow, ToolContext& context) {
    if (!flow) {
        // 找第一个自动执行的流程
        for (Flow* f : m_flows) {
            if (f->autoExecute()) {
                flow = f;
                break;
            }
        }
    }

    if (!flow) {
        VI_LOG_ERROR("没有可执行的流程");
        // 也要发完成信号, 避免调用方(如状态栏)永远停留在"执行中..."
        emit flowExecuted(nullptr, false);
        return false;
    }

    // 设置硬件接口到上下文
    context.setCameraDriver(m_camera);
    for (auto it = m_namedCameras.begin(); it != m_namedCameras.end(); ++it)
        context.setNamedCamera(it.key(), it.value());
    context.setPLCDriver(m_plc);
    context.setServoDriver(m_servo);
    context.setGlobalVariables(m_globalVars);
    context.setFlowEngine(this);   // 执行流程工具(子流程调用)需要按名查找其他流程
    context.setRunIndex(m_runIndex.fetch_add(1) + 1);

    // 重置流程控制状态 (防上一轮残留, 保证每轮干净)
    context.setData("__loop_active", false);
    context.setData("__loop_break", false);
    context.setData("__flow_ng", false);
    context.setMessage(0);

    bool allOk = true;

    VI_LOG_INFO(QString("开始执行流程: %1 (工具数: %2)")
                .arg(flow->name())
                .arg(flow->toolCount()));

    // 快照工具列表: 执行期间UI线程增删工具不影响本轮
    const QList<ITool*> tools = flow->snapshotTools();
    QList<QPair<QString, bool>> toolStates;   // 各工具执行状态 (供统计/记录)

    // 执行索引 (支持消息驱动的跳转: 循环回跳 message==1 / 跳转 message==3)
    // 由工具写入 context 的 "__engine_index" 由引擎在此处回写; 工具读它得到自己的流程序号
    int i = 0;
    const int maxSteps = 200000;   // 防死循环保护 (正常流程远达不到)
    int steps = 0;

    while (i >= 0 && i < tools.size() && steps++ < maxSteps) {
        ITool* tool = tools[i];

        // 回写当前索引供工具(Loop/LoopEnd等)感知自身位置
        context.setData("__engine_index", i);
        context.setData("__engine_tool_count", tools.size());

        // 括号式补正作用域 (对标CKVision): 位置补正→结束补正之间的工具自动跟随,
        // 免勾选 useCorrection. 位置补正执行时置位, 结束补正执行时复位 (见各工具 execute).
        // 此处仅维护继承: 上一工具的自动标志延续到本工具 (EndCorrection 自己复位).

        // 中止检查 (stopRunning后当前工具执行完即退出)
        if (m_abort) {
            VI_LOG_WARN("收到中止请求, 提前结束流程");
            allOk = false;
            break;
        }

        // 跳过禁用的工具
        if (!tool->isActive()) {
            emit toolStatusChanged(flow, i, ToolStatus::Disabled);
            ++i;
            continue;
        }

        // 为当前工具提供可用的图像来源列表
        QStringList availableImages;
        availableImages << "当前图像";
        if (context.cameraDriver() && context.cameraDriver()->isOpen()) {
            availableImages << "相机图像";
        }
        // 添加前序工具的输出图像
        for (int j = 0; j < i; ++j) {
            ITool* prevTool = tools[j];
            if (prevTool && prevTool->isActive()) {
                availableImages << prevTool->instanceName();
            }
        }
        context.setData("availableImages", availableImages);

        // 设置执行状态
        emit toolStatusChanged(flow, i, ToolStatus::Running);

        // 属性引用解析 (执行前替换 "$(工具名.键)", 执行后恢复)
        PropertyResolveGuard guard(tool, context);

        // 执行! (计时)
        bool success = false;
        QElapsedTimer timer;
        timer.start();
        try {
            success = tool->execute(context);
        } catch (const std::exception& e) {
            VI_LOG_ERROR(QString("工具执行异常: %1 - %2")
                        .arg(tool->instanceName())
                        .arg(e.what()));
            success = false;
        } catch (...) {
            VI_LOG_ERROR(QString("工具执行未知异常: %1")
                        .arg(tool->instanceName()));
            success = false;
        }

        // 工具内建上下限判定 (对齐CKVision: 执行成功后检查启用判定的结果键)
        if (success && tool->hasJudgments() && !tool->evaluateJudgments()) {
            VI_LOG_INFO(QString("工具[%1] 数据判定NG: %2")
                        .arg(tool->instanceName())
                        .arg(tool->resultData().value("judgeFailedKeys").toString()));
            success = false;
        }
        const qint64 elapsedMs = timer.elapsed();

        // 更新状态
        ToolStatus status = success ? ToolStatus::OK : ToolStatus::NG;
        tool->setStatus(status);
        toolStates.append(qMakePair(tool->instanceName(), status == ToolStatus::OK));
        emit toolStatusChanged(flow, i, status);
        emit toolExecuted(flow, i, status, elapsedMs);

        // 保存工具的输出图像到命名槽中，供后续工具引用
        CvImagePtr outputImg = context.currentImage();
        if (outputImg) {
            context.setImage(tool->instanceName(), outputImg);
        }

        // 结果聚合: 写入 "工具名.键" 全局命名空间 + 按工具归档
        const DataMap result = tool->resultData();
        context.setToolResult(tool->instanceName(), result);
        for (auto it = result.begin(); it != result.end(); ++it) {
            context.setData(tool->instanceName() + QLatin1Char('.') + it.key(), it.value());
        }

        // 叠加图形收集 (工具在图像坐标系下描述的检测结果)
        context.addOverlays(tool->overlays());

        if (!success) {
            allOk = false;
            context.setData("__flow_ng", true);   // 供 停止循环(流程NG条件) 使用
            // 与CKVision默认一致: NG不中断, 继续执行后续工具
        }

        // 消息驱动的跳转 (M-23 修复): 消费工具写入的消息, 决定下一执行索引
        //   消息值约定: 0=无 1=循环回跳(到 __loop_start) 2=停止循环(到 __loop_end 之后) 3=无条件跳转(__next_index)
        const int msg = context.message();
        if (msg != 0) context.setMessage(0);   // 消费消息, 避免泄漏到后续工具
        int next = -1;
        if (msg == 1) {
            next = context.getInt("__loop_start", -1);
        } else if (msg == 2) {
            // 停止循环: 跳到循环结束工具之后 (e+1==size 时即结束本轮)
            const int e = context.getInt("__loop_end", -1);
            if (e >= 0 && e < tools.size()) {
                i = e + 1;
                continue;
            }
            // 未知结束索引 (循环体首次迭代即触发): 顺序继续, 由 LoopEnd 兜底清除循环状态
        } else if (msg == 3) {
            next = context.getInt("__next_index", -1);
        }
        if (next >= 0 && next < tools.size()) {
            i = next;
            continue;
        }
        ++i;
    }

    if (steps >= maxSteps) {
        VI_LOG_ERROR("流程执行步骤超过保护上限, 疑似死循环, 已强制结束");
        allOk = false;
    }

    // 记录末帧图与叠加层
    {
        QMutexLocker locker(&m_lastImageMutex);
        m_lastImage = context.currentImage();
        m_lastOverlays = context.overlays();
    }

    // 记录结果数据与各工具状态 (供统计/记录/报表)
    {
        QMutexLocker locker(&m_lastResultMutex);
        m_lastResultData = context.allData();
        m_lastToolStates = toolStates;
    }

    emit flowExecuted(flow, allOk);

    VI_LOG_INFO(QString("流程执行完成: %1 结果: %2")
                .arg(flow->name())
                .arg(allOk ? "全部OK" : "有NG"));

    return allOk;
}

void FlowEngine::executeOnceAsync(Flow* flow) {
    bool expected = false;
    if (!m_executing.compare_exchange_strong(expected, true)) {
        VI_LOG_WARN("流程正在执行中, 忽略本次执行请求");
        return;
    }
    // 保存句柄供 shutdownAndWait 等待; 执行互斥防止与连续运行并发 doExecute
    m_onceFuture = QtConcurrent::run([this, flow]() {
        QMutexLocker execLock(&m_execMutex);
        ToolContext context;
        doExecute(flow, context);
        m_executing = false;
    });
}

bool FlowEngine::executeOnce(Flow* flow, ToolContext& context) {
    bool expected = false;
    if (!m_executing.compare_exchange_strong(expected, true)) {
        VI_LOG_WARN("流程正在执行中, 忽略同步执行请求");
        return false;
    }
    QMutexLocker execLock(&m_execMutex);
    bool ok = doExecute(flow, context);
    m_executing = false;
    return ok;
}

bool FlowEngine::executeSubFlow(Flow* flow, ToolContext& context) {
    // 子流程: 不抢 m_executing/m_execMutex (父流程执行中), 共享上下文直接跑
    if (!flow) return false;
    return doExecute(flow, context);
}

void FlowEngine::startRunning(Flow* flow) {
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true)) {
        VI_LOG_WARN("已在运行中, 忽略启动请求");
        return;
    }
    m_abort = false;
    m_stopPending = false;
    emit runStateChanged(true);
    VI_LOG_INFO("开始连续运行: " + (flow ? flow->name() : QString("默认流程")));

    m_runFuture = QtConcurrent::run([this, flow]() { runLoop(flow); });
}

void FlowEngine::runLoop(Flow* flow) {
    while (m_running && !m_stopPending && !m_abort) {
        m_executing = true;
        ToolContext context;
        {
            // 与单次执行互斥, 避免同轮 doExecute 并发改工具属性/结果
            QMutexLocker execLock(&m_execMutex);
            if (m_abort || m_stopPending || !m_running) {
                m_executing = false;
                break;
            }
            doExecute(flow, context);
        }
        m_executing = false;

        if (m_abort || m_stopPending || !m_running) break;

        // 轮次间隔 (期间可快速响应停止)
        int delay = flow ? flow->delayMs() : 0;
        if (delay <= 0) delay = 10;
        for (int slept = 0; m_running && !m_abort && m_stopPending == false && slept < delay; slept += 20)
            QThread::msleep(20);
    }
    m_running = false;
    m_abort = false;
    m_stopPending = false;
    emit runStateChanged(false);
    VI_LOG_INFO("连续运行已停止");
}

void FlowEngine::stopRunning(bool force) {
    if (!m_running) return;
    if (force) {
        m_abort = true;
        m_running = false;
        VI_LOG_INFO("请求强制停止 (当前工具执行完立即返回)");
    } else {
        m_stopPending = true;
        m_running = false;
        VI_LOG_INFO("请求普通停止 (执行完本轮流程后停止)");
    }
}

void FlowEngine::shutdownAndWait(int timeoutMs) {
    // 置中止标志: 单次执行 + 连续运行都在工具间隙检查 m_abort, 尽快退出
    m_abort = true;
    m_running = false;
    m_stopPending = false;

    // 等待工作线程结束 (关窗/重建项目前必须, 否则释放资源时 use-after-free)
    QFuture<void> once = m_onceFuture;
    QFuture<void> run = m_runFuture;
    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + timeoutMs;
    while ((!once.isFinished() || !run.isFinished()) &&
           QDateTime::currentMSecsSinceEpoch() < deadline) {
        QThread::msleep(20);
    }

    m_abort = false;
    VI_LOG_INFO("引擎已停止并等待工作线程结束");
}

} // namespace VisionInspector
