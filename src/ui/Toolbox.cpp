#include "Toolbox.h"
#include "IconHelper.h"
#include "../engine/ToolRegistry.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMimeData>
#include <QLineEdit>

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

    // 搜索框: 按显示名/类型名实时过滤 (80+ 工具全靠翻树找不到)
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("搜索工具..."));
    m_search->setClearButtonEnabled(true);
    layout->addWidget(m_search);

    m_tree = new ToolTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setStyleSheet("background-color: #dcebf8; color: #ddd;");
    layout->addWidget(m_tree, 1);

    auto* hint = new QLabel(QStringLiteral("提示：双击工具，或拖拽工具到右侧流程图，即可添加"), this);
    hint->setStyleSheet("color: #5a7a9c; font-size: 12px; padding: 3px;");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    connect(m_search, &QLineEdit::textChanged, this, &Toolbox::applyFilter);
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
        group->setIcon(0, IconHelper::categoryIcon(cat, 18));

        for (const auto& meta : byCategory[cat]) {
            auto* item = new QTreeWidgetItem(group, QStringList{meta.displayName});
            item->setData(0, Qt::UserRole, meta.typeName);
            item->setData(0, Qt::UserRole + 1, meta.description); // 供搜索匹配描述关键词
            // 悬浮提示: 工具名 + 说明 + 所属分类 + 类型名(供属性引用 $(类型名) 时查)
            item->setToolTip(0, QStringLiteral("%1\n%2\n分类：%3 ｜ 类型名：%4")
                .arg(meta.displayName,
                     meta.description.isEmpty() ? QStringLiteral("（暂无说明）") : meta.description,
                     categoryToString(cat), meta.typeName));
            // 优先使用该工具的专属素材图标, 无素材时内部回退分类图标
            item->setIcon(0, IconHelper::toolIcon(meta.typeName, meta.category, 16));
        }
    }

    m_tree->expandAll();
    applyFilter(m_search->text());
}

void Toolbox::applyFilter(const QString& text) {
    const QString needle = text.trimmed();
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto* group = m_tree->topLevelItem(i);
        int visible = 0;
        for (int j = 0; j < group->childCount(); ++j) {
            auto* item = group->child(j);
            // 显示名/类型名/中文描述 任一命中即显示 (搜"报警"能找到播放声音)
            const bool hit = needle.isEmpty()
                || item->text(0).contains(needle, Qt::CaseInsensitive)
                || item->data(0, Qt::UserRole).toString().contains(needle, Qt::CaseInsensitive)
                || item->data(0, Qt::UserRole + 1).toString().contains(needle, Qt::CaseInsensitive);
            item->setHidden(!hit);
            if (hit) ++visible;
        }
        group->setHidden(visible == 0);
    }
}

} // namespace VisionInspector
