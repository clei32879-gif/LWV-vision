#include "PropertyDialog.h"
#include "IconHelper.h"
#include "../engine/ToolRegistry.h"
#include "widgets/ImageViewWidget.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QFileDialog>
#include <QPushButton>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QTabWidget>
#include <QTimer>
#include <algorithm>

namespace VisionInspector {

PropertyDialog::PropertyDialog(ITool* tool, const QStringList& availableTools,
                               const QMap<QString, QStringList>& linkableData,
                               const CvImagePtr& lastImage,
                               QWidget* parent)
    : QDialog(parent), m_tool(tool), m_availableTools(availableTools),
      m_linkableData(linkableData), m_lastImage(lastImage) {
    setWindowTitle(QString("属性编辑 - %1").arg(tool->displayName()));
    setWindowIcon(IconHelper::categoryIcon(tool->category(), 32));
    setMinimumSize(560, 520);

    // 即改即显: 参数变化后 300ms 防抖自动试执行
    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(300);
    connect(m_previewTimer, &QTimer::timeout, this, &PropertyDialog::updatePreview);

    buildUI();
}

void PropertyDialog::buildUI() {
    auto* mainLayout = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);
    mainLayout->addWidget(m_tabs, 1);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &PropertyDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &PropertyDialog::reject);
    mainLayout->addWidget(m_buttonBox);

    // ============ 页签1: 基本设置 ============
    auto* basicPage = new QWidget(this);
    auto* basicForm = new QFormLayout(basicPage);
    m_nameEdit = new QLineEdit(m_tool->instanceName(), basicPage);
    basicForm->addRow(QStringLiteral("工具名:"), m_nameEdit);
    m_commentEdit = new QPlainTextEdit(m_tool->comment(), basicPage);
    m_commentEdit->setFixedHeight(70);
    basicForm->addRow(QStringLiteral("注释:"), m_commentEdit);
    m_activeCheck = new QCheckBox(QStringLiteral("启用此工具 (取消勾选后流程执行时跳过)"), basicPage);
    m_activeCheck->setChecked(m_tool->isActive());
    basicForm->addRow(m_activeCheck);
    basicForm->addRow(new QLabel(
        QStringLiteral("类型: %1    分类: %2")
            .arg(m_tool->typeName(), categoryToString(m_tool->category())), basicPage));
    // 工具说明: 告诉新用户这个工具是干什么的
    {
        const QString desc = ToolRegistry::instance().metaData(m_tool->typeName()).description;
        if (!desc.isEmpty()) {
            auto* descLabel = new QLabel(desc, basicPage);
            descLabel->setWordWrap(true);
            descLabel->setStyleSheet("color: #9aa0a6; padding: 2px;");
            basicForm->addRow(descLabel);
        }
    }
    m_tabs->addTab(basicPage, QStringLiteral("基本设置"));

    // ============ 页签2: 参数设置 ============
    auto* paramPage = new QWidget(this);
    auto* paramLayout = new QVBoxLayout(paramPage);
    // 使用提示: 让用户一眼知道怎么操作
    auto* hint = new QLabel(
        QStringLiteral("说明：修改参数后会自动在[试执行]页预览效果；数值/下拉项带悬浮说明。"), paramPage);
    hint->setStyleSheet("color: #4a9eff; background: #1e2a3a; padding: 6px; border-radius: 3px;");
    hint->setWordWrap(true);
    paramLayout->addWidget(hint);
    m_formLayout = new QFormLayout();
    paramLayout->addLayout(m_formLayout);
    paramLayout->addStretch();
    m_tabs->addTab(paramPage, QStringLiteral("参数设置"));

    const bool isCaptureTool = (m_tool->typeName() == "CaptureImage");
    const bool needsImage = ITool::needsInputImage(m_tool->typeName());

    if (isCaptureTool) {
        auto* combo = new QComboBox(paramPage);
        combo->setMinimumWidth(200);
        combo->addItem("当前图像");
        combo->addItem("相机图像");
        combo->addItem("文件图像");
        QString currentInput = m_tool->propertyValue("inputImage").toString();
        int idx = combo->findText(currentInput);
        if (idx >= 0) combo->setCurrentIndex(idx);
        m_formLayout->addRow("图像来源:", combo);
        m_editors["inputImage"] = combo;
        connectAutoPreview(combo);
    } else if (needsImage) {
        auto* combo = new QComboBox(paramPage);
        combo->setMinimumWidth(200);
        combo->addItem("当前图像");
        for (const QString& toolName : m_availableTools)
            combo->addItem(toolName);
        QString currentInput = m_tool->propertyValue("inputImage").toString();
        int idx = combo->findText(currentInput);
        if (idx >= 0) combo->setCurrentIndex(idx);
        m_formLayout->addRow("输入图像:", combo);
        m_editors["inputImage"] = combo;
        connectAutoPreview(combo);
    }

    const PropertyDefList defs = m_tool->propertyDefs();
    for (const auto& def : defs) {
        QWidget* editor = nullptr;
        QVariant currentVal = m_tool->propertyValue(def.name);

        switch (def.type) {
        case PropertyType::Int: {
            auto* spin = new QSpinBox(paramPage);
            spin->setMinimum(def.minValue.toInt());
            spin->setMaximum(def.maxValue.toInt());
            spin->setValue(currentVal.toInt());
            editor = spin;
            break;
        }
        case PropertyType::Double: {
            auto* dspin = new QDoubleSpinBox(paramPage);
            dspin->setDecimals(3);
            dspin->setMinimum(def.minValue.toDouble());
            dspin->setMaximum(def.maxValue.toDouble());
            dspin->setValue(currentVal.toDouble());
            editor = dspin;
            break;
        }
        case PropertyType::Boolean: {
            auto* check = new QCheckBox(paramPage);
            check->setChecked(currentVal.toBool());
            editor = check;
            break;
        }
        case PropertyType::Enum: {
            auto* combo = new QComboBox(paramPage);
            combo->addItems(def.enumValues);
            combo->setCurrentIndex(currentVal.toInt());
            editor = combo;
            break;
        }
        case PropertyType::String:
        default: {
            QString name = def.name.toLower();
            if (name.contains("path") || name.contains("file") || name.contains("dir") || name.contains("image")) {
                bool isDir = name.contains("dir");
                QString filter = isDir ? "所有文件 (*)" :
                    "图像文件 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;所有文件 (*)";
                editor = createPathEditor(def.name, currentVal.toString(), filter, isDir);
            } else {
                editor = createLinkEditor(def.name, currentVal.toString());
            }
            break;
        }
        }

        if (editor) {
            QString label = def.label;
            if (!def.group.isEmpty()) label += QString(" [%1]").arg(def.group);
            auto* labelWidget = new QLabel(label + ":", paramPage);
            // 悬浮说明: PropertyDef.tooltip 声明的参数含义, 标签与编辑器都可见
            if (!def.tooltip.isEmpty()) {
                labelWidget->setToolTip(def.tooltip);
                editor->setToolTip(def.tooltip);
            }
            m_formLayout->addRow(labelWidget, editor);
            m_editors[def.name] = editor;
            connectAutoPreview(editor);
        }
    }

    // ============ 页签3: 数据判定 ============
    auto* judgePage = new QWidget(this);
    auto* judgeLayout = new QVBoxLayout(judgePage);
    judgeLayout->addWidget(new QLabel(
        QStringLiteral("勾选启用后, 对应结果超出上下限即判NG (结果键来自试执行/最近一次执行):"), judgePage));
    m_judgeContainer = new QWidget(judgePage);
    m_judgeLayout = new QVBoxLayout(m_judgeContainer);
    m_judgeLayout->setContentsMargins(0, 0, 0, 0);
    judgeLayout->addWidget(m_judgeContainer);
    m_tabs->addTab(judgePage, QStringLiteral("数据判定"));
    buildJudgeSection();

    // ============ 页签4: 试执行 ============
    // 是否具备图形化ROI编辑条件
    // 主流约定: roiCenterX/Y + roiWidth/Height [+roiAngle] (34个工具)
    m_hasRoi = m_editors.contains("roiCenterX") && m_editors.contains("roiCenterY")
            && m_editors.contains("roiWidth") && m_editors.contains("roiHeight");
    m_roiCornerMode = false;
    if (!m_hasRoi) {
        // 角点约定: useROI + roiX/roiY/roiW/roiH (条码/二维码/OCR 等老工具)
        m_hasRoi = m_editors.contains("roiX") && m_editors.contains("roiY")
                && m_editors.contains("roiW") && m_editors.contains("roiH");
        m_roiCornerMode = m_hasRoi;
    }

    auto* tryPage = new QWidget(this);
    auto* tryLayout = new QVBoxLayout(tryPage);
    auto* tryHint = new QLabel(
        QStringLiteral("修改任何参数后自动试执行 (0.3秒防抖); 也可点按钮立即重跑。"), tryPage);
    tryHint->setStyleSheet("color: #9aa0a6; font-size: 12px;");
    tryLayout->addWidget(tryHint);
    auto* tryBtn = new QPushButton(QStringLiteral("▶ 立即试执行当前参数"), tryPage);
    tryLayout->addWidget(tryBtn);

    if (m_hasRoi) {
        m_roiEditBtn = new QPushButton(QStringLiteral("▣ 在图上拖拽编辑ROI (移动/缩放)"), tryPage);
        m_roiEditBtn->setCheckable(true);
        tryLayout->addWidget(m_roiEditBtn);
        connect(m_roiEditBtn, &QPushButton::toggled, this, [this](bool on) {
            m_previewViewer->setRoiEditing(on);
            if (on) {
                // ROI类型为"无"时自动切到"矩形"; useROI未勾选时自动勾上
                if (auto* typeCombo = qobject_cast<QComboBox*>(m_editors.value("roiType")))
                    if (typeCombo->currentIndex() == 0)
                        typeCombo->setCurrentIndex(1);
                if (auto* useRoi = qobject_cast<QCheckBox*>(m_editors.value("useROI")))
                    if (!useRoi->isChecked())
                        useRoi->setChecked(true);
                syncRoiToViewer();
            }
        });
    }

    m_previewViewer = new ImageViewWidget(tryPage);
    tryLayout->addWidget(m_previewViewer, 1);
    if (m_hasRoi)
        connect(m_previewViewer, &ImageViewWidget::roiEdited,
                this, &PropertyDialog::onRoiEdited);
    connect(tryBtn, &QPushButton::clicked, this, [this]() { updatePreview(); });
    m_tabs->addTab(tryPage, QStringLiteral("试执行"));

    // 默认定位到"参数设置", 让用户先看到该工具最关键的配置
    m_tabs->setCurrentIndex(1);
}

