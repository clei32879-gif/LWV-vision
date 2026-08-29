/** @file IconHelper.h - 图标生成辅助工具 */
#pragma once
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QPolygon>

namespace VisionInspector {

class IconHelper {
public:
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
