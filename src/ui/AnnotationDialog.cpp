/**
 * @file AnnotationDialog.cpp
 * @brief 标注工具实现 — YOLO检测标注 + 训练集导出
 */
#include "AnnotationDialog.h"
#include "../utils/Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QSplitter>
#include <QGroupBox>

namespace VisionInspector {

AnnotationDialog::AnnotationDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    setWindowTitle(QStringLiteral("缺陷标注工具 (YOLO检测格式)"));
    resize(1200, 760);
}

void AnnotationDialog::setupUi() {
    auto* root = new QVBoxLayout(this);

    // 顶部: 文件夹选择 + 类别
    auto* topBar = new QHBoxLayout();
    m_folderEdit = new QLineEdit(this);
    m_folderEdit->setPlaceholderText(QStringLiteral("选择图片文件夹..."));
    auto* browseBtn = new QPushButton(QStringLiteral("浏览..."), this);
    topBar->addWidget(new QLabel(QStringLiteral("图库:"), this));
    topBar->addWidget(m_folderEdit, 1);
    topBar->addWidget(browseBtn);
    topBar->addWidget(new QLabel(QStringLiteral("类别:"), this));
    m_classCombo = new QComboBox(this);
    m_classCombo->setEditable(true);
    m_classCombo->addItems({QStringLiteral("开裂"), QStringLiteral("头麻"),
                            QStringLiteral("发黑"), QStringLiteral("头疤"),
                            QStringLiteral("电镀不良"), QStringLiteral("未点胶"),
                            QStringLiteral("良品")});
    m_classCombo->setMinimumWidth(110);
    topBar->addWidget(m_classCombo);
    root->addLayout(topBar);

    // 中部: 图像 | 列表
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    m_view = new ImageViewWidget();
    m_view->setAnnotationMode(true);
    splitter->addWidget(m_view);

    auto* sidePanel = new QWidget(this);
    auto* sideLayout = new QVBoxLayout(sidePanel);
    sideLayout->addWidget(new QLabel(QStringLiteral("图片列表:"), sidePanel));
    m_imageList = new QListWidget(sidePanel);
    sideLayout->addWidget(m_imageList, 2);
    sideLayout->addWidget(new QLabel(QStringLiteral("当前图标注框:"), sidePanel));
    m_rectList = new QListWidget(sidePanel);
    sideLayout->addWidget(m_rectList, 1);
    auto* delBtn = new QPushButton(QStringLiteral("删除选中框"), sidePanel);
    sideLayout->addWidget(delBtn);
    splitter->addWidget(sidePanel);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);

    // 底部: 导航 + 保存
    auto* bottomBar = new QHBoxLayout();
    auto* prevBtn = new QPushButton(QStringLiteral("◀ 上一张"), this);
    auto* nextBtn = new QPushButton(QStringLiteral("下一张 ▶"), this);
    m_posLabel = new QLabel(QStringLiteral("未加载"), this);
    auto* saveBtn = new QPushButton(QStringLiteral("保存标签"), this);
    auto* saveNextBtn = new QPushButton(QStringLiteral("保存并下一张 (S)"), this);
    auto* exportBtn = new QPushButton(QStringLiteral("导出训练集..."), this);
    bottomBar->addWidget(prevBtn);
    bottomBar->addWidget(nextBtn);
    bottomBar->addWidget(m_posLabel, 1);
    bottomBar->addWidget(saveBtn);
    bottomBar->addWidget(saveNextBtn);
    bottomBar->addWidget(exportBtn);
    root->addLayout(bottomBar);

    connect(browseBtn, &QPushButton::clicked, this, &AnnotationDialog::onPickFolder);
    connect(prevBtn, &QPushButton::clicked, this, &AnnotationDialog::onPrev);
    connect(nextBtn, &QPushButton::clicked, this, &AnnotationDialog::onNext);
    connect(m_imageList, &QListWidget::currentRowChanged, this, &AnnotationDialog::onImageSelected);
    connect(saveBtn, &QPushButton::clicked, this, &AnnotationDialog::onSaveLabel);
    connect(saveNextBtn, &QPushButton::clicked, this, &AnnotationDialog::onSaveAndNext);
    connect(exportBtn, &QPushButton::clicked, this, &AnnotationDialog::onExportDataset);
    connect(delBtn, &QPushButton::clicked, this, &AnnotationDialog::onDeleteSelected);
    connect(m_view, &ImageViewWidget::annotationCreated, this, [this](const QRectF&) {
        refreshClassCombo();
        // 把新框加入列表
        const QList<QRectF>& rects = m_view->annotations();
        if (!rects.isEmpty() && m_rectList->count() < rects.size()) {
            const QRectF r = rects.last();
            m_rectList->addItem(QStringLiteral("[%1] (%2,%3 %4×%5)")
                                    .arg(m_classCombo->currentText())
                                    .arg(r.x(), 0, 'f', 0).arg(r.y(), 0, 'f', 0)
                                    .arg(r.width(), 0, 'f', 0).arg(r.height(), 0, 'f', 0));
        }
    });
    connect(m_view, &ImageViewWidget::annotationDeleted, this, [this](int idx) {
        if (idx >= 0 && idx < m_rectList->count())
            delete m_rectList->item(idx);
    });
}

