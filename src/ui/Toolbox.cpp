#include "Toolbox.h"
#include "../engine/ToolRegistry.h"
#include <QVBoxLayout>
#include <QHeaderView>

namespace VisionInspector {

Toolbox::Toolbox(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabel(QStringLiteral("工具列表"));
    m_tree->setDragEnabled(false);  // 暂时禁用拖拽，使用双击添加
    m_tree->setStyleSheet("background-color: #2b2b2b; color: #ddd;");
    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemDoubleClicked, [this](QTreeWidgetItem* item) {
        if (item && !item->data(0, Qt::UserRole).isNull()) {
            emit toolDoubleClicked(item->data(0, Qt::UserRole).toString());
        }
    });

    refreshTools();
}

void Toolbox::refreshTools() {
    m_tree->clear();

    // 按分类分组显示
    QMap<ToolCategory, QTreeWidgetItem*> groupItems;
    for (int c = 0; c <= static_cast<int>(ToolCategory::Special); ++c) {
        auto cat = static_cast<ToolCategory>(c);
        auto* group = new QTreeWidgetItem(m_tree, QStringList{categoryToString(cat)});
        group->setExpanded(true);
        QFont f = group->font(0); f.setBold(true); group->setFont(0, f);
        groupItems[cat] = group;
    }

    auto metas = ToolRegistry::instance().allMetaData();
    for (const auto& meta : metas) {
        auto* item = new QTreeWidgetItem(groupItems[meta.category],
                                         QStringList{meta.displayName});
        item->setData(0, Qt::UserRole, meta.typeName);
        item->setToolTip(0, meta.typeName);
    }

    m_tree->expandAll();
}

} // namespace VisionInspector
