/**
 * @file ImageViewWidget.cpp
 * @brief 图像查看控件实现 (架构参考 OpenIVS ImageViewerWidget, Apache-2.0)
 */

#include "ImageViewWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPaintEvent>
#include <cmath>

namespace VisionInspector {

ImageViewWidget::ImageViewWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setStyleSheet("background-color: #141414;");
    setMinimumSize(160, 120);
}

void ImageViewWidget::setImage(const QImage& img) {
    m_image = img;
    // P1-8: 仅在图像尺寸变化时自动适配, 连续运行时保持用户的缩放/平移
    // (此前每帧 fitToWidget, 用户放大细看在运行中不可用)
    if (!img.isNull() && img.size() != m_fittedSize) {
        fitToWidget();
        m_fittedSize = img.size();
    }
    update();
}

void ImageViewWidget::setOverlays(const QVariantList& overlays) {
    m_overlays = overlays;
    update();
}

void ImageViewWidget::clearOverlays() {
    m_overlays.clear();
    update();
}

void ImageViewWidget::clearImage() {
    m_image = QImage();
    m_overlays.clear();
    update();
}

void ImageViewWidget::fitToWidget() {
    if (m_image.isNull()) return;
    const qreal sx = qreal(width()) / m_image.width();
    const qreal sy = qreal(height()) / m_image.height();
    m_scale = std::min(sx, sy) * 0.98;
    m_offset = QPointF((width() - m_image.width() * m_scale) / 2.0,
                       (height() - m_image.height() * m_scale) / 2.0);
    update();
}

void ImageViewWidget::zoomIn()  { m_scale = std::min(m_scale * 1.25, 40.0); update(); }
void ImageViewWidget::zoomOut() { m_scale = std::max(m_scale / 1.25, 0.02); update(); }
void ImageViewWidget::zoom1x1() { m_scale = 1.0; update(); }
void ImageViewWidget::zoomFit() { fitToWidget(); }

QPointF ImageViewWidget::viewToImage(const QPointF& viewPos) const {
    return (viewPos - m_offset) / m_scale;
}

QPointF ImageViewWidget::imageToView(const QPointF& imagePos) const {
    return imagePos * m_scale + m_offset;
}

void ImageViewWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), QColor(0x14, 0x14, 0x14));

    if (m_image.isNull()) {
        painter.setPen(QColor(0x66, 0x66, 0x66));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("无图像"));
        return;
    }

    painter.save();
    painter.translate(m_offset);
    painter.scale(m_scale, m_scale);
    painter.drawImage(0, 0, m_image);

    // 叠加层: 线宽用 1/scale 保持视觉恒定
    const qreal penW = 1.0 / m_scale;
    for (const QVariant& v : m_overlays) {
        const QVariantMap shape = v.toMap();
        const QColor color(shape.value("color").toString().isEmpty()
                             ? QStringLiteral("#00ff00")
                             : shape.value("color").toString());
        QPen pen(color);
        pen.setWidthF(penW * 1.5);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        drawOverlayShape(painter, shape);
    }
    // 标注模式: 画标注矩形(黄色) + 正在拖拽的矩形(青色)
    if (m_annotating) {
        painter.setPen(QPen(QColor(255, 220, 0), penW * 1.5));
        painter.setBrush(QColor(255, 220, 0, 30));
        for (const QRectF& r : m_annotations)
            painter.drawRect(r);
        if (m_drawing) {
            painter.setPen(QPen(QColor(0, 220, 255), penW * 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(m_drawingRect);
        }
    }
    // ROI编辑模式: 可拖拽/缩放的ROI框
    if (m_roiEditing)
        drawRoi(painter);
    painter.restore();
}

QImage ImageViewWidget::renderAnnotated() const {
    if (m_image.isNull()) return {};
    QImage out = m_image.convertToFormat(QImage::Format_RGB32);
    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const QVariant& v : m_overlays) {
        const QVariantMap shape = v.toMap();
        QPen pen(QColor(shape.value("color").toString().isEmpty()
                            ? QStringLiteral("#00ff00")
                            : shape.value("color").toString()));
        pen.setWidthF(1.5);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        drawOverlayShape(painter, shape);
    }
    painter.end();
    return out;
}

