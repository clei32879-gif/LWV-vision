#include "PropertyDialog.h"
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

namespace VisionInspector {

PropertyDialog::PropertyDialog(ITool* tool, const QStringList& availableTools,
                               const QMap<QString, QStringList>& linkableData,
                               QWidget* parent)
    : QDialog(parent), m_tool(tool), m_availableTools(availableTools),
      m_linkableData(linkableData) {
    setWindowTitle(QString("属性编辑 - %1").arg(tool->displayName()));
    setMinimumWidth(480);
    buildUI();
}

void PropertyDialog::buildUI() {
    auto* mainLayout = new QVBoxLayout(this);
    m_formLayout = new QFormLayout();
    mainLayout->addLayout(m_formLayout);

    PropertyDefList defs = m_tool->propertyDefs();

    // 根据工具类型添加"输入图像"下拉框
    bool isCaptureTool = (m_tool->typeName() == "CaptureImage");
    bool needsImage = !isCaptureTool &&
                       m_tool->typeName() != "PositionCorrection" &&
                       m_tool->typeName() != "EndCorrection" &&
                       m_tool->typeName() != "CoordSystem" &&
                       m_tool->typeName() != "CalculateVariable" &&
                       m_tool->typeName() != "SetVariable" &&
                       m_tool->typeName() != "DataJudgment" &&
                       m_tool->typeName() != "Delay" &&
                       m_tool->typeName() != "Loop";

    if (isCaptureTool) {
        // 采集图像：显示相机/文件选项
        auto* combo = new QComboBox(this);
        combo->setMinimumWidth(200);
        combo->addItem("当前图像");
        combo->addItem("相机图像");
        combo->addItem("文件图像");
        QString currentInput = m_tool->propertyValue("inputImage").toString();
        int idx = combo->findText(currentInput);
        if (idx >= 0) combo->setCurrentIndex(idx);
        m_formLayout->addRow("图像来源:", combo);
        m_editors["inputImage"] = combo;
    } else if (needsImage) {
        // 其他需要图像的工具：仅显示流程中的图像处理工具
        auto* combo = new QComboBox(this);
        combo->setMinimumWidth(200);
        combo->addItem("当前图像");
        for (const QString& toolName : m_availableTools) {
            combo->addItem(toolName);
        }
        QString currentInput = m_tool->propertyValue("inputImage").toString();
        int idx = combo->findText(currentInput);
        if (idx >= 0) combo->setCurrentIndex(idx);
        m_formLayout->addRow("输入图像:", combo);
        m_editors["inputImage"] = combo;
    }

    for (const auto& def : defs) {
        QWidget* editor = nullptr;
        QVariant currentVal = m_tool->propertyValue(def.name);

        switch (def.type) {
        case PropertyType::Int: {
            auto* spin = new QSpinBox(this);
            spin->setMinimum(def.minValue.toInt());
            spin->setMaximum(def.maxValue.toInt());
            spin->setValue(currentVal.toInt());
            editor = spin;
            break;
        }
        case PropertyType::Double: {
            auto* dspin = new QDoubleSpinBox(this);
            dspin->setDecimals(3);
            dspin->setMinimum(def.minValue.toDouble());
            dspin->setMaximum(def.maxValue.toDouble());
            dspin->setValue(currentVal.toDouble());
            editor = dspin;
            break;
        }
        case PropertyType::Boolean: {
            auto* check = new QCheckBox(this);
            check->setChecked(currentVal.toBool());
            editor = check;
            break;
        }
        case PropertyType::Enum: {
            auto* combo = new QComboBox(this);
            combo->addItems(def.enumValues);
            combo->setCurrentIndex(currentVal.toInt());
            editor = combo;
            break;
        }
        case PropertyType::String:
        default: {
            // 路径类型的属性 → 带文件浏览
            QString name = def.name.toLower();
            if (name.contains("path") || name.contains("file") || name.contains("dir") || name.contains("image")) {
                bool isDir = name.contains("dir");
                QString filter = isDir ? "所有文件 (*)" :
                    "图像文件 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;所有文件 (*)";
                editor = createPathEditor(def.name, currentVal.toString(), filter, isDir);
            } else {
                // 普通字符串 → 带数据链接按钮
                editor = createLinkEditor(def.name, currentVal.toString());
            }
            break;
        }
        }

        if (editor) {
            m_formLayout->addRow(def.label + ":", editor);
            m_editors[def.name] = editor;
        }
    }

    // 数据判定区 (对齐CKVision: 工具内直接配上下限)
    buildJudgeSection();

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &PropertyDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &PropertyDialog::reject);
    mainLayout->addWidget(m_buttonBox);
}

