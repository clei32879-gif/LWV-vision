/** @file ValueWidget.h - 自定义控件(后续实现) */
#pragma once
#include <QWidget>
namespace VisionInspector {
class ValueWidget : public QWidget {
    Q_OBJECT
public:
    explicit ValueWidget(QWidget* parent = nullptr) : QWidget(parent) {}
};
} // namespace VisionInspector
