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

    /** 标注模式: 鼠标拖拽画矩形(图像坐标), 右键删除最近框 */
    void setAnnotationMode(bool on);
    void setAnnotations(const QList<QRectF>& rects);
    const QList<QRectF>& annotations() const { return m_annotations; }

    /** ROI 编辑模式: 在图上拖拽移动/缩放ROI框 (中心+宽高+角度, 图像坐标) */
    void setRoiEditing(bool on);
    bool isRoiEditing() const { return m_roiEditing; }
    /** 设置ROI框 (拖拽进行中调用会被忽略, 防止外部刷新打架) */
    void setRoiRect(const QRectF& rect, double angleDeg = 0.0);
    QRectF roiRect() const { return m_roiRect; }

    /** 视图坐标 → 图像坐标 */
    QPointF viewToImage(const QPointF& viewPos) const;
    /** 图像坐标 → 视图坐标 */
    QPointF imageToView(const QPointF& imagePos) const;

signals:
    /** 鼠标在图像上的坐标变化(状态栏显示用) */
    void cursorImagePos(const QPointF& pos);
    /** 标注模式下新建了一个矩形 */
    void annotationCreated(const QRectF& rect);
    /** 标注模式下右键删除了一个矩形 */
    void annotationDeleted(int index);
    /** ROI编辑: 框被拖拽/缩放 (图像坐标, 拖拽中连续发射) */
    void roiEdited(const QRectF& rect);

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
    // ---- ROI 编辑 ----
    enum class RoiHandle { None, Body, TL, TR, BR, BL, Top, Bottom, Left, Right };
    RoiHandle roiHitTest(const QPointF& imagePos) const;
    void drawRoi(QPainter& painter) const;
    void updateRoiCursor(RoiHandle h);
    /** 旋转ROI的四角/边中点 (图像坐标): u=宽度方向单位向量, v=高度方向 */
    QPointF roiHandlePoint(int idx) const; // 0..7: TL TR BR BL T R B L

    QImage m_image;
    QVariantList m_overlays;
    QSize m_fittedSize;               // 最近一次自动适配的图像尺寸 (P1-8: 连续运行不重置缩放)
    bool m_annotating = false;           // 标注模式
    QList<QRectF> m_annotations;         // 标注矩形(图像坐标)
    QRectF m_drawingRect;                // 正在拖拽的矩形
    bool m_drawing = false;
    qreal m_scale = 1.0;
    QPointF m_offset{0, 0};          // 图像原点在控件中的位置
    bool m_dragging = false;
    QPoint m_lastMouse;
    // ---- ROI 编辑 ----
    bool m_roiEditing = false;
    QRectF m_roiRect;                    // 图像坐标
    double m_roiAngle = 0.0;             // 度
    RoiHandle m_roiDrag = RoiHandle::None;
    QPointF m_roiPressImg;               // 按下时图像坐标
    QRectF m_roiPressRect;               // 按下时ROI
};

} // namespace VisionInspector