void PropertyDialog::onTryRun() {
    // 临时保存界面参数 → 执行 → 恢复 (与正式accept分离)
    struct Restore { ITool* t; QMap<QString,QVariant> saved; } r{m_tool, {}};
    // 读取界面值(仅缓存, 不写回) — 直接执行: 把界面参数临时套用
    PropertyDefList defs = m_tool->propertyDefs();
    for (const auto& def : defs) {
        QWidget* editor = m_editors.value(def.name);
        if (!editor) continue;
        QVariant v;
        switch (def.type) {
        case PropertyType::Int:    v = qobject_cast<QSpinBox*>(editor)->value(); break;
        case PropertyType::Double: v = qobject_cast<QDoubleSpinBox*>(editor)->value(); break;
        case PropertyType::Boolean:v = qobject_cast<QCheckBox*>(editor)->isChecked(); break;
        case PropertyType::Enum:   v = qobject_cast<QComboBox*>(editor)->currentIndex(); break;
        default: {
            auto* container = qobject_cast<QWidget*>(editor);
            QLineEdit* line = container ? container->findChild<QLineEdit*>() : nullptr;
            if (!line) line = qobject_cast<QLineEdit*>(editor);
            if (line) v = line->text();
            break;
        }
        }
        if (v.isValid()) {
            r.saved[def.name] = m_tool->propertyValue(def.name);
            m_tool->setProperty(def.name, v);
        }
    }
    // 执行(试)
    if (m_lastImage && !m_lastImage->empty()) {
        ToolContext ctx;
        ctx.setCurrentImage(std::make_shared<CvImage>(m_lastImage->clone()));
        m_tool->execute(ctx);
    }
    // 恢复原属性
    for (auto it = r.saved.begin(); it != r.saved.end(); ++it)
        m_tool->setProperty(it.key(), it.value());
    (void)r;
}

