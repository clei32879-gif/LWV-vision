/** @file ROI.h - ROI区域定义（5种类型） */
#pragma once
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QVector>
#include <QVariant>
#include <QMap>
#include <QString>
#include <QColor>

namespace VisionInspector {

/**
 * ROI类型
 */
enum class ROIType {
    None,       // 无ROI（全图像）
    Rectangle,  // 矩形
    Diamond,    // 菱形
    Circle,     // 圆形
    Ring,       // 环形
    Freeform    // 自由形状（多边形）
};

/**
 * ROI区域定义
 */
struct ROIRegion {
    ROIType type = ROIType::None;
    
    // 矩形/菱形参数
    double centerX = 0;
    double centerY = 0;
    double width = 100;
    double height = 100;
    double angle = 0;  // 旋转角度（度）
    
    // 圆形/环形参数
    double radius = 50;
    double thickness = 20;  // 环形厚度
    double startAngle = 0;  // 起始角度
    double sweepAngle = 360; // 扫描角度范围
    
    // 自由形状参数
    QPolygonF polygon;
    
    // OK/NG颜色
    QColor okColor = QColor(0, 200, 0);
    QColor ngColor = QColor(200, 0, 0);
    
    /**
     * 检查点是否在ROI内
     */
    bool contains(double x, double y) const;
    
    /**
     * 获取ROI的边界矩形
     */
    QRectF boundingRect() const;
    
    /**
     * 从工具属性创建ROI
     */
    static ROIRegion fromProperties(const QMap<QString, QVariant>& props);
    
    /**
     * 将ROI转换为属性
     */
    QMap<QString, QVariant> toProperties() const;
};

} // namespace VisionInspector
