/**
 * @file ITool.h
 * @brief Tool base interface - core of the entire project
 *
 * All detection tools (BLOB analysis, edge detection, circle detection...)
 * inherit from ITool.
 *
 * Tool lifecycle:
 *   1. Create -> setInstanceName()
 *   2. Configure -> setProperty() / user sets params in property panel
 *   3. Execute -> execute() / called by flow engine
 *   4. Get results -> resultData()
 */

#pragma once

#include "../utils/Common.h"
#include "ToolContext.h"
#include <QObject>
#include <QString>
#include <QVariant>
#include <QIcon>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <functional>

namespace VisionInspector {

class ToolRegistry;

// ============================================================
// Property definitions (for property panel)
// ============================================================

enum class PropertyType {
    Int,
    Double,
    String,
    Boolean,
    Enum,
    Rect,
    Point,
    Color
};

struct PropertyDef {
    QString name;
    QString label;
    PropertyType type;
    QVariant defaultValue;
    QVariant minValue;
    QVariant maxValue;
    QStringList enumValues;
    QString group;
    QString tooltip;

    static PropertyDef intProp(const QString& name, const QString& label,
                               int def, int min = 0, int max = INT_MAX,
                               const QString& group = "") {
        PropertyDef p;
        p.name = name; p.label = label; p.type = PropertyType::Int;
        p.defaultValue = def; p.minValue = min; p.maxValue = max;
        p.group = group;
        return p;
    }

    static PropertyDef doubleProp(const QString& name, const QString& label,
                                  double def, double min = 0.0, double max = 1e9,
                                  const QString& group = "") {
        PropertyDef p;
        p.name = name; p.label = label; p.type = PropertyType::Double;
        p.defaultValue = def; p.minValue = min; p.maxValue = max;
        p.group = group;
        return p;
    }

    static PropertyDef boolProp(const QString& name, const QString& label,
                                bool def, const QString& group = "") {
        PropertyDef p;
        p.name = name; p.label = label; p.type = PropertyType::Boolean;
        p.defaultValue = def; p.group = group;
        return p;
    }

    static PropertyDef enumProp(const QString& name, const QString& label,
                                const QStringList& options, int defIndex = 0,
                                const QString& group = "") {
        PropertyDef p;
        p.name = name; p.label = label; p.type = PropertyType::Enum;
        p.enumValues = options; p.defaultValue = defIndex; p.group = group;
        return p;
    }

    static PropertyDef stringProp(const QString& name, const QString& label,
                                  const QString& def = QString(),
                                  const QString& group = "") {
        PropertyDef p;
        p.name = name; p.label = label; p.type = PropertyType::String;
        p.defaultValue = def; p.group = group;
        return p;
    }
};

using PropertyDefList = std::vector<PropertyDef>;

// ============================================================
// ITool - base class for all tools
// ============================================================

class ITool : public QObject {
    Q_OBJECT

public:
    ITool(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~ITool() = default;

    // --- Basic info (subclasses must implement) ---

    virtual QString typeName() const = 0;
    virtual QString displayName() const = 0;
    virtual ToolCategory category() const = 0;
    virtual QIcon icon() const { return QIcon(); }
    virtual QString description() const { return QString(); }

    // --- Instance info ---

    QString instanceName() const { return m_instanceName; }
    void setInstanceName(const QString& name) { m_instanceName = name; }

    QString comment() const { return m_comment; }
    void setComment(const QString& comment) { m_comment = comment; }

    bool isActive() const { return m_active; }
    void setActive(bool active) { m_active = active; }

    ToolStatus status() const { return m_status; }

    // --- Property system ---

    // 基础属性（所有工具都有）
    static PropertyDefList basePropertyDefs() {
        return {
            PropertyDef::enumProp("inputImage", "输入图像",
                {"当前图像", "相机图像", "文件图像", "自定义"}, 0),
        };
    }
    
    virtual PropertyDefList propertyDefs() const { return {}; }
    virtual QVariant propertyValue(const QString& name) const;
    virtual void setProperty(const QString& name, const QVariant& value);
    const QVariantMap& properties() const { return m_properties; }

    // --- Execution (core!) ---

    virtual bool execute(ToolContext& context) = 0;
    const DataMap& resultData() const { return m_resultData; }
    virtual std::vector<QVariant> overlays() const { return {}; }

    // --- Serialization ---

    virtual QJsonObject toJson() const;
    virtual void fromJson(const QJsonObject& json);

public:
    void setStatus(ToolStatus status) { m_status = status; }

protected:
    void setResultData(const QString& key, const QVariant& value) {
        m_resultData[key] = value;
    }

    CvImagePtr getInputImage(const ToolContext& context) const;
    void setOutputImage(ToolContext& context, const CvImagePtr& img);

protected:
    QVariantMap m_properties;

private:
    QString m_instanceName;
    QString m_comment;
    bool m_active = true;
    ToolStatus m_status = ToolStatus::Idle;
    DataMap m_resultData;
};

// ============================================================
// Tool factory (for plugin registration)
// ============================================================

using ToolCreateFunc = std::function<ITool*()>;

struct ToolMetaData {
    QString typeName;
    QString displayName;
    ToolCategory category;
    ToolCreateFunc createFunc;
};

} // namespace VisionInspector

// ============================================================
// Tool registration macro (used in plugins)
// ============================================================

#define VI_REGISTER_TOOL(ClassName, DisplayName, Category) \
    static VisionInspector::ITool* create##ClassName() { return new VisionInspector::ClassName(); } \
    static VisionInspector::ToolMetaData meta##ClassName = { \
        #ClassName, \
        DisplayName, \
        Category, \
        create##ClassName \
    }; \
    static bool registered##ClassName = \
        VisionInspector::ToolRegistry::instance().registerTool(#ClassName, meta##ClassName);
