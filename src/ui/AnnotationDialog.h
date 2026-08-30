/**
 * @file AnnotationDialog.h
 * @brief 标注工具 — 软件内框选标注, 导出YOLO训练集 (阶段4)
 *
 * 流程: 选图库文件夹 → 逐张拖框标注缺陷 → 保存YOLO标签 → 导出训练集
 * 标签格式: YOLO检测 (class cx cy w h, 归一化), 类别表 classes.txt
 */
#pragma once

#include "widgets/ImageViewWidget.h"
#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QStringList>

namespace VisionInspector {

class AnnotationDialog : public QDialog {
    Q_OBJECT
public:
    explicit AnnotationDialog(QWidget* parent = nullptr);

private slots:
    void onPickFolder();
    void onImageSelected(int index);
    void onPrev();
    void onNext();
    void onSaveLabel();
    void onSaveAndNext();
    void onDeleteSelected();
    void onExportDataset();

private:
    void setupUi();
    void loadImage(int index);
    void saveLabel(int index);
    QString labelPathFor(const QString& imageFile) const;
    void refreshClassCombo();

    ImageViewWidget* m_view = nullptr;
    QListWidget* m_imageList = nullptr;
    QListWidget* m_rectList = nullptr;
    QComboBox* m_classCombo = nullptr;
    QLabel* m_posLabel = nullptr;
    QLineEdit* m_folderEdit = nullptr;
    QString m_folder;
    QStringList m_images;
    int m_cur = -1;
    QSize m_curImageSize;
};

} // namespace VisionInspector
