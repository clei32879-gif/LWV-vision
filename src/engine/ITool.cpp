#include "ITool.h"
#include "ToolRegistry.h"
#include <QJsonArray>
#include <QDebug>
#include <cmath>

namespace VisionInspector {

QVariant ITool::propertyValue(const QString& name) const {
    std::lock_guard<std::mutex> lk(m_stateMutex);
    if (m_properties.contains(name))
        return m_properties.value(name);
    for (const auto& def : propertyDefs()) {
        if (def.name == name)
            return def.defaultValue;
    }
    return QVariant();
}

void ITool::setProperty(const QString& name, const QVariant& value) {
    std::lock_guard<std::mutex> lk(m_stateMutex);
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
    {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        for (auto it = m_properties.begin(); it != m_properties.end(); ++it) {
            propsJson[it.key()] = QJsonValue::fromVariant(it.value());
        }
    }
    json["properties"] = propsJson;

    // 结果判定
    QJsonArray judgeArray;
    QList<ResultJudgment> judges = judgments();
    for (const auto& j : judges) {
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
    {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        for (auto it = propsJson.begin(); it != propsJson.end(); ++it) {
            m_properties[it.key()] = it.value().toVariant();
        }
    }
    QList<ResultJudgment> jl;
    const QJsonArray judgeArray = json.value("judgments").toArray();
    for (const auto& v : judgeArray) {
        const QJsonObject o = v.toObject();
        ResultJudgment j;
        j.resultKey = o.value("key").toString();
        j.enabled = o.value("enabled").toBool(false);
        j.lower = o.value("lower").toDouble(-1e18);
        j.upper = o.value("upper").toDouble(1e18);
        if (!j.resultKey.isEmpty())
            jl.append(j);
    }
    setJudgments(jl);
}

bool ITool::evaluateJudgments() {
    QStringList failed;
    QList<ResultJudgment> judges = judgments();
    for (const auto& j : judges) {
        if (!j.enabled) continue;
        const QVariant rv = resultData().value(j.resultKey);
        if (!rv.isValid()) {
            failed << j.resultKey;
            continue;
        }
        bool ok = false;
        const double v = rv.toDouble(&ok);
        if (!ok || v < j.lower || v > j.upper)
            failed << j.resultKey;
    }
    if (failed.isEmpty()) return true;
    setResultData("judgeFailedKeys", failed.join(","));
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

bool ITool::applyCorrection(const ToolContext& context,
                            double& x, double& y, double& angleDeg) const {
    // 未开启跟随则原样返回
    if (!propertyValue("useCorrection").toBool()) return false;

    // 上下文不存在补正数据 (位置补正/坐标系统未执行, 或已被结束补正清除)
    if (!context.hasData("coord_cos") || !context.hasData("coord_sin") ||
        !context.hasData("coord_originX") || !context.hasData("coord_originY"))
        return false;

    // 补正矩阵: 把模板/基准坐标映射到图像坐标
    //   x_img = originX + x*cos - y*sin
    //   y_img = originY + x*sin + y*cos
    const double c = context.getDouble("coord_cos", 1.0);
    const double s = context.getDouble("coord_sin", 0.0);
    const double ox = context.getDouble("coord_originX", 0.0);
    const double oy = context.getDouble("coord_originY", 0.0);

    // 单位矩阵 (无实际偏移/旋转) 时无需处理
    if (std::fabs(c - 1.0) < 1e-9 && std::fabs(s) < 1e-9 &&
        std::fabs(ox) < 1e-9 && std::fabs(oy) < 1e-9)
        return false;

    const double nx = ox + x * c - y * s;
    const double ny = oy + x * s + y * c;
    x = nx;
    y = ny;

    // 角度跟随: 基准角度 + 补正角度
    const double corrAngle = context.getDouble("coord_angle", 0.0);
    angleDeg += corrAngle;
    return true;
}

} // namespace VisionInspector