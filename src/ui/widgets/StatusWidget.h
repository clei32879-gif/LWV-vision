/** @file StatusWidget.h - 自定义控件(后续实现) */
#pragma once
#include <QWidget>
namespace VisionInspector {
class StatusWidget : public QWidget {
    Q_OBJECT
public:
    explicit StatusWidget(QWidget* parent = nullptr) : QWidget(parent) {}
};
} // namespace VisionInspector
