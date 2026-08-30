/** @file IconHelper.h - 图标生成辅助工具 */
#pragma once
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QPolygon>
#include <QLinearGradient>
#include <QBrush>
#include <cmath>
#include "../utils/Common.h"

namespace VisionInspector {

class IconHelper {
public:
    // 应用图标: 蓝色圆盘 + 眼睛(虹膜/瞳孔/高光) + 取景框四角
    static QIcon appIcon(int size = 64) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal s = size / 256.0;

        // 蓝色渐变圆盘
        QLinearGradient lg(0, 0, 0, size);
        lg.setColorAt(0.0, QColor(33, 150, 243));
        lg.setColorAt(1.0, QColor(13, 71, 161));
        p.setPen(Qt::NoPen);
        p.setBrush(lg);
        p.drawEllipse(0, 0, size, size);

        // 取景框四角
        if (size >= 48) {
            QPen pen(QColor(255, 255, 255, 210), 3.5 * s);
            pen.setCapStyle(Qt::RoundCap);
            p.setPen(pen);
            const qreal m = 30 * s, L = 30 * s, mx = size - 1 - m;
            for (qreal x : {m, mx}) {
                for (qreal y : {m, mx}) {
                    const qreal dx = (x < size / 2.0) ? L : -L;
                    const qreal dy = (y < size / 2.0) ? L : -L;
                    p.drawLine(QPointF(x, y + dy), QPointF(x, y));
                    p.drawLine(QPointF(x, y), QPointF(x + dx, y));
                }
            }
        }

        // 眼睛: 白色眼白 + 虹膜 + 瞳孔 + 高光
        const qreal cx = size / 2.0, cy = size / 2.0;
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawEllipse(QPointF(cx, cy), 58 * s, 46 * s);
        p.setBrush(QColor(100, 149, 237));           // 虹膜
        p.drawEllipse(QPointF(cx, cy), 34 * s, 34 * s);
        p.setBrush(QColor(13, 43, 75));              // 瞳孔
        p.drawEllipse(QPointF(cx, cy), 17 * s, 17 * s);
        p.setBrush(Qt::white);                       // 高光
        p.drawEllipse(QPointF(cx - 14 * s, cy - 14 * s), 8 * s, 8 * s);

        return QIcon(pm);
    }

    // 工具分类图标: 每类一个配色 + 图形
    static QIcon categoryIcon(ToolCategory cat, int size = 24) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal s = size / 24.0;
        QColor c;
        switch (cat) {
            case ToolCategory::Camera:        c = QColor(74, 158, 255);  break;
            case ToolCategory::ImageProcess:  c = QColor(255, 159, 64);  break;
            case ToolCategory::Calibration:   c = QColor(0, 200, 200);   break;
            case ToolCategory::Detection:     c = QColor(0, 200, 80);    break;
            case ToolCategory::Geometry:      c = QColor(176, 124, 255); break;
            case ToolCategory::Communication: c = QColor(255, 210, 63);  break;
            case ToolCategory::Logic:         c = QColor(255, 107, 138); break;
            case ToolCategory::System:        c = QColor(154, 160, 166); break;
            case ToolCategory::ThreeD:        c = QColor(64, 224, 208);  break;
            case ToolCategory::Special:       c = QColor(255, 92, 92);   break;
        }
        QPen pen(c, 1.6 * s);
        p.setPen(pen);
        p.setBrush(QColor(c.red(), c.green(), c.blue(), 60));

