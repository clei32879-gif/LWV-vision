/**
 * @file SettingsDialogs.cpp
 * @brief 系统设置 / 项目设置 / 全局变量 对话框实现
 */

#include "SettingsDialogs.h"
#include "../core/GlobalVariables.h"
#include "../utils/Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QHeaderView>
#include <QSettings>
#include <QFileDialog>
#include <QLabel>

namespace VisionInspector {

// ============================================================
// 系统设置
// ============================================================
SystemSettingsDialog::SystemSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("系统设置"));
    auto* form = new QFormLayout(this);

    m_language = new QComboBox(this);
    m_language->addItem(QStringLiteral("简体中文"));
    m_language->addItem(QStringLiteral("English (敬请期待)"));
    form->addRow(QStringLiteral("界面语言:"), m_language);

    m_logLevel = new QComboBox(this);
    m_logLevel->addItem(QStringLiteral("调试"), int(LogLevel::Debug));
    m_logLevel->addItem(QStringLiteral("信息"), int(LogLevel::Info));
    m_logLevel->addItem(QStringLiteral("警告"), int(LogLevel::Warn));
    m_logLevel->addItem(QStringLiteral("错误"), int(LogLevel::Error));
    form->addRow(QStringLiteral("日志级别:"), m_logLevel);

    auto* dirRow = new QHBoxLayout();
    m_imageDir = new QLineEdit(this);
    QPushButton* browse = new QPushButton(QStringLiteral("浏览..."), this);
    dirRow->addWidget(m_imageDir);
    dirRow->addWidget(browse);
    form->addRow(QStringLiteral("图像保存目录:"), dirRow);
    connect(browse, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择图像保存目录"),
                                                        m_imageDir->text());
        if (!dir.isEmpty()) m_imageDir->setText(dir);
    });

    // 读取当前配置
    QSettings s("VisionInspector", "VisionInspector");
    m_imageDir->setText(s.value("imageSaveDir",
        QCoreApplication::applicationDirPath() + "/images").toString());
    const int level = s.value("logLevel", int(LogLevel::Info)).toInt();
    m_logLevel->setCurrentIndex(qMax(0, m_logLevel->findData(level)));

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &SystemSettingsDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SystemSettingsDialog::onAccept() {
    QSettings s("VisionInspector", "VisionInspector");
    s.setValue("imageSaveDir", m_imageDir->text());
    s.setValue("logLevel", m_logLevel->currentData().toInt());
    Logger::instance().setLevel(LogLevel(m_logLevel->currentData().toInt()));
    accept();
}

// ============================================================
// 项目设置
// ============================================================
ProjectSettingsDialog::ProjectSettingsDialog(const QString& name, const QString& note,
                                             QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("项目设置"));
    auto* form = new QFormLayout(this);

    m_name = new QLineEdit(name, this);
    form->addRow(QStringLiteral("项目名称:"), m_name);

    m_note = new QPlainTextEdit(note, this);
    m_note->setFixedHeight(120);
    form->addRow(QStringLiteral("项目备注:"), m_note);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

// ============================================================
// 全局变量管理
// ============================================================
GlobalVariablesDialog::GlobalVariablesDialog(GlobalVariables* globals, QWidget* parent)
    : QDialog(parent)
    , m_globals(globals)
{
    setWindowTitle(QStringLiteral("全局变量"));
    resize(420, 380);
    auto* layout = new QVBoxLayout(this);

    m_table = new QTableWidget(0, 2, this);
    m_table->setHorizontalHeaderLabels({QStringLiteral("变量名"), QStringLiteral("值")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(m_table);

    auto* buttons = new QHBoxLayout();
    QPushButton* add = new QPushButton(QStringLiteral("添加"), this);
    QPushButton* remove = new QPushButton(QStringLiteral("删除选中"), this);
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch();
    layout->addLayout(buttons);

    connect(add, &QPushButton::clicked, this, &GlobalVariablesDialog::onAdd);
    connect(remove, &QPushButton::clicked, this, &GlobalVariablesDialog::onRemove);
    connect(m_table, &QTableWidget::cellChanged,
            this, &GlobalVariablesDialog::onCellChanged);

    refresh();
}

void GlobalVariablesDialog::refresh() {
    m_updating = true;
    const QMap<QString, QVariant> all = m_globals->all();
    m_table->setRowCount(all.size());
    int row = 0;
    for (auto it = all.begin(); it != all.end(); ++it, ++row) {
        m_table->setItem(row, 0, new QTableWidgetItem(it.key()));
        m_table->setItem(row, 1, new QTableWidgetItem(it.value().toString()));
    }
    m_updating = false;
}

void GlobalVariablesDialog::onAdd() {
    // 自动命名 newVar / newVar1 / newVar2 ...
    QString base = "newVar";
    QString name = base;
    int idx = 0;
    while (m_globals->has(name)) name = base + QString::number(++idx);
    m_globals->set(name, 0);
    refresh();
}

void GlobalVariablesDialog::onRemove() {
    const int row = m_table->currentRow();
    if (row < 0) return;
    m_globals->remove(m_table->item(row, 0)->text());
    refresh();
}

void GlobalVariablesDialog::onCellChanged(int row, int column) {
    if (m_updating || row < 0 || column < 0) return;
    const QString oldName = m_table->item(row, 0)->text();
    const QString newName = m_table->item(row, 0)->text().trimmed();
    const QString value = m_table->item(row, 1)->text();

    if (column == 0 && newName != oldName) {
        // 重命名: 删旧建新
        m_globals->remove(oldName);
        if (!newName.isEmpty()) m_globals->set(newName, value);
        refresh();
        return;
    }
    if (column == 1) {
        bool numOk = false;
        const double d = value.toDouble(&numOk);
        m_globals->set(oldName, numOk ? QVariant(d) : QVariant(value));
    }
}

} // namespace VisionInspector
