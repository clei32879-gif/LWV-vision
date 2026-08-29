/**
 * @file DisplayArea.h
 * @brief 显示区域 (中心区域) - 支持缩放控制
 */
#pragma once
#include "../utils/Common.h"
#include <QWidget>
#include <QScrollArea>
#include <QLabel>
#include <QWheelEvent>

namespace VisionInspector {

class DisplayArea : public QWidget {
    Q_OBJECT
public:
    explicit DisplayArea(QWidget* parent = nullptr);

    /** 显示图像 */
    void displayImage(const CvImage& image);

    /** 清除显示 */
    void clear();

    // 缩放控制（参考CKvsRUNCtrl）
    void zoomIn();      // 放大
    void zoomOut();     // 缩小
    void zoom1x1();     // 1:1显示
    void zoomFit();     // 自适应窗口

    /** 获取当前缩放比例 */
    double zoomLevel() const { return m_zoomLevel; }

signals:
    void zoomChanged(double level);

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    QLabel* m_imageLabel;
    QImage m_currentImage;
    double m_zoomLevel = 1.0;
    bool m_fitMode = true;

    void updateDisplay();
};

} // namespace VisionInspector
