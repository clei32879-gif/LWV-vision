/** @file ProfileChart.h - 灰度/梯度剖面曲线控件 (对标CKVision检测边缘曲线页) */
#pragma once
#include <QWidget>
#include <QVector>
#include <QVariantList>

namespace VisionInspector {

/** 剖面曲线图: 画 [位置,值] 序列折线, 自动缩放纵轴, 带网格与刻度 */
class ProfileChart : public QWidget {
    Q_OBJECT
public:
    explicit ProfileChart(QWidget* parent = nullptr);

    void setData(const QVariantList& profile);   // [[x,y],...] 对
    void setMarker(double x);                    // 边缘位置竖线标记(可选)
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<QPointF> m_points;   // (位置, 值)
    double m_markerX = 0;
    bool m_hasMarker = false;
};
} // namespace VisionInspector