void PropertyDialog::updatePreview() {
    if (!m_lastImage || m_lastImage->empty()) return; // 无图像时不自动预览
    onTryRun();
    if (!m_previewViewer) return;
    m_previewViewer->setImage(cvMatToQImage(*m_lastImage));
    QVariantList overlays;
    if (m_tool->status() == ToolStatus::OK)
        overlays = QVector<QVariant>(m_tool->overlays().begin(),
                                     m_tool->overlays().end()).toList();
    m_previewViewer->setOverlays(overlays);
    // ROI编辑开启时保持数值→框同步 (拖拽中由setRoiRect内部忽略, 不会打架)
    if (m_roiEditBtn && m_roiEditBtn->isChecked())
        syncRoiToViewer();
    // 判定表此前为空(无结果键)时, 试执行拿到结果键后立即补建, 不用先跑整条流程
    if (!m_judgeTable && !m_tool->resultData().isEmpty())
        buildJudgeSection();
}

void PropertyDialog::syncRoiToViewer() {
    auto spinVal = [this](const char* name) -> double {
        if (auto* d = qobject_cast<QDoubleSpinBox*>(m_editors.value(name)))
            return d->value();
        if (auto* s = qobject_cast<QSpinBox*>(m_editors.value(name)))
            return s->value();
        return 0.0;
    };
    if (m_roiCornerMode) { // 角点约定: 直接就是左上角+宽高
        const double x = spinVal("roiX"), y = spinVal("roiY");
        const double w = std::max(4.0, spinVal("roiW"));
        const double h = std::max(4.0, spinVal("roiH"));
        m_previewViewer->setRoiRect(QRectF(x, y, w, h));
        return;
    }
    const double cx = spinVal("roiCenterX"), cy = spinVal("roiCenterY");
    const double w = std::max(4.0, spinVal("roiWidth"));
    const double h = std::max(4.0, spinVal("roiHeight"));
    const double angle = m_editors.contains("roiAngle") ? spinVal("roiAngle") : 0.0;
    m_previewViewer->setRoiRect(QRectF(cx - w / 2.0, cy - h / 2.0, w, h), angle);
}

