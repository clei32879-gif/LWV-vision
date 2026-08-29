/**
 * @file DataModel.h
 * @brief 数据流模型 - 工具间数据传递
 *
 * 封装 ToolContext, 提供类型安全的访问接口。
 * 支持的数据类型: int, double, QString, QImage, QPoint, QRect, QPolygon, bool。
 *
 * 数据命名规范: "toolName.dataKey"
 * 例如: "BlobAnalysis.area", "Caliper.distance"
 */

#pragma once
#include <QObject>
#include <QString>
#include <QVariant>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QPolygon>
#include <QMap>
#include <QMutex>

namespace VisionInspector {

class ToolContext;

/** 数据条目元信息 */
struct DataEntry {
    QString toolName;   // 来源工具名
    QString dataKey;    // 数据键
    QVariant value;     // 数据值
    int typeId;         // QMetaType::Type

    /** 构建完整数据名 "toolName.dataKey" */
    QString fullKey() const { return toolName + "." + dataKey; }
};

class DataModel : public QObject {
    Q_OBJECT

public:
    explicit DataModel(QObject* parent = nullptr);
    ~DataModel() override;

    // ======== 数据写入 ========

    /** 存储工具输出数据 */
    void setData(const QString& toolName, const QString& dataKey, const QVariant& value);

    /** 便捷方法 - 各类型 */
    void setInt(const QString& toolName, const QString& key, int value);
    void setDouble(const QString& toolName, const QString& key, double value);
    void setString(const QString& toolName, const QString& key, const QString& value);
    void setBool(const QString& toolName, const QString& key, bool value);
    void setImage(const QString& toolName, const QString& key, const QImage& value);
    void setPoint(const QString& toolName, const QString& key, const QPoint& value);
    void setRect(const QString& toolName, const QString& key, const QRect& value);
    void setPolygon(const QString& toolName, const QString& key, const QPolygon& value);

    // ======== 数据读取 ========

    /** 读取数据 (按完整名 "toolName.dataKey") */
    QVariant getData(const QString& fullKey, const QVariant& defaultValue = QVariant()) const;

    /** 按工具名+键读取 */
    QVariant getData(const QString& toolName, const QString& dataKey,
                     const QVariant& defaultValue = QVariant()) const;

    /** 便捷方法 - 各类型 */
    int getInt(const QString& toolName, const QString& key, int defaultValue = 0) const;
    double getDouble(const QString& toolName, const QString& key, double defaultValue = 0.0) const;
    QString getString(const QString& toolName, const QString& key,
                      const QString& defaultValue = QString()) const;
    bool getBool(const QString& toolName, const QString& key, bool defaultValue = false) const;
    QImage getImage(const QString& toolName, const QString& key) const;
    QPoint getPoint(const QString& toolName, const QString& key) const;
    QRect getRect(const QString& toolName, const QString& key) const;
    QPolygon getPolygon(const QString& toolName, const QString& key) const;

    // ======== 数据查询 ========

    /** 检查数据是否存在 */
    bool hasData(const QString& toolName, const QString& dataKey) const;

    /** 获取某个工具输出的所有数据 */
    QMap<QString, QVariant> toolOutputs(const QString& toolName) const;

    /** 获取所有数据条目 */
    QList<DataEntry> allEntries() const;

    /** 数据条目数量 */
    int entryCount() const;

    /** 获取所有工具名列表 */
    QStringList toolNames() const;

    /** 获取某个工具的所有数据键 */
    QStringList dataKeys(const QString& toolName) const;

    // ======== 批量操作 ========

    /** 从 ToolContext 导入所有数据 */
    void importFromContext(const ToolContext& context, const QString& toolName);

    /** 导出到 ToolContext */
    void exportToContext(ToolContext& context) const;

    /** 清空所有数据 */
    void clear();

    /** 清空指定工具的数据 */
    void clearTool(const QString& toolName);

    // ======== 序列化 ========

    /** 导出为 JSON 兼容的 QVariantMap */
    QVariantMap toVariantMap() const;

    /** 从 QVariantMap 恢复 */
    void fromVariantMap(const QVariantMap& map);

signals:
    /** 数据被写入 */
    void dataSet(const QString& toolName, const QString& dataKey, const QVariant& value);

    /** 数据被清除 */
    void dataCleared(const QString& toolName);

    /** 所有数据被清空 */
    void allDataCleared();

private:
    /** 构建内部存储键 */
    static QString makeKey(const QString& toolName, const QString& dataKey);

    /** 验证数据类型 */
    bool isValidType(const QVariant& value) const;

    mutable QMutex m_mutex;
    QMap<QString, DataEntry> m_entries; // key = "toolName.dataKey"
};

} // namespace VisionInspector
