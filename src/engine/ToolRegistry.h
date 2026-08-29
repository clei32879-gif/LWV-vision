/**
 * @file ToolRegistry.h
 * @brief 工具注册表
 *
 * 管理所有已注册的工具类型。
 * 插件通过 VI_REGISTER_TOOL 宏注册工具, 主程序通过注册表查找和创建工具。
 *
 * 使用方式:
 *   // 注册 (在插件中)
 *   VI_REGISTER_TOOL(BlobAnalysis, "BLOB分析", ToolCategory::Detection)
 *
 *   // 创建 (在主程序中)
 *   auto tool = ToolRegistry::instance().createTool("BlobAnalysis");
 *   if (tool) tool->execute(context);
 *
 *   // 枚举所有工具 (用于工具箱面板)
 *   auto metas = ToolRegistry::instance().allMetaData();
 *   for (const auto& meta : metas) {
 *       qDebug() << meta.typeName << meta.displayName;
 *   }
 */

#pragma once

#include "ITool.h"
#include <QMap>
#include <QList>
#include <functional>
#include <mutex>

namespace VisionInspector {

class ToolRegistry {
public:
    static ToolRegistry& instance();

    /**
     * 注册工具类型
     * @param typeName 类型名 (如 "BlobAnalysis")
     * @param meta 元数据
     * @return true=成功, false=已存在
     */
    bool registerTool(const QString& typeName, const ToolMetaData& meta);

    /**
     * 注销工具类型
     */
    bool unregisterTool(const QString& typeName);

    /**
     * 创建工具实例
     * @param typeName 类型名
     * @return 工具指针 (调用者拥有所有权), 失败返回nullptr
     */
    ITool* createTool(const QString& typeName) const;

    /**
     * 获取工具元数据
     */
    ToolMetaData metaData(const QString& typeName) const;

    /**
     * 获取所有注册的元数据
     */
    QList<ToolMetaData> allMetaData() const;

    /**
     * 获取指定分类的所有工具
     */
    QList<ToolMetaData> metaDataByCategory(ToolCategory category) const;

    /**
     * 是否已注册
     */
    bool isRegistered(const QString& typeName) const;

    /**
     * 已注册工具数量
     */
    int count() const;

    /**
     * 清空所有注册 (程序退出时调用)
     */
    void clear();

private:
    ToolRegistry() = default;

    mutable std::mutex m_mutex;
    QMap<QString, ToolMetaData> m_registry;
};

} // namespace VisionInspector
