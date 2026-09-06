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
#include "ROI.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#endif
#include <QObject>
#include <QString>
#include <QVariant>
#include <QIcon>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QMap>
#include <memory>
#include <functional>
#include <mutex>
#include <mutex>

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
// 结果判定 (对齐CKVision: 工具内直接配上下限, NG不依赖外部工具)
// ============================================================
struct ResultJudgment {
    QString resultKey;          // 判定的结果键 (如 "blobCount")
    bool enabled = false;       // 是否启用
    double lower = -1e18;       // 下限
    double upper = 1e18;        // 上限
};

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

    ToolStatus status() const {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        return m_status;
    }

    // --- Property system ---

    // 基础属性（所有工具都有）
    static PropertyDefList basePropertyDefs() {
        return {
            PropertyDef::enumProp("inputImage", "输入图像",
                {"当前图像", "相机图像", "文件图像", "自定义"}, 0),
        };
    }

    // 该工具是否需要输入图像(用于自动补"采集图像"与属性界面是否显示"输入图像")
    static bool needsInputImage(const QString& typeName) {
        static const QStringList noImageTools = {
            QStringLiteral("CaptureImage"),
            QStringLiteral("PositionCorrection"),
            QStringLiteral("EndCorrection"),
            QStringLiteral("CoordSystem"),
            QStringLiteral("Calibration"),
            QStringLiteral("CalculateVariable"),
            QStringLiteral("SetVariable"),
            QStringLiteral("Calculator"),
            QStringLiteral("DataJudge"),
            QStringLiteral("ConditionBranch"),
            QStringLiteral("Delay"),
            QStringLiteral("Loop"),
            QStringLiteral("EthernetTool"),
            QStringLiteral("SerialPort"),
            QStringLiteral("SerialPortTool"),
            QStringLiteral("ModbusComm"),
            QStringLiteral("ModbusRead"),
            QStringLiteral("ModbusWrite"),
            QStringLiteral("PlcLink"),
            QStringLiteral("DataDisplay"),
            QStringLiteral("UpdateView"),
        };
        return !noImageTools.contains(typeName);
    }
    
    virtual PropertyDefList propertyDefs() const { return {}; }
    virtual QVariant propertyValue(const QString& name) const;
    virtual void setProperty(const QString& name, const QVariant& value);
    // 锁内快照返回副本 (H-5: UI线程读与工作线程写不竞态)
    QVariantMap properties() const {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        return m_properties;
    }

    // --- Execution (core!) ---

    virtual bool execute(ToolContext& context) = 0;
    // 锁内快照返回副本 (H-5: 结果键枚举不读半写状态)
    DataMap resultData() const {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        return m_resultData;
    }
    virtual std::vector<QVariant> overlays() const { return {}; }

    // --- 结果判定 (上下限, 执行后由流程引擎调用) ---

    QList<ResultJudgment> judgments() const {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        return m_judgments;
    }
    void setJudgments(const QList<ResultJudgment>& j) {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        m_judgments = j;
    }

    /** 是否有启用的判定 */
    bool hasJudgments() const {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        for (const auto& j : m_judgments)
            if (j.enabled) return true;
        return false;
    }

    /**
     * 执行后判定: 所有启用的判定都在限内返回true。
     * 结果键不存在或非数值视为NG; 失败键写入 resultData["judgeFailedKeys"]。
     */
    bool evaluateJudgments();

    // --- Serialization ---

    virtual QJsonObject toJson() const;
    virtual void fromJson(const QJsonObject& json);

public:
    void setStatus(ToolStatus status) {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        m_status = status;
    }

protected:
    void setResultData(const QString& key, const QVariant& value) {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        m_resultData[key] = value;
    }

    CvImagePtr getInputImage(const ToolContext& context) const;
    void setOutputImage(ToolContext& context, const CvImagePtr& img);

#ifdef VI_HAS_OPENCV
    /**
     * ROI形状掩码 (#2 菱形/圆形/环形ROI真实生效):
     * 按 roi.type 生成与 roiRect 同尺寸的掩码:
     *   Rectangle=全白 / Diamond=菱形 / Circle=椭圆(宽高) / Ring=环带(外椭圆-内椭圆)
     * 返回空 Mat 表示矩形(无需掩码)。工具把生成的掩码与结果掩码按位与即可。
     * roiRect 为工具已裁剪的局部ROI外接矩形; roi 为完整ROI定义(坐标系一致)。
     */
    static cv::Mat makeRoiShapeMask(const ROIRegion& roi, const QRectF& roiRect);

    /**
     * 模板图像缓存 (#8 性能): 匹配类工具每帧都读模板文件是纯磁盘IO浪费。
     * 按 路径+flags 缓存 imread 结果; 文件修改时间变化时自动重读。
     * 返回的 Mat 为缓存共享对象, 调用方不得就地修改 (需要修改时自行clone)。
     */
    static cv::Mat cachedTemplateImage(const QString& path, int flags = cv::IMREAD_GRAYSCALE);
#endif

    /**
     * 位置补正跟随 (P0-1 数据流打通):
     * 当本工具开启 useCorrection 属性 且 上下文存在补正矩阵
     * (coord_cos/sin/originX/originY, 由 位置补正/坐标系统 工具写入) 时,
     * 把"模板/基准坐标系"下定义的点 (x,y) 与角度 angleDeg(度) 变换到当前图像坐标系。
     * 返回 true 表示已应用补正; false 表示未开启或无补正数据。
     * 工具调用方式: 在读取 ROI 属性后, 若返回 true 用变换后的值继续执行。
     */
    bool applyCorrection(const ToolContext& context,
                         double& x, double& y, double& angleDeg) const;

protected:
    QVariantMap m_properties;

    // H-4b 带锁辅助: 供覆写 propertyValue/setProperty 的派生类(YOLOv8Detect等)
    // 复用基类锁, 避免覆写实现绕过 m_stateMutex 造成线程安全缺口
    QVariant propertyValueLocked(const QString& name) const {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        return m_properties.value(name);
    }
    void setPropertyLocked(const QString& name, const QVariant& value) {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        m_properties[name] = value;
    }
    // 在基类状态锁内执行一段代码 (覆写实现需要原子更新"自身成员+基类属性"时用)
    template <typename F>
    auto withStateLock(F&& f) const -> decltype(f()) {
        std::lock_guard<std::mutex> lk(m_stateMutex);
        return f();
    }

private:
    QString m_instanceName;
    QString m_comment;
    bool m_active = true;
    ToolStatus m_status = ToolStatus::Idle;
    DataMap m_resultData;
    QList<ResultJudgment> m_judgments;   // 工具内建上下限判定
    // H-4/H-5 线程安全: 保护 m_properties/m_resultData/m_status/m_judgments
    // (工作线程 execute 写 vs UI 线程属性对话框/结果枚举读)
    mutable std::mutex m_stateMutex;
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
    // 中文说明(工具箱悬浮/属性对话框展示); 宏注册留空, 由 ToolRegistry 说明表填充
    QString description;
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
