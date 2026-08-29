/**
 * @file GlobalVariables.h
 * @brief 全局变量管理 - 跨流程共享, 线程安全
 *
 * 存储运行时全局变量，供流程中的各个工具读取和写入。
 * 所有操作都是线程安全的 (QMutex 保护)。
 */

#pragma once
#include <QObject>
#include <QMap>
#include <QVariant>
#include <QMutex>
#include <QMutexLocker>

namespace VisionInspector {

class GlobalVariables : public QObject {
    Q_OBJECT

public:
    explicit GlobalVariables(QObject* parent = nullptr);
    ~GlobalVariables() override;

    /** 设置变量 (线程安全) */
    void set(const QString& name, const QVariant& value);

    /** 获取变量 (线程安全, 返回默认值如果不存在) */
    QVariant get(const QString& name, const QVariant& defaultValue = QVariant()) const;

    /** 检查变量是否存在 (线程安全) */
    bool has(const QString& name) const;

    /** 删除变量 (线程安全) */
    void remove(const QString& name);

    /** 清空所有变量 (线程安全) */
    void clear();

    /** 获取所有变量的快照 (线程安全) */
    QMap<QString, QVariant> all() const;

    /** 变量数量 */
    int count() const;

    /** 获取所有变量名 */
    QStringList keys() const;

    // ======== 便捷类型转换 ========

    int getInt(const QString& name, int defaultValue = 0) const;
    double getDouble(const QString& name, double defaultValue = 0.0) const;
    bool getBool(const QString& name, bool defaultValue = false) const;
    QString getString(const QString& name, const QString& defaultValue = QString()) const;

signals:
    /** 变量值变化 (在释放锁后发射) */
    void variableChanged(const QString& name, const QVariant& value);

    /** 变量被删除 */
    void variableRemoved(const QString& name);

    /** 所有变量被清空 */
    void variablesCleared();

private:
    mutable QMutex m_mutex;
    QMap<QString, QVariant> m_vars;
};

} // namespace VisionInspector