void ImageViewWidget::drawOverlayShape(QPainter& painter, const QVariantMap& s) const {
    const QString type = s.value("type").toString();
    if (type == "circle") {
        const double cx = s.value("cx").toDouble(), cy = s.value("cy").toDouble();
        const double r = s.value("r").toDouble();
        painter.drawEllipse(QPointF(cx, cy), r, r);
        // 十字中心线
        painter.drawLine(QPointF(cx - r * 1.2, cy), QPointF(cx + r * 1.2, cy));
        painter.drawLine(QPointF(cx, cy - r * 1.2), QPointF(cx, cy + r * 1.2));
    } else if (type == "line") {
        painter.drawLine(QPointF(s.value("x1").toDouble(), s.value("y1").toDouble()),
                         QPointF(s.value("x2").toDouble(), s.value("y2").toDouble()));
    } else if (type == "lines") {
        // 批量线段(螺纹牙型刻线等): lines=[[x1,y1,x2,y2],...]
        const QVariantList lns = s.value("lines").toList();
        for (const QVariant& lv : lns) {
            const QVariantList q = lv.toList();
            if (q.size() < 4) continue;
            painter.drawLine(QPointF(q[0].toDouble(), q[1].toDouble()),
                             QPointF(q[2].toDouble(), q[3].toDouble()));
        }
    } else if (type == "points") {
        const QVariantList pts = s.value("pts").toList();
        const double size = 3.0 / m_scale;
        for (const QVariant& pv : pts) {
            const QVariantList xy = pv.toList();
            if (xy.size() < 2) continue;
            const QPointF p(xy[0].toDouble(), xy[1].toDouble());
            painter.drawEllipse(p, size, size);
            painter.drawLine(QPointF(p.x() - size * 2, p.y()), QPointF(p.x() + size * 2, p.y()));
            painter.drawLine(QPointF(p.x(), p.y() - size * 2), QPointF(p.x(), p.y() + size * 2));
        }
    } else if (type == "cross") {
        const double cx = s.value("cx").toDouble(), cy = s.value("cy").toDouble();
        const double size = s.value("size").toDouble();
        painter.drawLine(QPointF(cx - size, cy), QPointF(cx + size, cy));
        painter.drawLine(QPointF(cx, cy - size), QPointF(cx, cy + size));
    } else if (type == "rect") {
        painter.drawRect(QRectF(s.value("x").toDouble(), s.value("y").toDouble(),
                                s.value("w").toDouble(), s.value("h").toDouble()));
    } else if (type == "text") {
        const double size = std::max(6.0, s.value("size").toDouble());
        QFont f = painter.font();
        f.setPixelSize(int(size));
        painter.setFont(f);
        painter.drawText(QPointF(s.value("x").toDouble(), s.value("y").toDouble()),
                         s.value("text").toString());
    }
}

void ImageViewWidget::setAnnotationMode(bool on) {
    m_annotating = on;
    if (on) setCursor(Qt::CrossCursor);
    else setCursor(Qt::ArrowCursor);
    update();
}

// ============================================================
// ROI 图形化编辑 (属性对话框试执行页用)
// ============================================================

void ImageViewWidget::setRoiEditing(bool on) {
    m_roiEditing = on;
    m_roiDrag = RoiHandle::None;
    setCursor(on ? Qt::SizeAllCursor : Qt::ArrowCursor);
    update();
}

void ImageViewWidget::setRoiRect(const QRectF& rect, double angleDeg) {
    if (m_roiDrag != RoiHandle::None) return; // 拖拽中不接受外部刷新
    m_roiRect = rect;
    m_roiAngle = angleDeg;
    update();
}

QPointF ImageViewWidget::roiHandlePoint(int idx) const {
    // 0..7: TL TR BR BL T R B L; u=宽度方向, v=高度方向 (随角度旋转)
    const double a = qDegreesToRadians(m_roiAngle);
    const QPointF u(std::cos(a), std::sin(a));
    const QPointF v(-std::sin(a), std::cos(a));
    const QPointF c = m_roiRect.center();
    const double hw = m_roiRect.width() / 2.0, hh = m_roiRect.height() / 2.0;
    switch (idx) {
    case 0: return c - hw * u - hh * v;
    case 1: return c + hw * u - hh * v;
    case 2: return c + hw * u + hh * v;
    case 3: return c - hw * u + hh * v;
    case 4: return c - hh * v;
    case 5: return c + hw * u;
    case 6: return c + hh * v;
    default: return c - hw * u;
    }
}