QWidget* PropertyDialog::createLinkEditor(const QString& name, const QString& value) {
    auto* container = new QWidget(this);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* lineEdit = new QLineEdit(value, this);
    lineEdit->setPlaceholderText("可直接输入, 或点右侧按钮插入数据引用");
    layout->addWidget(lineEdit, 1);

    auto* linkBtn = new QPushButton("...", this);
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
        if (!hasItems) {
            menu.addAction("暂无可用数据 (先执行一次流程)")->setEnabled(false);
        }
        menu.exec(linkBtn->mapToGlobal(QPoint(0, linkBtn->height())));
    });

    return container;
}

void PropertyDialog::buildJudgeSection() {
    // 判定行集合 = 已配置的判定 ∪ 上次运行的结果键
    QList<ResultJudgment> judges = m_tool->judgments();
    QStringList resultKeys = m_tool->resultData().keys();
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
    if (judges.isEmpty()) return;

    auto* group = new QWidget(this);
    auto* vLayout = new QVBoxLayout(group);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->addWidget(new QLabel("数据判定 (勾选启用后, 结果超出上下限即判NG):", group));

    m_judgeTable = new QTableWidget(judges.size(), 4, group);
    m_judgeTable->setHorizontalHeaderLabels({"结果键", "启用", "下限", "上限"});
    m_judgeTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_judgeTable->verticalHeader()->setVisible(false);
    m_updatingJudge = true;

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
    m_updatingJudge = false;
    vLayout->addWidget(m_judgeTable);
    static_cast<QVBoxLayout*>(layout())->addWidget(group);
}

QWidget* PropertyDialog::createPathEditor(const QString& name, const QString& value,
                                           const QString& filter, bool isDir) {
    auto* container = new QWidget(this);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* lineEdit = new QLineEdit(value, this);
    lineEdit->setObjectName("pathEdit_" + name);
    layout->addWidget(lineEdit, 1);

    auto* browseBtn = new QPushButton("...", this);
    browseBtn->setMaximumWidth(30);
    browseBtn->setToolTip("浏览...");
    layout->addWidget(browseBtn);

    // 连接浏览按钮
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

        if (!newPath.isEmpty()) {
            lineEdit->setText(newPath);
        }
    });

    return container;
}

void PropertyDialog::accept() {
    // 保存输入图像选择
    QWidget* inputImgEditor = m_editors.value("inputImage");
    if (inputImgEditor) {
        auto* combo = qobject_cast<QComboBox*>(inputImgEditor);
        if (combo) {
            m_tool->setProperty("inputImage", combo->currentText());
        }
    }

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
            // 处理路径类型（带浏览按钮的）
            auto* container = qobject_cast<QWidget*>(editor);
            if (container) {
                auto* lineEdit = container->findChild<QLineEdit*>();
                if (lineEdit) {
                    m_tool->setProperty(def.name, lineEdit->text());
                    break;
                }
            }
            // 普通文本输入框
            auto* line = qobject_cast<QLineEdit*>(editor);
            if (line) {
                m_tool->setProperty(def.name, line->text());
            }
            break;
        }
        }
    }

    // 保存数据判定
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
