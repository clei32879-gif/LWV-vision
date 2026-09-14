#include "MultiViewWidget.h"
#include "ImageViewWidget.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPainter>

namespace VisionInspector {

MultiViewWidget::MultiViewWidget(QWidget* parent) : QWidget(parent) {
    setStyleSheet("background-color: #1a1a1a;");
    m_gridLayout = new QGridLayout(this);
    m_gridLayout->setContentsMargins(2, 2, 2, 2);
    m_gridLayout->setSpacing(2);
    rebuildLayout();
}

void MultiViewWidget::setLayout(int rows, int cols) {
    if (rows < 1) rows = 1; if (rows > 4) rows = 4;
    if (cols < 1) cols = 1; if (cols > 4) cols = 4;
    if (rows == m_rows && cols == m_cols) return;
    m_rows = rows; m_cols = cols;
    rebuildLayout();
}

void MultiViewWidget::rebuildLayout() {
    // 清除旧的
    QLayoutItem* item;
    while ((item = m_gridLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    m_imageViews.clear();
    m_titleLabels.clear();
    m_statusLabels.clear();

    int total = m_rows * m_cols;
    for (int i = 0; i < total; ++i) {
        auto* container = new QWidget(this);
        container->setStyleSheet("background-color: #dcebf8; border: 1px solid #b8d2ea;");
        auto* layout = new QVBoxLayout(container);
        layout->setContentsMargins(2, 2, 2, 2);
        layout->setSpacing(1);

        // 标题行
        auto* titleBar = new QWidget();
        auto* titleLayout = new QHBoxLayout(titleBar);
        titleLayout->setContentsMargins(4, 2, 4, 2);
        auto* titleLabel = new QLabel(QString("CCD%1").arg(i + 1));
        titleLabel->setStyleSheet("color: #4a6a8c; font-size: 11px; font-weight: bold;");
        auto* statusLabel = new QLabel("");
        statusLabel->setStyleSheet("color: #4a6a8c; font-size: 10px;");
        titleLayout->addWidget(titleLabel);
        titleLayout->addStretch();
        titleLayout->addWidget(statusLabel);
        layout->addWidget(titleBar);

        // 图像查看器(缩放/平移/叠加)
        auto* viewer = new ImageViewWidget();
        layout->addWidget(viewer, 1);

        m_gridLayout->addWidget(container, i / m_cols, i % m_cols);
        m_imageViews.append(viewer);
        m_titleLabels.append(titleLabel);
        m_statusLabels.append(statusLabel);
    }
}

void MultiViewWidget::setImage(int index, const QImage& image) {
    if (index < 0 || index >= m_imageViews.size()) return;
    if (image.isNull())
        m_imageViews[index]->clearImage();
    else
        m_imageViews[index]->setImage(image);
}

void MultiViewWidget::setOverlays(int index, const QVariantList& overlays) {
    if (index < 0 || index >= m_imageViews.size()) return;
    m_imageViews[index]->setOverlays(overlays);
}

void MultiViewWidget::setTitle(int index, const QString& title) {
    if (index < 0 || index >= m_titleLabels.size()) return;
    m_titleLabels[index]->setText(title);
}

void MultiViewWidget::setStatus(int index, const QString& status) {
    if (index < 0 || index >= m_statusLabels.size()) return;
    m_statusLabels[index]->setText(status);
    // 状态着色
    if (status.contains("OK")) m_statusLabels[index]->setStyleSheet("color: #0f0; font-size: 10px;");
    else if (status.contains("NG")) m_statusLabels[index]->setStyleSheet("color: #f44; font-size: 10px;");
    else m_statusLabels[index]->setStyleSheet("color: #4a6a8c; font-size: 10px;");
}

void MultiViewWidget::clearAll() {
    for (auto* v : m_imageViews)
        v->clearImage();
    for (auto* label : m_statusLabels)
        label->setText("");
}

} // namespace VisionInspector
