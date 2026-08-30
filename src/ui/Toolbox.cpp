#include "Toolbox.h"
#include "../engine/ToolRegistry.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMimeData>

namespace VisionInspector {

// 支持把工具拖拽到流程编辑器的树控件
class ToolTreeWidget : public QTreeWidget {
public:
    explicit ToolTreeWidget(QWidget* parent = nullptr) : QTreeWidget(parent) {
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
    }

protected:
    // 拖拽时携带工具类型名(自定义MIME), 供流程编辑器识别
    QMimeData* mimeData(const QList<QTreeWidgetItem*>& items) const override {
        if (items.isEmpty()) return nullptr;
        const QVariant v = items.first()->data(0, Qt::UserRole);
        if (v.isNull()) return nullptr;
        auto* mime = new QMimeData();
        mime->setData(QStringLiteral("application/x-lwvision-tool"), v.toString().toUtf8());
        mime->setText(items.first()->text(0));
        return mime;
    }
};

Toolbox::Toolbox(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_tree = new ToolTreeWidget(this);
    m_tree->setHeaderLabel(QStringLiteral("工具列表"));
    m_tree->setStyleSheet("background-color: #2b2b2b; color: #ddd;");
    layout->addWidget(m_tree, 1);

    auto* hint = new QLabel(QStringLiteral("提示：双击工具，或拖拽工具到右侧流程图，即可添加"), this);
    hint->setStyleSheet("color: #9aa0a6; font-size: 12px; padding: 3px;");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    connect(m_tree, &QTreeWidget::itemDoubleClicked, [this](QTreeWidgetItem* item) {
        if (item && !item->data(0, Qt::UserRole).isNull()) {
            emit toolDoubleClicked(item->data(0, Qt::UserRole).toString());
        }
    });

    refreshTools();
}

void Toolbox::refreshTools() {
    m_tree->clear();

    // 先按分类归集, 只显示"有工具"的分类 (隐藏空的"系统工具/三维测量"等)
    const auto metas = ToolRegistry::instance().allMetaData();
    QMap<ToolCategory, QList<ToolMetaData>> byCategory;
    for (const auto& meta : metas) {
        byCategory[meta.category].append(meta);
    }

    for (int c = 0; c <= static_cast<int>(ToolCategory::Special); ++c) {
        const auto cat = static_cast<ToolCategory>(c);
        if (!byCategory.contains(cat)) continue;

        auto* group = new QTreeWidgetItem(m_tree, QStringList{categoryToString(cat)});
        group->setExpanded(true);
        QFont f = group->font(0); f.setBold(true); group->setFont(0, f);

        for (const auto& meta : byCategory[cat]) {
            auto* item = new QTreeWidgetItem(group, QStringList{meta.displayName});
            item->setData(0, Qt::UserRole, meta.typeName);
            item->setToolTip(0, meta.typeName);
        }
    }

    m_tree->expandAll();
}

} // namespace VisionInspector
