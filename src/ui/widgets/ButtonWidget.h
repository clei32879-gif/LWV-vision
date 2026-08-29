/** @file ButtonWidget.h - 自定义控件(后续实现) */
#pragma once
#include <QWidget>
namespace VisionInspector {
class ButtonWidget : public QWidget {
    Q_OBJECT
public:
    explicit ButtonWidget(QWidget* parent = nullptr) : QWidget(parent) {}
};
} // namespace VisionInspector