void AnnotationDialog::refreshClassCombo() {
    if (m_folder.isEmpty()) return;
    QFile cf(m_folder + "/labels/classes.txt");
    if (!cf.open(QIODevice::ReadOnly)) return;
    const QStringList existing;
    Q_UNUSED(existing);
    QTextStream in(&cf);
    int idx = 0;
    while (!in.atEnd()) {
        const QString cls = in.readLine().trimmed();
        if (cls.isEmpty()) continue;
        if (m_classCombo->findText(cls) < 0) {
            // 保持顺序与类别索引一致: 插到对应位置
            m_classCombo->insertItem(std::min(idx, m_classCombo->count()), cls);
        }
        ++idx;
    }
}

void AnnotationDialog::onPickFolder() {
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择图片文件夹"),
        m_folder.isEmpty() ? QStringLiteral("G:/图片素材") : m_folder);
    if (dir.isEmpty()) return;
    m_folder = dir;
    m_folderEdit->setText(dir);

    m_images.clear();
    for (const QFileInfo& fi : QDir(dir).entryInfoList(
             {"*.png", "*.jpg", "*.jpeg", "*.bmp"}, QDir::Files, QDir::Name)) {
        if (fi.suffix().compare(QStringLiteral("bmp"), Qt::CaseInsensitive) == 0)
            m_images << fi.absoluteFilePath();   // 保留bmp原路径
        else
            m_images << fi.absoluteFilePath();
    }
    refreshClassCombo();
    m_imageList->clear();
    for (const QString& f : m_images)
        m_imageList->addItem(QFileInfo(f).fileName());
    if (!m_images.isEmpty())
        m_imageList->setCurrentRow(0);
    Logger::instance().log(static_cast<LogLevel>(1),
                           QString("标注: 载入图库 %1 共%2张").arg(dir).arg(m_images.size()));
}

void AnnotationDialog::onImageSelected(int index) {
    loadImage(index);
}

void AnnotationDialog::loadImage(int index) {
    if (index < 0 || index >= m_images.size()) return;
    // 切图前保存上一张
    if (m_cur >= 0 && m_cur < m_images.size() && m_cur != index)
        saveLabel(m_cur);
    m_cur = index;
    const QString path = m_images[index];
    QImage img(path);
    m_curImageSize = img.size();
    m_view->setImage(img);
    m_view->setAnnotations({});
    m_rectList->clear();
    m_posLabel->setText(QString("%1 / %2  %3  (%4×%5)")
                            .arg(index + 1).arg(m_images.size())
                            .arg(QFileInfo(path).fileName())
                            .arg(img.width()).arg(img.height()));
    // 已有标签则载入
    const QString lp = labelPathFor(path);
    QFile f(lp);
    if (f.open(QIODevice::ReadOnly)) {
        QList<QRectF> rects;
        QTextStream in(&f);
        while (!in.atEnd()) {
            const QStringList parts = in.readLine().trimmed().split(' ', Qt::SkipEmptyParts);
            if (parts.size() < 5) continue;
            const double cx = parts[1].toDouble(), cy = parts[2].toDouble();
            const double w = parts[3].toDouble(), h = parts[4].toDouble();
            rects.append(QRectF((cx - w / 2) * img.width(), (cy - h / 2) * img.height(),
                                w * img.width(), h * img.height()));
        }
        m_view->setAnnotations(rects);
        for (const QRectF& r : rects)
            m_rectList->addItem(QStringLiteral("(已有) (%1,%2 %3×%4)")
                                    .arg(r.x(), 0, 'f', 0).arg(r.y(), 0, 'f', 0)
                                    .arg(r.width(), 0, 'f', 0).arg(r.height(), 0, 'f', 0));
    }
}

void AnnotationDialog::onPrev() {
    if (m_cur > 0) m_imageList->setCurrentRow(m_cur - 1);
}

void AnnotationDialog::onNext() {
    if (m_cur + 1 < m_images.size()) m_imageList->setCurrentRow(m_cur + 1);
}

QString AnnotationDialog::labelPathFor(const QString& imageFile) const {
    const QFileInfo fi(imageFile);
    return fi.absolutePath() + "/labels/" + fi.completeBaseName() + ".txt";
}