void PropertyDialog::onRoiEdited(const QRectF& rect) {
    auto setSpin = [this](const char* name, double v) {
        if (auto* d = qobject_cast<QDoubleSpinBox*>(m_editors.value(name)))
            d->setValue(v); // 触发 valueChanged → 自动预览
        else if (auto* s = qobject_cast<QSpinBox*>(m_editors.value(name)))
            s->setValue((int)std::lround(v));
    };
    if (m_roiCornerMode) {
        setSpin("roiX", rect.left());
        setSpin("roiY", rect.top());
        setSpin("roiW", rect.width());
        setSpin("roiH", rect.height());
        return;
    }
    setSpin("roiCenterX", rect.center().x());
    setSpin("roiCenterY", rect.center().y());
    setSpin("roiWidth", rect.width());
    setSpin("roiHeight", rect.height());
}

void PropertyDialog::connectAutoPreview(QWidget* editor) {
    if (!editor) return;
    auto kick = [this]() { m_previewTimer->start(); };
    if (auto* s = qobject_cast<QSpinBox*>(editor))
        connect(s, &QSpinBox::valueChanged, this, kick);
    else if (auto* d = qobject_cast<QDoubleSpinBox*>(editor))
        connect(d, &QDoubleSpinBox::valueChanged, this, kick);
    else if (auto* c = qobject_cast<QCheckBox*>(editor))
        connect(c, &QCheckBox::toggled, this, kick);
    else if (auto* cb = qobject_cast<QComboBox*>(editor))
        connect(cb, &QComboBox::currentIndexChanged, this, kick);
    else if (auto* le = qobject_cast<QLineEdit*>(editor))
        connect(le, &QLineEdit::textChanged, this, kick);
    else // 路径/数据链接容器: 接内部 QLineEdit
        if (auto* inner = editor->findChild<QLineEdit*>())
            connect(inner, &QLineEdit::textChanged, this, kick);
}

