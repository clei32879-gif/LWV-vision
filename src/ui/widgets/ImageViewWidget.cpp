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
    if (!img.isNull())
        fitToWidget();
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
    painter.restore();
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
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastMouse = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void ImageViewWidget::mouseMoveEvent(QMouseEvent* event) {
    emit cursorImagePos(viewToImage(event->position()));
    if (!m_dragging) return;
    m_offset += QPointF(event->pos() - m_lastMouse);
    m_lastMouse = event->pos();
    update();
}

void ImageViewWidget::mouseReleaseEvent(QMouseEvent* event) {
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