ImageViewWidget::RoiHandle ImageViewWidget::roiHitTest(const QPointF& p) const {
    // 手柄优先 (视图空间 8px 容差), 再测框体内部
    static const RoiHandle kHandles[8] = {
        RoiHandle::TL, RoiHandle::TR, RoiHandle::BR, RoiHandle::BL,
        RoiHandle::Top, RoiHandle::Right, RoiHandle::Bottom, RoiHandle::Left };
    const double tol = 8.0 / m_scale;
    for (int i = 0; i < 8; ++i) {
        const QPointF h = roiHandlePoint(i);
        if (std::hypot(p.x() - h.x(), p.y() - h.y()) <= tol)
            return kHandles[i];
    }
    // 点是否在旋转矩形内: 投影到 u/v 轴
    const double a = qDegreesToRadians(m_roiAngle);
    const QPointF u(std::cos(a), std::sin(a));
    const QPointF v(-std::sin(a), std::cos(a));
    const QPointF d = p - m_roiRect.center();
    const double du = d.x() * u.x() + d.y() * u.y();
    const double dv = d.x() * v.x() + d.y() * v.y();
    if (std::abs(du) <= m_roiRect.width() / 2.0 && std::abs(dv) <= m_roiRect.height() / 2.0)
        return RoiHandle::Body;
    return RoiHandle::None;
}

void ImageViewWidget::updateRoiCursor(RoiHandle h) {
    switch (h) {
    case RoiHandle::Body:    setCursor(Qt::SizeAllCursor); break;
    case RoiHandle::TL: case RoiHandle::BR: setCursor(Qt::SizeFDiagCursor); break;
    case RoiHandle::TR: case RoiHandle::BL: setCursor(Qt::SizeBDiagCursor); break;
    case RoiHandle::Top: case RoiHandle::Bottom: setCursor(Qt::SizeVerCursor); break;
    case RoiHandle::Left: case RoiHandle::Right: setCursor(Qt::SizeHorCursor); break;
    default: setCursor(Qt::ArrowCursor); break;
    }
}

void ImageViewWidget::drawRoi(QPainter& painter) const {
    const qreal penW = 1.0 / m_scale;
    const QPointF c = m_roiRect.center();
    painter.save();
    painter.translate(c);
    painter.rotate(m_roiAngle);
    QPen pen(QColor(0, 200, 255), penW * 1.5); // 青色ROI框, 与检测结果(绿)/标注(黄)区分
    painter.setPen(pen);
    painter.setBrush(QColor(0, 200, 255, 18));
    painter.drawRect(QRectF(-m_roiRect.width() / 2.0, -m_roiRect.height() / 2.0,
                            m_roiRect.width(), m_roiRect.height()));
    painter.restore();

    // 8个手柄 + 中心十字 (尺寸随缩放保持视觉恒定)
    const qreal hs = 5.0 / m_scale;
    painter.setPen(QPen(QColor(0, 200, 255), penW));
    painter.setBrush(QColor(0, 200, 255, 160));
    for (int i = 0; i < 8; ++i)
        painter.drawRect(QRectF(roiHandlePoint(i) - QPointF(hs, hs), QSizeF(hs * 2, hs * 2)));
    painter.drawLine(QPointF(c.x() - hs * 2, c.y()), QPointF(c.x() + hs * 2, c.y()));
    painter.drawLine(QPointF(c.x(), c.y() - hs * 2), QPointF(c.x(), c.y() + hs * 2));
}

void ImageViewWidget::setAnnotations(const QList<QRectF>& rects) {
    m_annotations = rects;
    update();
}

void ImageViewWidget::wheelEvent(QWheelEvent* event) {
    // 以鼠标位置为中心缩放
    const QPointF before = viewToImage(event->position());
    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    m_scale = std::clamp(m_scale * factor, 0.02, 40.0);
    const QPointF after = before * m_scale + m_offset;
    m_offset += QPointF(event->position()) - after;
    update();
}