void PropertyDialog::buildJudgeSection() {
    // 清空旧内容(表或占位提示), 支持试执行后重建
    if (m_judgeTable) {
        m_judgeTable->deleteLater();
        m_judgeTable = nullptr;
    }
    QLayoutItem* child;
    while ((child = m_judgeLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    QList<ResultJudgment> judges = m_tool->judgments();
    const QStringList resultKeys = m_tool->resultData().keys();
    for (const QString& key : resultKeys) {
        bool exists = false;
        for (const auto& j : judges)
            if (j.resultKey == key) { exists = true; break; }
        if (!exists && key != "judgeFailedKeys" && key != "error" && key != "status") {
            ResultJudgment j;
            j.resultKey = key;
            judges.append(j);
        }
    }
    if (judges.isEmpty()) {
        m_judgeLayout->addWidget(new QLabel(
            QStringLiteral("(尚无结果键 — 切到[试执行]页跑一次, 或先执行一次流程)"), m_judgeContainer));
        return;
    }

    m_judgeTable = new QTableWidget(judges.size(), 4, this);
    m_judgeTable->setHorizontalHeaderLabels({"结果键", "启用", "下限", "上限"});
    m_judgeTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_judgeTable->verticalHeader()->setVisible(false);
    for (int r = 0; r < judges.size(); ++r) {
        const ResultJudgment& j = judges[r];
        auto* keyItem = new QTableWidgetItem(j.resultKey);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        m_judgeTable->setItem(r, 0, keyItem);

        auto* enableItem = new QTableWidgetItem();
        enableItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        enableItem->setCheckState(j.enabled ? Qt::Checked : Qt::Unchecked);
        m_judgeTable->setItem(r, 1, enableItem);

        m_judgeTable->setItem(r, 2, new QTableWidgetItem(
            j.lower <= -1e17 ? QString() : QString::number(j.lower)));
        m_judgeTable->setItem(r, 3, new QTableWidgetItem(
            j.upper >= 1e17 ? QString() : QString::number(j.upper)));
    }
    m_judgeLayout->addWidget(m_judgeTable);
}

QWidget* PropertyDialog::createLinkEditor(const QString& name, const QString& value) {
    auto* container = new QWidget(this);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* lineEdit = new QLineEdit(value, container);
    lineEdit->setPlaceholderText("可直接输入, 或点右侧按钮插入数据引用");
    layout->addWidget(lineEdit, 1);

    auto* linkBtn = new QPushButton("...", container);
    linkBtn->setMaximumWidth(30);
    linkBtn->setToolTip("插入数据链接: 引用前序工具的结果值, 执行时自动替换");
    layout->addWidget(linkBtn);

    connect(linkBtn, &QPushButton::clicked, this, [this, lineEdit, linkBtn]() {
        QMenu menu(this);
        bool hasItems = false;
        for (auto it = m_linkableData.begin(); it != m_linkableData.end(); ++it) {
            QMenu* sub = menu.addMenu(it.key());
            for (const QString& key : it.value()) {
                const QString ref = QString("$(%1.%2)").arg(it.key(), key);
                sub->addAction(ref, this, [lineEdit, ref]() {
                    lineEdit->insert(ref);
                });
                hasItems = true;
            }
        }
        if (!hasItems)
            menu.addAction("暂无可用数据 (先执行一次流程)")->setEnabled(false);
        menu.exec(linkBtn->mapToGlobal(QPoint(0, linkBtn->height())));
    });

    return container;
}

QWidget* PropertyDialog::createPathEditor(const QString& name, const QString& value,
                                           const QString& filter, bool isDir) {
    auto* container = new QWidget(this);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* lineEdit = new QLineEdit(value, container);
    lineEdit->setObjectName("pathEdit_" + name);
    layout->addWidget(lineEdit, 1);

    auto* browseBtn = new QPushButton("...", container);
    browseBtn->setMaximumWidth(30);
    browseBtn->setToolTip("浏览...");
    layout->addWidget(browseBtn);

    connect(browseBtn, &QPushButton::clicked, this, [this, lineEdit, filter, isDir]() {
        QString currentPath = lineEdit->text();
        QString newPath;
        if (isDir) {
            newPath = QFileDialog::getExistingDirectory(this, "选择目录",
                currentPath.isEmpty() ? "." : currentPath);
        } else {
            newPath = QFileDialog::getOpenFileName(this, "选择文件",
                currentPath.isEmpty() ? "." : currentPath, filter);
        }
        if (!newPath.isEmpty())
            lineEdit->setText(newPath);
    });

    return container;
}

void PropertyDialog::accept() {
    // 页签1: 基本设置
    const QString newName = m_nameEdit->text().trimmed();
    if (!newName.isEmpty())
        m_tool->setInstanceName(newName);
    m_tool->setComment(m_commentEdit->toPlainText());
    m_tool->setActive(m_activeCheck->isChecked());

    // 页签2: 参数设置
    PropertyDefList defs = m_tool->propertyDefs();
    for (const auto& def : defs) {
        QWidget* editor = m_editors.value(def.name);
        if (!editor) continue;

        switch (def.type) {
        case PropertyType::Int:
            m_tool->setProperty(def.name, qobject_cast<QSpinBox*>(editor)->value());
            break;
        case PropertyType::Double:
            m_tool->setProperty(def.name, qobject_cast<QDoubleSpinBox*>(editor)->value());
            break;
        case PropertyType::Boolean:
            m_tool->setProperty(def.name, qobject_cast<QCheckBox*>(editor)->isChecked());
            break;
        case PropertyType::Enum:
            m_tool->setProperty(def.name, qobject_cast<QComboBox*>(editor)->currentIndex());
            break;
        case PropertyType::String:
        default: {
            auto* container = qobject_cast<QWidget*>(editor);
            if (container) {
                auto* lineEdit = container->findChild<QLineEdit*>();
                if (lineEdit) {
                    m_tool->setProperty(def.name, lineEdit->text());
                    break;
                }
            }
            auto* line = qobject_cast<QLineEdit*>(editor);
            if (line)
                m_tool->setProperty(def.name, line->text());
            break;
        }
        }
    }

    // 页签3: 数据判定
    if (m_judgeTable) {
        QList<ResultJudgment> judges;
        for (int r = 0; r < m_judgeTable->rowCount(); ++r) {
            ResultJudgment j;
            j.resultKey = m_judgeTable->item(r, 0)->text();
            j.enabled = m_judgeTable->item(r, 1)->checkState() == Qt::Checked;
            if (!j.enabled) continue;
            const QString lo = m_judgeTable->item(r, 2)->text().trimmed();
            const QString hi = m_judgeTable->item(r, 3)->text().trimmed();
            j.lower = lo.isEmpty() ? -1e18 : lo.toDouble();
            j.upper = hi.isEmpty() ? 1e18 : hi.toDouble();
            if (!j.resultKey.isEmpty())
                judges.append(j);
        }
        m_tool->setJudgments(judges);
    }

    QDialog::accept();
}

} // namespace VisionInspector