        switch (cat) {
            case ToolCategory::Camera:
                p.drawRoundedRect(QRectF(3 * s, 7 * s, 18 * s, 12 * s), 2 * s, 2 * s);
                p.setBrush(c);
                p.drawEllipse(QPointF(12 * s, 13 * s), 3.5 * s, 3.5 * s);
                break;
            case ToolCategory::ImageProcess: {
                // 直方图/滑杆: 三条竖线
                p.drawLine(QPointF(6 * s, 18 * s), QPointF(6 * s, 14 * s));
                p.drawLine(QPointF(12 * s, 18 * s), QPointF(12 * s, 8 * s));
                p.drawLine(QPointF(18 * s, 18 * s), QPointF(18 * s, 11 * s));
                p.setBrush(c);
                p.drawEllipse(QPointF(12 * s, 6 * s), 2.2 * s, 2.2 * s);
                break;
            }
            case ToolCategory::Calibration:
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(QPointF(12 * s, 12 * s), 7 * s, 7 * s);
                p.drawLine(QPointF(12 * s, 2 * s), QPointF(12 * s, 7 * s));
                p.drawLine(QPointF(12 * s, 17 * s), QPointF(12 * s, 22 * s));
                p.drawLine(QPointF(2 * s, 12 * s), QPointF(7 * s, 12 * s));
                p.drawLine(QPointF(17 * s, 12 * s), QPointF(22 * s, 12 * s));
                break;
            case ToolCategory::Detection:
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(QPointF(9 * s, 9 * s), 5 * s, 5 * s);
                p.drawLine(QPointF(13 * s, 13 * s), QPointF(20 * s, 20 * s));
                p.drawLine(QPointF(6 * s, 9 * s), QPointF(8 * s, 11 * s));
                p.drawLine(QPointF(8 * s, 11 * s), QPointF(6 * s, 13 * s));
                break;
            case ToolCategory::Geometry:
                p.setBrush(Qt::NoBrush);
                p.drawPolygon(QPolygonF() << QPointF(12 * s, 4 * s)
                    << QPointF(20 * s, 20 * s) << QPointF(4 * s, 20 * s));
                p.drawLine(QPointF(7 * s, 20 * s), QPointF(9 * s, 13 * s));
                p.drawLine(QPointF(17 * s, 20 * s), QPointF(15 * s, 13 * s));
                break;
            case ToolCategory::Communication:
                p.setBrush(Qt::NoBrush);
                p.drawLine(QPointF(3 * s, 12 * s), QPointF(13 * s, 12 * s));
                p.drawLine(QPointF(9 * s, 8 * s), QPointF(13 * s, 12 * s));
                p.drawLine(QPointF(9 * s, 16 * s), QPointF(13 * s, 12 * s));
                p.drawLine(QPointF(21 * s, 12 * s), QPointF(11 * s, 12 * s));
                p.drawLine(QPointF(15 * s, 8 * s), QPointF(11 * s, 12 * s));
                p.drawLine(QPointF(15 * s, 16 * s), QPointF(11 * s, 12 * s));
                break;
            case ToolCategory::Logic:
                p.setBrush(Qt::NoBrush);
                p.drawPolygon(QPolygonF() << QPointF(12 * s, 3 * s)
                    << QPointF(21 * s, 12 * s) << QPointF(12 * s, 21 * s)
                    << QPointF(3 * s, 12 * s));
                break;
            case ToolCategory::System: {
                // 齿轮: 外圈 + 内孔
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(QPointF(12 * s, 12 * s), 5 * s, 5 * s);
                for (int i = 0; i < 6; ++i) {
                    const qreal a = i * 3.14159 / 3.0;
                    p.drawLine(QPointF(12 * s + 5 * s * std::cos(a), 12 * s + 5 * s * std::sin(a)),
                               QPointF(12 * s + 7.5 * s * std::cos(a), 12 * s + 7.5 * s * std::sin(a)));
                }
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(QPointF(12 * s, 12 * s), 2.5 * s, 2.5 * s);
                break;
            }
            case ToolCategory::ThreeD: {
                // 立方体线框
                p.setBrush(Qt::NoBrush);
                p.drawPolygon(QPolygonF() << QPointF(12 * s, 4 * s)
                    << QPointF(20 * s, 8 * s) << QPointF(20 * s, 18 * s)
                    << QPointF(12 * s, 22 * s) << QPointF(4 * s, 18 * s)
                    << QPointF(4 * s, 8 * s));
                p.drawLine(QPointF(12 * s, 4 * s), QPointF(12 * s, 22 * s));
                p.drawLine(QPointF(4 * s, 8 * s), QPointF(20 * s, 8 * s));
                p.drawLine(QPointF(4 * s, 18 * s), QPointF(20 * s, 18 * s));
                break;
            }
            case ToolCategory::Special: {
                // 星形
                p.setBrush(Qt::NoBrush);
                QPolygonF star;
                for (int i = 0; i < 10; ++i) {
                    const qreal r = (i % 2 == 0) ? 8 * s : 3.6 * s;
                    const qreal a = -1.570796 + i * 3.14159 / 5.0;
                    star << QPointF(12 * s + r * std::cos(a), 12 * s + r * std::sin(a));
                }
                p.drawPolygon(star);
                break;
            }
        }
        return QIcon(pm);
    }

    // 相机图标（蓝色相机形状）
    static QIcon cameraIcon(int size = 24) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);

        // 相机机身
        p.setPen(QPen(QColor(74, 158, 255), 2));
        p.setBrush(QColor(74, 158, 255, 60));
        p.drawRoundedRect(3, 7, size - 6, size - 12, 3, 3);

        // 镜头
        p.setBrush(QColor(74, 158, 255));
        p.drawEllipse(size/2 - 5, size/2 - 4, 10, 10);
        p.setBrush(Qt::white);
        p.drawEllipse(size/2 - 3, size/2 - 2, 6, 6);

        // 闪光灯
        p.setBrush(QColor(255, 255, 255));
        p.drawRect(size/2 + 4, 5, 4, 3);

        return QIcon(pixmap);
    }

    // 用户图标（人形轮廓）
    static QIcon userIcon(int size = 24) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);

        // 头部
        p.setPen(QPen(QColor(74, 158, 255), 2));
        p.setBrush(QColor(74, 158, 255, 60));
        p.drawEllipse(size/2 - 5, 2, 10, 10);

        // 身体
        p.setBrush(QColor(74, 158, 255));
        p.drawEllipse(size/2 - 8, size/2 + 1, 16, 12);

        return QIcon(pixmap);
    }

    // 扫描图标（放大镜+刷新）
    static QIcon scanIcon(int size = 24) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);

        // 放大镜
        p.setPen(QPen(QColor(74, 158, 255), 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(4, 4, 12, 12);
        p.drawLine(13, 13, 18, 18);

        // 循环箭头
        p.setPen(QPen(QColor(0, 200, 0), 2));
        p.drawArc(8, 8, 8, 8, 30 * 16, 270 * 16);

        return QIcon(pixmap);
    }

    // 执行图标（绿色播放按钮）
    static QIcon executeIcon(int size = 24) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);

        // 圆形背景
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 180, 0));
        p.drawEllipse(2, 2, size - 4, size - 4);

        // 播放三角形
        p.setBrush(Qt::white);
        QPolygon triangle;
        triangle << QPoint(size/2 - 3, size/2 - 5)
                 << QPoint(size/2 - 3, size/2 + 5)
                 << QPoint(size/2 + 5, size/2);
        p.drawPolygon(triangle);

        return QIcon(pixmap);
    }

    // 运行图标（绿色双箭头）
    static QIcon runIcon(int size = 24) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 180, 0));
        p.drawEllipse(2, 2, size - 4, size - 4);

        // 双箭头
        p.setBrush(Qt::white);
        QPolygon arrow1;
        arrow1 << QPoint(7, 6) << QPoint(7, 18) << QPoint(13, 12);
        p.drawPolygon(arrow1);
        QPolygon arrow2;
        arrow2 << QPoint(11, 6) << QPoint(11, 18) << QPoint(17, 12);
        p.drawPolygon(arrow2);

        return QIcon(pixmap);
    }

    // 停止图标（红色方块）
    static QIcon stopIcon(int size = 24) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(220, 50, 50));
        p.drawEllipse(2, 2, size - 4, size - 4);

        // 方块
        p.setBrush(Qt::white);
        p.drawRect(size/2 - 4, size/2 - 4, 8, 8);

        return QIcon(pixmap);
    }

    // 新建图标（白色文件+蓝色加号）
    static QIcon newIcon(int size = 24) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);

        // 文件
        p.setPen(QPen(QColor(74, 158, 255), 2));
        p.setBrush(QColor(74, 158, 255, 40));
        p.drawRect(5, 2, 12, 18);
        p.drawLine(5, 6, 17, 6);

        // 加号
        p.setPen(QPen(QColor(0, 180, 0), 2));
        p.drawLine(10, 10, 10, 18);
        p.drawLine(6, 14, 14, 14);

        return QIcon(pixmap);
    }

    // 打开图标（文件夹）
    static QIcon openIcon(int size = 24) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);

        // 文件夹
        p.setPen(QPen(QColor(255, 180, 0), 2));
        p.setBrush(QColor(255, 180, 0, 60));
        p.drawRect(2, 6, 20, 14);
        p.drawRect(2, 4, 8, 4);

        // 打开效果
        p.setBrush(QColor(255, 220, 100));
        p.drawRect(4, 8, 16, 3);

        return QIcon(pixmap);
    }

    // 保存图标（软盘）
    static QIcon saveIcon(int size = 24) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);

        // 软盘主体
        p.setPen(QPen(QColor(74, 158, 255), 2));
        p.setBrush(QColor(74, 158, 255, 60));
        p.drawRect(3, 3, 18, 18);

        // 金属片
        p.setBrush(QColor(200, 200, 200));
        p.drawRect(7, 3, 10, 6);

        // 标签
        p.setBrush(QColor(255, 255, 255));
        p.drawRect(5, 12, 14, 8);

        return QIcon(pixmap);
    }
};

} // namespace VisionInspector
