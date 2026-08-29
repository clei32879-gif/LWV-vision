/**
 * @file ImageViewWidget.h
 * @brief 图像查看控件 — 缩放/平移/检测结果叠加绘制
 *
 * 架构参考 OpenIVS 的 ImageViewerWidget (Apache-2.0, github.com/dl-cv/OpenIVS)
 * 叠加层使用与工具无关的通用图形描述(QVariantMap):
 *   {"type":"circle", "cx":.., "cy":.., "r":.., "color":"#rrggbb"}
 *   {"type":"line",   "x1":.., "y1":.., "x2":.., "y2":.., "color":..}
 *   {"type":"lines",  "lines":[[x1,y1,x2,y2],...], "color":..}
 *   {"type":"points", "pts":[[x,y],...], "color":..}
 *   {"type":"cross",  "cx":.., "cy":.., "size":.., "color":..}
 *   {"type":"rect",   "x":.., "y":.., "w":.., "h":.., "color":..}
 * 坐标全部为图像坐标系; 线宽随缩放保持视觉恒定。
 */
#pragma once
#include <QWidget>
#include <QImage>
#include <QPointF>
#include <QVariantList>
#include <QVariantMap>

class QMouseEvent;
class QWheelEvent;
class QPaintEvent;

namespace VisionInspector {

class ImageViewWidget : public QWidget {
    Q_OBJECT
public:
    explicit ImageViewWidget(QWidget* parent = nullptr);

    void setImage(const QImage& img);
    void setOverlays(const QVariantList& overlays);
    void clearOverlays();
    void clearImage();

    void zoomIn();
    void zoomOut();
    void zoomFit();
    void zoom1x1();

    /** 图像+叠加层合成为一张QImage (保存带标注图用) */
    QImage renderAnnotated() const;

    /** 视图坐标 → 图像坐标 */
    QPointF viewToImage(const QPointF& viewPos) const;
    /** 图像坐标 → 视图坐标 */
    QPointF imageToView(const QPointF& imagePos) const;

signals:
    /** 鼠标在图像上的坐标变化(状态栏显示用) */
    void cursorImagePos(const QPointF& pos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void fitToWidget();
    void drawOverlayShape(QPainter& painter, const QVariantMap& shape) const;

    QImage m_image;
    QVariantList m_overlays;
    qreal m_scale = 1.0;
    QPointF m_offset{0, 0};          // 图像原点在控件中的位置
    bool m_dragging = false;
    QPoint m_lastMouse;
};

} // namespace VisionInspector
