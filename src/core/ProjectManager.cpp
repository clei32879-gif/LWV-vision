/**
 * @file ProjectManager.cpp
 * @brief 项目管理器实现 — 项目保存/加载闭环
 *
 * .vipj 文件结构 (JSON):
 * {
 *   "version": "1.0.0",
 *   "projectName": "...", "projectNote": "...",
 *   "flows": [ { "name":..., "tools": [ {typeName, instanceName, properties...} ] } ],
 *   "globalVariables": { name: value }
 * }
 */
#include "ProjectManager.h"
#include "../engine/FlowEngine.h"
#include "GlobalVariables.h"
#include "../engine/ToolRegistry.h"
#include "../utils/Logger.h"
#include "../utils/Common.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace VisionInspector {

ProjectManager::ProjectManager(QObject* parent)
    : QObject(parent)
{
}

void ProjectManager::setServices(FlowEngine* engine, GlobalVariables* globals) {
    m_engine = engine;
    m_globals = globals;
}

void ProjectManager::newProject() {
    m_currentPath.clear();
    m_projectName = QStringLiteral("新项目");
    m_projectNote.clear();
    m_modified = false;

    if (m_engine) {
        for (Flow* f : m_engine->flows()) {
            f->clear();
            m_engine->removeFlow(f);
            // 先脱离 QObject 父子关系再延迟删除, 否则引擎析构时会二次释放 (双重释放崩溃)
            f->setParent(nullptr);
            f->deleteLater();
        }
    }
    if (m_globals) m_globals->clear();

    emit projectModified();
    VI_LOG_INFO("新项目已创建");
}

bool ProjectManager::loadProject(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        VI_LOG_ERROR("Cannot open project file: " + path);
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    if (error.error != QJsonParseError::NoError) {
        VI_LOG_ERROR("Project file format error: " + error.errorString());
        return false;
    }

    const QJsonObject json = doc.object();
    m_projectName = json.value("projectName").toString("Untitled");
    m_projectNote = json.value("projectNote").toString();

    // 重建流程与工具
    if (m_engine) {
        for (Flow* f : m_engine->flows()) {
            f->clear();
            m_engine->removeFlow(f);
            // 先脱离 QObject 父子关系再延迟删除, 避免引擎析构时二次释放
            f->setParent(nullptr);
            f->deleteLater();
        }
        const QJsonArray flows = json.value("flows").toArray();
        int rebuiltTools = 0, missingTypes = 0;
        for (const QJsonValue& fv : flows) {
            const QJsonObject flowJson = fv.toObject();
            Flow* flow = new Flow(m_engine);
            flow->setName(flowJson.value("name").toString(QStringLiteral("流程%1")
                                                             .arg(m_engine->flowCount() + 1)));
            flow->setAutoExecute(flowJson.value("autoExecute").toBool(true));
            flow->setDelayMs(flowJson.value("delayMs").toInt(0));

            const QJsonArray tools = flowJson.value("tools").toArray();
            for (const QJsonValue& tv : tools) {
                const QJsonObject toolJson = tv.toObject();
                const QString typeName = toolJson.value("typeName").toString();
                ITool* tool = ToolRegistry::instance().createTool(typeName);
                if (!tool) {
                    VI_LOG_WARN(QString("项目中的工具类型未注册, 跳过: %1").arg(typeName));
                    ++missingTypes;
                    continue;
                }
                tool->fromJson(toolJson);
                flow->addTool(tool);
                ++rebuiltTools;
            }
            m_engine->addFlow(flow);
        }
        VI_LOG_INFO(QString("流程重建完成: %1个流程, %2个工具 (缺失类型%3个)")
                    .arg(flows.size()).arg(rebuiltTools).arg(missingTypes));
    }

    // 全局变量
    if (m_globals) {
        m_globals->clear();
        const QJsonObject gv = json.value("globalVariables").toObject();
        for (auto it = gv.begin(); it != gv.end(); ++it)
            m_globals->set(it.key(), it.value().toVariant());
    }

    m_currentPath = path;
    m_modified = false;

    emit projectLoaded(path);
    VI_LOG_INFO("Project loaded: " + path);
    return true;
}

bool ProjectManager::saveProject(const QString& path) {
    QJsonObject json;
    json["projectName"] = m_projectName;
    json["projectNote"] = m_projectNote;
    json["version"] = versionString();
    json["savedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    // 全部流程与工具
    if (m_engine) {
        QJsonArray flows;
        for (Flow* f : m_engine->flows())
            flows.append(f->toJson());
        json["flows"] = flows;
    }

    // 全局变量
    if (m_globals) {
        // 注意: all() 返回副本(临时QMap), 必须先拷贝再迭代,
        // 不能直接在临时对象上取 begin()/end() (悬垂迭代器, 正常堆下崩溃/挂死)
        const QMap<QString, QVariant> allmap = m_globals->all();
        QJsonObject gv;
        for (auto it = allmap.begin(); it != allmap.end(); ++it)
            gv[it.key()] = QJsonValue::fromVariant(it.value());
        json["globalVariables"] = gv;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        VI_LOG_ERROR("Cannot write project file: " + path);
        return false;
    }
    file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
    file.close();

    m_currentPath = path;
    m_modified = false;

    emit projectSaved(path);
    VI_LOG_INFO("Project saved: " + path);
    return true;
}

} // namespace VisionInspector
