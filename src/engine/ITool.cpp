#include "ITool.h"
#include "ToolRegistry.h"
#include <QJsonArray>
#include <QDebug>

namespace VisionInspector {

QVariant ITool::propertyValue(const QString& name) const {
    if (m_properties.contains(name))
        return m_properties.value(name);
    for (const auto& def : propertyDefs()) {
        if (def.name == name)
            return def.defaultValue;
    }
    return QVariant();
}

void ITool::setProperty(const QString& name, const QVariant& value) {
    m_properties[name] = value;
}

QJsonObject ITool::toJson() const {
    QJsonObject json;
    json["typeName"] = typeName();
    json["instanceName"] = m_instanceName;
    json["comment"] = m_comment;
    json["active"] = m_active;
    QJsonObject propsJson;
    for (const auto& def : propertyDefs()) {
        if (def.type != PropertyType::Rect) {
            propsJson[def.name] = QJsonValue::fromVariant(def.defaultValue);
        }
    }
    for (auto it = m_properties.begin(); it != m_properties.end(); ++it) {
        propsJson[it.key()] = QJsonValue::fromVariant(it.value());
    }
    json["properties"] = propsJson;

    // 结果判定
    QJsonArray judgeArray;
    for (const auto& j : m_judgments) {
        judgeArray.append(QJsonObject{
            {"key", j.resultKey}, {"enabled", j.enabled},
            {"lower", j.lower}, {"upper", j.upper}});
    }
    json["judgments"] = judgeArray;
    return json;
}

void ITool::fromJson(const QJsonObject& json) {
    m_instanceName = json.value("instanceName").toString();
    m_comment = json.value("comment").toString();
    m_active = json.value("active").toBool(true);
    QJsonObject propsJson = json.value("properties").toObject();
    for (auto it = propsJson.begin(); it != propsJson.end(); ++it) {
        m_properties[it.key()] = it.value().toVariant();
    }
    m_judgments.clear();
    const QJsonArray judgeArray = json.value("judgments").toArray();
    for (const auto& v : judgeArray) {
        const QJsonObject o = v.toObject();
        ResultJudgment j;
        j.resultKey = o.value("key").toString();
        j.enabled = o.value("enabled").toBool(false);
        j.lower = o.value("lower").toDouble(-1e18);
        j.upper = o.value("upper").toDouble(1e18);
        if (!j.resultKey.isEmpty())
            m_judgments.append(j);
    }
}

bool ITool::evaluateJudgments() {
    QStringList failed;
    for (const auto& j : m_judgments) {
        if (!j.enabled) continue;
        if (!m_resultData.contains(j.resultKey)) {
            failed << j.resultKey;
            continue;
        }
        bool ok = false;
        const double v = m_resultData.value(j.resultKey).toDouble(&ok);
        if (!ok || v < j.lower || v > j.upper)
            failed << j.resultKey;
    }
    if (failed.isEmpty()) return true;
    m_resultData["judgeFailedKeys"] = failed.join(",");
    return false;
}

CvImagePtr ITool::getInputImage(const ToolContext& context) const {
    // 检查是否有指定的输入图像
    QString inputKey = propertyValue("inputImage").toString();
    if (!inputKey.isEmpty() && inputKey != "Current" && inputKey != "<NULL>") {
        // 从命名图像槽中获取
        CvImagePtr img = context.getImage(inputKey);
        if (img) return img;
    }
    // 默认获取当前图像
    return context.currentImage();
}

void ITool::setOutputImage(ToolContext& context, const CvImagePtr& img) {
    context.setCurrentImage(img);
}

} // namespace VisionInspector