void AnnotationDialog::saveLabel(int index) {
    if (index < 0 || index >= m_images.size()) return;
    const QString path = m_images[index];
    QDir().mkpath(QFileInfo(path).absolutePath() + "/labels");
    QFile f(labelPathFor(path));
    if (!f.open(QIODevice::WriteOnly)) return;
    QTextStream out(&f);
    // 类别表
    const QStringList classes;
    Q_UNUSED(classes);
    // 每个框: 找它对应的类别 (按创建顺序记录在rectList? 简化: 全部用当前类别)
    // v1简化: 标注列表顺序即创建顺序, 类别从rectList文本解析不可靠 — 使用"当前类别"批量
    // 改进(后续): rect存类别。此处按创建时类别逐框存——rectList文本含[类别]
    const QList<QRectF>& rects = m_view->annotations();
    for (int i = 0; i < rects.size(); ++i) {
        // 类别: 从rectList取(含[类别]), 列表缺项时用当前类别
        QString clsName = m_classCombo->currentText();
        if (i < m_rectList->count()) {
            const QString text = m_rectList->item(i)->text();
            const int b = text.indexOf('['), e = text.indexOf(']');
            if (b >= 0 && e > b) clsName = text.mid(b + 1, e - b - 1);
        }
        // 类别索引
        int clsIdx = m_classCombo->findText(clsName);
        if (clsIdx < 0) { m_classCombo->addItem(clsName); clsIdx = m_classCombo->count() - 1; }
        const QRectF& r = rects[i];
        const double cx = (r.x() + r.width() / 2) / m_curImageSize.width();
        const double cy = (r.y() + r.height() / 2) / m_curImageSize.height();
        out << clsIdx << ' ' << QString::number(cx, 'f', 6) << ' '
            << QString::number(cy, 'f', 6) << ' '
            << QString::number(r.width() / m_curImageSize.width(), 'f', 6) << ' '
            << QString::number(r.height() / m_curImageSize.height(), 'f', 6) << '\n';
    }
}

void AnnotationDialog::onSaveLabel() {
    if (m_cur < 0) return;
    saveLabel(m_cur);
    // 同步写类别表
    QDir().mkpath(QFileInfo(m_images[m_cur]).absolutePath() + "/labels");
    QFile cf(QFileInfo(m_images[m_cur]).absolutePath() + "/labels/classes.txt");
    if (cf.open(QIODevice::WriteOnly)) {
        QTextStream out(&cf);
        for (int i = 0; i < m_classCombo->count(); ++i)
            out << m_classCombo->itemText(i) << '\n';
    }
    m_posLabel->setText(m_posLabel->text() + QStringLiteral("  ✓已保存"));
}

void AnnotationDialog::onSaveAndNext() {
    onSaveLabel();
    onNext();
}

void AnnotationDialog::onDeleteSelected() {
    const int row = m_rectList->currentRow();
    if (row < 0) return;
    if (row < m_view->annotations().size()) {
        QList<QRectF> rects = m_view->annotations();
        rects.removeAt(row);
        m_view->setAnnotations(rects);
    }
    delete m_rectList->item(row);
}

void AnnotationDialog::onExportDataset() {
    if (m_images.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("导出"), QStringLiteral("请先加载图库"));
        return;
    }
    QString outDir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择训练集导出目录(生成images/labels结构与data.yaml)"), m_folder);
    if (outDir.isEmpty()) return;

    // 把已标注的图与标签收集为 ultralytics 训练结构
    int copied = 0;
    for (int i = 0; i < m_images.size(); ++i) {
        const QFileInfo fi(m_images[i]);
        const QString lp = labelPathFor(m_images[i]);
        if (!QFileInfo::exists(lp)) continue;   // 未标注的跳过
        QDir().mkpath(outDir + "/images/train");
        QDir().mkpath(outDir + "/labels/train");
        const QString dstImg = outDir + "/images/train/" + fi.fileName();
        if (!QFileInfo::exists(dstImg))
            QFile::copy(m_images[i], dstImg);
        QFile::copy(lp, outDir + "/labels/train/" + fi.completeBaseName() + ".txt");
        ++copied;
    }
    // data.yaml
    QFile yaml(outDir + "/data.yaml");
    if (yaml.open(QIODevice::WriteOnly)) {
        QTextStream out(&yaml);
        out << "# LW Vision 标注工具导出\n";
        out << "path: " << QDir::toNativeSeparators(outDir) << "\n";
        out << "train: images/train\n";
        out << "val: images/train\n";
        out << "names:\n";
        for (int i = 0; i < m_classCombo->count(); ++i)
            out << "  " << i << ": " << m_classCombo->itemText(i) << "\n";
    }
    QMessageBox::information(this, QStringLiteral("导出完成"),
        QStringLiteral("已导出 %1 张已标注图到:\n%2\n\n训练命令(装有ultralytics的机器):\n"
                       "yolo detect train data=<目录>/data.yaml model=yolov8n.pt epochs=50 imgsz=640")
            .arg(copied).arg(QDir::toNativeSeparators(outDir)));
    VI_LOG_INFO(QString("标注导出: %1张 -> %2").arg(copied).arg(outDir));
}

} // namespace VisionInspector
