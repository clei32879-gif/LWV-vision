/** @file ProfileChart.cpp — 剖面曲线控件实现 */
#include "ProfileChart.h"
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace VisionInspector {

ProfileChart::ProfileChart(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(80);
}

void ProfileChart::setData(const QVariantList& profile) {
    m_points.clear();
    for (const QVariant& v : profile) {
        const QVariantList xy = v.toList();
        if (xy.size() >= 2)
            m_points.append(QPointF(xy[0].toDouble(), xy[1].toDouble()));
    }
    update();
}

void ProfileChart::setMarker(double x) {
    m_markerX = x; m_hasMarker = true; update();
}

void ProfileChart::clear() {
    m_points.clear(); m_hasMarker = false; update();
}

void ProfileChart::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(20, 24, 38));   // 深底(对标创科曲线页)

    if (m_points.size() < 2) {
        p.setPen(QColor(120, 130, 150));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("试执行后显示剖面曲线"));
        return;
    }

    double x0 = m_points.first().x(), x1 = x0;
    double y0 = m_points.first().y(), y1 = y0;
    for (const QPointF& pt : m_points) {
        x0 = std::min(x0, pt.x()); x1 = std::max(x1, pt.x());
        y0 = std::min(y0, pt.y()); y1 = std::max(y1, pt.y());
    }
    const double xSpan = std::max(1.0, x1 - x0);
    double yLo = std::min(y0, 0.0), yHi = std::max(y1, 255.0);
    if (std::fabs(yHi - yLo) < 1e-6) { yLo -= 1; yHi += 1; }

    const QRectF plot = rect().adjusted(44, 10, -10, -24);

    // 网格
    p.setPen(QPen(QColor(60, 72, 100), 1));
    for (int i = 0; i <= 4; ++i) {
        const double fy = plot.top() + plot.height() * i / 4.0;
        p.drawLine(QPointF(plot.left(), fy), QPointF(plot.right(), fy));
    }
    for (int i = 0; i <= 6; ++i) {
        const double fx = plot.left() + plot.width() * i / 6.0;
        p.drawLine(QPointF(fx, plot.top()), QPointF(fx, plot.bottom()));
    }

    auto mapPt = [&](const QPointF& pt) -> QPointF {
        const double fx = plot.left() + (pt.x() - x0) / xSpan * plot.width();
        const double fy = plot.bottom() - (pt.y() - yLo) / (yHi - yLo) * plot.height();
        return QPointF(fx, fy);
    };

    // 曲线
    p.setPen(QPen(QColor(160, 100, 255), 1.6));
    for (int i = 1; i < m_points.size(); ++i)
        p.drawLine(mapPt(m_points[i-1]), mapPt(m_points[i]));

    // 边缘位置标记 (竖线)
    if (m_hasMarker) {
        p.setPen(QPen(QColor(80, 200, 255), 1, Qt::DashLine));
        const double mx = plot.left() + (m_markerX - x0) / xSpan * plot.width();
        p.drawLine(QPointF(mx, plot.top()), QPointF(mx, plot.bottom()));
    }

    // 轴刻度
    p.setPen(QColor(150, 160, 180));
    p.setFont(QFont(QStringLiteral("Segoe UI"), 7));
    p.drawText(QPointF(plot.left(), plot.bottom() + 14), QString::number(x0, 'f', 0));
    p.drawText(QPointF(plot.right() - 30, plot.bottom() + 14), QString::number(x1, 'f', 0));
    p.drawText(QPointF(4, plot.top() + 10), QString::number(yHi, 'f', 0));
    p.drawText(QPointF(4, plot.bottom()), QString::number(yLo, 'f', 0));
}

} // namespace VisionInspector
