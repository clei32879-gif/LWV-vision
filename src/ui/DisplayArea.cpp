/** @file DisplayArea.cpp - 显示区域（支持缩放控制） */
#include "DisplayArea.h"
#include "../utils/Logger.h"
#include <QVBoxLayout>
#include <QPainter>
#include <QScrollArea>

namespace VisionInspector {

DisplayArea::DisplayArea(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet("background-color: #1a1a1a;");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet("color: #4a6a8c; font-size: 18px;");
    m_imageLabel->setText("LW Vision\n\n等待加载图像...");
    m_imageLabel->setMinimumSize(200, 150);

    layout->addWidget(m_imageLabel);
}

void DisplayArea::displayImage(const CvImage& image) {
#ifdef VI_HAS_OPENCV
    if (image.empty()) {
        m_imageLabel->setText("无图像");
        return;
    }
    m_currentImage = cvMatToQImage(image);
#else
    if (image.isNull()) {
        m_imageLabel->setText("无图像");
        return;
    }
    m_currentImage = image;
#endif
    m_fitMode = true;
    updateDisplay();
}

void DisplayArea::clear() {
    m_currentImage = QImage();
    m_imageLabel->clear();
    m_imageLabel->setText("无图像");
}

void DisplayArea::zoomIn() {
    m_fitMode = false;
    m_zoomLevel *= 1.25;
    if (m_zoomLevel > 10.0) m_zoomLevel = 10.0;
    updateDisplay();
    emit zoomChanged(m_zoomLevel);
}

void DisplayArea::zoomOut() {
    m_fitMode = false;
    m_zoomLevel /= 1.25;
    if (m_zoomLevel < 0.1) m_zoomLevel = 0.1;
    updateDisplay();
    emit zoomChanged(m_zoomLevel);
}

void DisplayArea::zoom1x1() {
    m_fitMode = false;
    m_zoomLevel = 1.0;
    updateDisplay();
    emit zoomChanged(m_zoomLevel);
}

void DisplayArea::zoomFit() {
    m_fitMode = true;
    updateDisplay();
    emit zoomChanged(m_zoomLevel);
}

void DisplayArea::wheelEvent(QWheelEvent* event) {
    if (event->angleDelta().y() > 0) {
        zoomIn();
    } else {
        zoomOut();
    }
    event->accept();
}

void DisplayArea::updateDisplay() {
    if (m_currentImage.isNull()) return;

    if (m_fitMode) {
        QPixmap scaled = QPixmap::fromImage(m_currentImage).scaled(
            m_imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_imageLabel->setPixmap(scaled);
        // 计算实际缩放比例
        if (!scaled.isNull() && m_currentImage.width() > 0 && m_currentImage.height() > 0) {
            double scaleX = (double)scaled.width() / m_currentImage.width();
            double scaleY = (double)scaled.height() / m_currentImage.height();
            m_zoomLevel = qMin(scaleX, scaleY);
        }
    } else {
        int w = (int)(m_currentImage.width() * m_zoomLevel);
        int h = (int)(m_currentImage.height() * m_zoomLevel);
        m_imageLabel->setPixmap(QPixmap::fromImage(m_currentImage).scaled(
            w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void DisplayArea::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
}

} // namespace VisionInspector
