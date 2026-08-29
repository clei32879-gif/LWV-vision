/** @file DataJudgment.h - 数据判断系统（上下限验证） */
#pragma once
#include <QMap>
#include <QString>
#include <QVariant>
#include <QColor>

namespace VisionInspector {

/**
 * 数据判断结果
 */
enum class JudgmentResult {
    OK,         // 在范围内
    NG,         // 超出范围
    NoLimit,    // 未设置限制
    Invalid     // 数据无效
};

/**
 * 单个数据的判断配置
 */
struct DataJudgmentItem {
    QString name;           // 数据名称
    QString dataKey;        // 数据链接键
    double lowerLimit = -99999;  // 下限
    double upperLimit = 99999;   // 上限
    bool enabled = false;   // 是否启用判断
    double value = 0;       // 当前值
    JudgmentResult result = JudgmentResult::NoLimit;
};

/**
 * 数据判断管理器
 */
class DataJudgmentManager {
public:
    /**
     * 添加判断项
     */
    void addItem(const QString& name, const QString& dataKey,
                 double lower = -99999, double upper = 99999, bool enabled = false);
    
    /**
     * 移除判断项
     */
    void removeItem(const QString& name);
    
    /**
     * 更新判断项
     */
    void updateItem(const QString& name, double lower, double upper, bool enabled);
    
    /**
     * 获取所有判断项
     */
    QVector<DataJudgmentItem> items() const { return m_items; }
    
    /**
     * 执行判断（从上下文获取数据并验证）
     */
    bool judge(const QMap<QString, QVariant>& data);
    
    /**
     * 获取综合状态
     */
    JudgmentResult overallResult() const { return m_overallResult; }
    
    /**
     * 获取综合状态字符串
     */
    QString overallResultString() const;
    
    /**
     * 从工具属性创建
     */
    static DataJudgmentManager fromProperties(const QMap<QString, QVariant>& props);
    
    /**
     * 转换为工具属性
     */
    QMap<QString, QVariant> toProperties() const;

private:
    QVector<DataJudgmentItem> m_items;
    JudgmentResult m_overallResult = JudgmentResult::NoLimit;
};

} // namespace VisionInspector
