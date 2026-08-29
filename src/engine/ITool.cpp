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