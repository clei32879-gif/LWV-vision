/**
 * @file FlowEngine.cpp
 * @brief 流程执行引擎实现
 */

#include "FlowEngine.h"
#include "../utils/Logger.h"
#include "../hal/ICameraDriver.h"
#include <QTimer>
#include <QCoreApplication>
#include <QDebug>

namespace VisionInspector {

// ============================================================
// Flow 实现
// ============================================================

void Flow::addTool(ITool* tool) {
    if (tool)
        m_tools.append(tool);
}

void Flow::insertTool(int index, ITool* tool) {
    if (!tool) return;
    if (index < 0 || index >= m_tools.size())
        m_tools.append(tool);
    else
        m_tools.insert(index, tool);
}

void Flow::removeTool(int index) {
    if (index >= 0 && index < m_tools.size())
        m_tools.removeAt(index);
}

void Flow::removeTool(ITool* tool) {
    m_tools.removeAll(tool);
}

void Flow::moveTool(int from, int to) {
    if (from < 0 || from >= m_tools.size()) return;
    if (to < 0 || to >= m_tools.size()) return;
    m_tools.move(from, to);
}

ITool* Flow::toolAt(int index) const {
    if (index >= 0 && index < m_tools.size())
        return m_tools[index];
    return nullptr;
}

void Flow::clear() {
    m_tools.clear();
}

QJsonObject Flow::toJson() const {
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
    m_name = json.value("name").toString();
    m_autoExecute = json.value("autoExecute").toBool(true);
    m_delayMs = json.value("delayMs").toInt(0);

    // 工具的加载需要ToolRegistry, 这里只清理, 实际创建由ProjectManager处理
    clear();
}

// ============================================================
// FlowEngine 实现
// ============================================================

FlowEngine::FlowEngine(QObject* parent)
    : QObject(parent)
{
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

bool FlowEngine::executeOnce(Flow* flow, ToolContext& context) {
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
        return false;
    }

    // 设置硬件接口到上下文
    context.setCameraDriver(m_camera);
    context.setPLCDriver(m_plc);
    context.setServoDriver(m_servo);
    context.setGlobalVariables(m_globalVars);
    context.setRunIndex(++m_runIndex);

    bool allOk = true;

    VI_LOG_INFO(QString("开始执行流程: %1 (工具数: %2)")
                .arg(flow->name())
                .arg(flow->toolCount()));

    // 按顺序执行所有工具
    for (int i = 0; i < flow->toolCount(); ++i) {
        ITool* tool = flow->toolAt(i);

        // 跳过禁用的工具
        if (!tool->isActive()) {
            emit toolStatusChanged(flow, i, ToolStatus::Disabled);
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
            ITool* prevTool = flow->toolAt(j);
            if (prevTool && prevTool->isActive()) {
                availableImages << prevTool->instanceName();
            }
        }
        context.setData("availableImages", availableImages);

        // 设置执行状态
        emit toolStatusChanged(flow, i, ToolStatus::Running);

        // 执行!
        bool success = false;
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

        // 更新状态
        ToolStatus status = success ? ToolStatus::OK : ToolStatus::NG;
        tool->setStatus(status);
        emit toolStatusChanged(flow, i, status);

        // 保存工具的输出图像到命名槽中，供后续工具引用
        CvImagePtr outputImg = context.currentImage();
        if (outputImg) {
            context.setImage(tool->instanceName(), outputImg);
        }

        if (!success) {
            allOk = false;
            // 可以选择继续执行或停止, 这里选择继续(类似CKVision默认行为)
        }
    }

    emit flowExecuted(flow, allOk);

    VI_LOG_INFO(QString("流程执行完成: %1 结果: %2")
                .arg(flow->name())
                .arg(allOk ? "全部OK" : "有NG"));

    return allOk;
}

void FlowEngine::startRunning(Flow* flow) {
    if (m_running) {
        VI_LOG_WARN("已在运行中, 忽略启动请求");
        return;
    }

    m_running = true;
    VI_LOG_INFO("开始循环运行: " + (flow ? flow->name() : QString("默认")));

    // 使用定时器循环执行
    // 实际实现中应该使用独立线程, 这里先用简单方式
    QTimer* timer = new QTimer(this);
    timer->setSingleShot(false);

    int delay = flow ? flow->delayMs() : 0;
    if (delay <= 0) delay = 10; // 最小10ms
    timer->setInterval(delay);

    Flow* targetFlow = flow;
    connect(timer, &QTimer::timeout, this, [this, targetFlow, timer]() {
        if (!m_running) {
            timer->stop();
            timer->deleteLater();
            return;
        }
        ToolContext context;
        executeOnce(targetFlow, context);
    });

    timer->start();
}

void FlowEngine::stopRunning() {
    if (!m_running) return;
    m_running = false;
    VI_LOG_INFO("停止循环运行");
}

} // namespace VisionInspector