void ImageViewWidget::mousePressEvent(QMouseEvent* event) {
    // ROI编辑优先: 命中手柄/框体才开始ROI拖拽, 空白处仍走平移
    if (m_roiEditing && event->button() == Qt::LeftButton) {
        const QPointF imgPos = viewToImage(event->position());
        const RoiHandle h = roiHitTest(imgPos);
        if (h != RoiHandle::None) {
            m_roiDrag = h;
            m_roiPressImg = imgPos;
            m_roiPressRect = m_roiRect;
            return;
        }
    }
    if (m_annotating) {
        const QPointF imgPos = viewToImage(event->position());
        if (event->button() == Qt::LeftButton) {
            m_drawing = true;
            m_drawingRect = QRectF(imgPos, imgPos);
        } else if (event->button() == Qt::RightButton) {
            // 删除最近的标注框
            int best = -1; double bestD = 1e18;
            for (int i = 0; i < m_annotations.size(); ++i) {
                const QPointF c = m_annotations[i].center();
                const double d = std::hypot(c.x() - imgPos.x(), c.y() - imgPos.y());
                if (d < bestD) { bestD = d; best = i; }
            }
            if (best >= 0 && bestD < 60) {
                m_annotations.removeAt(best);
                update();
                emit annotationDeleted(best);
            }
        }
        return;
    }
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastMouse = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void ImageViewWidget::mouseMoveEvent(QMouseEvent* event) {
    emit cursorImagePos(viewToImage(event->position()));
    // ROI拖拽: 移动框体 / 沿旋转轴投影缩放 (中心固定, 对称缩放)
    if (m_roiEditing && m_roiDrag != RoiHandle::None) {
        const QPointF imgPos = viewToImage(event->position());
        if (m_roiDrag == RoiHandle::Body) {
            m_roiRect = m_roiPressRect.translated(imgPos - m_roiPressImg);
        } else {
            const double a = qDegreesToRadians(m_roiAngle);
            const QPointF u(std::cos(a), std::sin(a));
            const QPointF v(-std::sin(a), std::cos(a));
            const QPointF d = imgPos - m_roiRect.center();
            const double du = d.x() * u.x() + d.y() * u.y();
            const double dv = d.x() * v.x() + d.y() * v.y();
            const QPointF c = m_roiRect.center();
            double w = m_roiRect.width(), h = m_roiRect.height();
            switch (m_roiDrag) {
            case RoiHandle::BR: w = 2 * du; h = 2 * dv; break;
            case RoiHandle::TR: w = 2 * du; h = -2 * dv; break;
            case RoiHandle::BL: w = -2 * du; h = 2 * dv; break;
            case RoiHandle::TL: w = -2 * du; h = -2 * dv; break;
            case RoiHandle::Right:  w = 2 * du; break;
            case RoiHandle::Left:   w = -2 * du; break;
            case RoiHandle::Bottom: h = 2 * dv; break;
            case RoiHandle::Top:    h = -2 * dv; break;
            default: break;
            }
            m_roiRect = QRectF(QPointF(c.x() - std::max(4.0, w) / 2.0,
                                       c.y() - std::max(4.0, h) / 2.0),
                               QSizeF(std::max(4.0, w), std::max(4.0, h)));
        }
        emit roiEdited(m_roiRect);
        update();
        return;
    }
    // ROI编辑悬停: 按手柄位置给对应光标
    if (m_roiEditing && !m_dragging) {
        updateRoiCursor(roiHitTest(viewToImage(event->position())));
    }
    if (m_annotating && m_drawing) {
        m_drawingRect.setBottomRight(viewToImage(event->position()));
        update();
        return;
    }
    if (!m_dragging) return;
    m_offset += QPointF(event->pos() - m_lastMouse);
    m_lastMouse = event->pos();
    update();
}

void ImageViewWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (m_roiEditing && m_roiDrag != RoiHandle::None && event->button() == Qt::LeftButton) {
        m_roiDrag = RoiHandle::None;
        emit roiEdited(m_roiRect); // 松手再发一次, 保证最终值必达
        setCursor(Qt::SizeAllCursor);
        return;
    }
    if (m_annotating && m_drawing && event->button() == Qt::LeftButton) {
        m_drawing = false;
        QRectF r = m_drawingRect.normalized();
        if (r.width() > 4 && r.height() > 4) {
            m_annotations.append(r);
            emit annotationCreated(r);
        }
        update();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
    }
}

void ImageViewWidget::mouseDoubleClickEvent(QMouseEvent*) {
    fitToWidget();
}

void ImageViewWidget::resizeEvent(QResizeEvent*) {
    if (!m_image.isNull())
        fitToWidget();
}

} // namespace VisionInspector
