#include "ROI.h"
#include <cmath>

namespace VisionInspector {

bool ROIRegion::contains(double x, double y) const {
    switch (type) {
    case ROIType::None:
        return true;  // 全图像
        
    case ROIType::Rectangle: {
        // 旋转矩形检测
        double rad = -angle * M_PI / 180.0;
        double cos_a = cos(rad);
        double sin_a = sin(rad);
        double dx = x - centerX;
        double dy = y - centerY;
        double rx = dx * cos_a - dy * sin_a;
        double ry = dx * sin_a + dy * cos_a;
        return std::abs(rx) <= width / 2 && std::abs(ry) <= height / 2;
    }
    
    case ROIType::Diamond: {
        // 菱形检测（旋转45度的矩形）
        double rad = -angle * M_PI / 180.0;
        double cos_a = cos(rad);
        double sin_a = sin(rad);
        double dx = x - centerX;
        double dy = y - centerY;
        double rx = dx * cos_a - dy * sin_a;
        double ry = dx * sin_a + dy * cos_a;
        return (std::abs(rx) / (width / 2) + std::abs(ry) / (height / 2)) <= 1.0;
    }
    
    case ROIType::Circle: {
        double dx = x - centerX;
        double dy = y - centerY;
        double dist = std::sqrt(dx * dx + dy * dy);
        return dist <= radius;
    }
    
    case ROIType::Ring: {
        double dx = x - centerX;
        double dy = y - centerY;
        double dist = std::sqrt(dx * dx + dy * dy);
        double innerRadius = radius - thickness / 2;
        double outerRadius = radius + thickness / 2;
        if (dist < innerRadius || dist > outerRadius) return false;
        
        // 角度范围检查
        double angleDeg = std::atan2(dy, dx) * 180.0 / M_PI;
        if (angleDeg < 0) angleDeg += 360;
        if (sweepAngle >= 360) return true;
        double start = startAngle;
        double end = startAngle + sweepAngle;
        if (end > 360) {
            return angleDeg >= start || angleDeg <= (end - 360);
        }
        return angleDeg >= start && angleDeg <= end;
    }
    
    case ROIType::Freeform:
        return polygon.containsPoint(QPointF(x, y), Qt::OddEvenFill);
    
    default:
        return false;
    }
}

QRectF ROIRegion::boundingRect() const {
    switch (type) {
    case ROIType::None:
        return QRectF(0, 0, 10000, 10000);
    case ROIType::Rectangle:
    case ROIType::Diamond:
        return QRectF(centerX - width / 2, centerY - height / 2, width, height);
    case ROIType::Circle:
    case ROIType::Ring:
        return QRectF(centerX - radius, centerY - radius, radius * 2, radius * 2);
    case ROIType::Freeform:
        return polygon.boundingRect();
    default:
        return QRectF();
    }
}

ROIRegion ROIRegion::fromProperties(const QMap<QString, QVariant>& props) {
    ROIRegion roi;
    roi.type = static_cast<ROIType>(props.value("roiType", 0).toInt());
    roi.centerX = props.value("roiCenterX", 70.0).toDouble();
    roi.centerY = props.value("roiCenterY", 70.0).toDouble();
    roi.width = props.value("roiWidth", 100.0).toDouble();
    roi.height = props.value("roiHeight", 100.0).toDouble();
    roi.angle = props.value("roiAngle", 0.0).toDouble();
    roi.radius = props.value("roiRadius", 50.0).toDouble();
    roi.thickness = props.value("roiThickness", 20.0).toDouble();
    roi.startAngle = props.value("roiStartAngle", 0.0).toDouble();
    roi.sweepAngle = props.value("roiSweepAngle", 360.0).toDouble();
    return roi;
}

QMap<QString, QVariant> ROIRegion::toProperties() const {
    QMap<QString, QVariant> props;
    props["roiType"] = static_cast<int>(type);
    props["roiCenterX"] = centerX;
    props["roiCenterY"] = centerY;
    props["roiWidth"] = width;
    props["roiHeight"] = height;
    props["roiAngle"] = angle;
    props["roiRadius"] = radius;
    props["roiThickness"] = thickness;
    props["roiStartAngle"] = startAngle;
    props["roiSweepAngle"] = sweepAngle;
    return props;
}

} // namespace VisionInspector
