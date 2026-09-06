/** @file Toolbox.h - 工具箱(按分类列出所有可用工具, 支持搜索过滤) */
#pragma once
#include "../utils/Common.h"
#include <QWidget>
#include <QTreeWidget>
class QLineEdit;

namespace VisionInspector {

class Toolbox : public QWidget {
    Q_OBJECT
public:
    explicit Toolbox(QWidget* parent = nullptr);
    void refreshTools();

signals:
    void toolDoubleClicked(const QString& typeName);

private:
    void applyFilter(const QString& text);

    QLineEdit* m_search;
    QTreeWidget* m_tree;
};

} // namespace VisionInspector
