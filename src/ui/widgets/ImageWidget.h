/** @file ImageWidget.h - 自定义控件(后续实现) */
#pragma once
#include <QWidget>
namespace VisionInspector {
class ImageWidget : public QWidget {
    Q_OBJECT
public:
    explicit ImageWidget(QWidget* parent = nullptr) : QWidget(parent) {}
};
} // namespace VisionInspector
