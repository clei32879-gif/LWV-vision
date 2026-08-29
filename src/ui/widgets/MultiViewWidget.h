/** @file MultiViewWidget.h - 多视图布局（1~16分屏, 每视图带缩放/平移/结果叠加） */
#pragma once
#include "../../utils/Common.h"
#include <QWidget>
#include <QLabel>
#include <QGridLayout>
#include <QVector>
#include <QVariantList>

namespace VisionInspector {

class ImageViewWidget;

/**
 * 多视图控件 - 支持1x1 ~ 4x4 布局
 * 用于同时显示多个相机的图像与检测结果叠加
 */
class MultiViewWidget : public QWidget {
    Q_OBJECT
public:
    explicit MultiViewWidget(QWidget* parent = nullptr);

    /** 设置布局 (rows x cols) */
    void setLayout(int rows, int cols);

    /** 设置某个视图的图像 */
    void setImage(int index, const QImage& image);

    /** 设置某个视图的结果叠加层 (工具overlays输出的图形描述列表) */
    void setOverlays(int index, const QVariantList& overlays);

    /** 获取某个视图的查看控件 */
    ImageViewWidget* viewAt(int index) const {
        return (index >= 0 && index < m_imageViews.size()) ? m_imageViews[index] : nullptr;
    }

    /** 设置某个视图的标题 */
    void setTitle(int index, const QString& title);

    /** 设置某个视图的状态 */
    void setStatus(int index, const QString& status);

    /** 清除所有图像 */
    void clearAll();

    /** 获取当前布局 */
    int rows() const { return m_rows; }
    int cols() const { return m_cols; }

signals:
    void viewClicked(int index);

private:
    QGridLayout* m_gridLayout;
    QVector<ImageViewWidget*> m_imageViews;
    QVector<QLabel*> m_titleLabels;
    QVector<QLabel*> m_statusLabels;
    int m_rows = 1;
    int m_cols = 1;
    void rebuildLayout();
};

} // namespace VisionInspector
