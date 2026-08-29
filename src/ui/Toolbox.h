/** @file Toolbox.h - 工具箱(按分类列出所有可用工具) */
#pragma once
#include "../utils/Common.h"
#include <QWidget>
#include <QTreeWidget>

namespace VisionInspector {

class Toolbox : public QWidget {
    Q_OBJECT
public:
    explicit Toolbox(QWidget* parent = nullptr);
    void refreshTools();

signals:
    void toolDoubleClicked(const QString& typeName);

private:
    QTreeWidget* m_tree;
};

} // namespace VisionInspector
