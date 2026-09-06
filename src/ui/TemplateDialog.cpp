/** @file TemplateDialog.cpp - 模板库画廊实现 */
#include "TemplateDialog.h"
#include "IconHelper.h"
#include "../core/ProjectManager.h"
#include "../core/DeviceTemplates.h"
#include "../engine/FlowEngine.h"
#include "../utils/Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace VisionInspector {

TemplateDialog::TemplateDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("模板库 - 从模板新建工程"));
    setMinimumSize(520, 420);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        QStringLiteral("选择一个设备模板快速建工程；调好的工程可通过[文件→保存为模板]存入模板库。"), this));

    m_list = new QListWidget(this);
    m_list->setIconSize(QSize(24, 24));
    layout->addWidget(m_list, 1);

    m_detail = new QLabel(this);
    m_detail->setWordWrap(true);
    m_detail->setStyleSheet("color: #9aa0a6; padding: 6px; background: #1e2a3a; border-radius: 3px;");
    layout->addWidget(m_detail);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    loadEntries();
    connect(m_list, &QListWidget::itemSelectionChanged, this, &TemplateDialog::updateDetail);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { accept(); });
    if (m_list->count() > 0) m_list->setCurrentRow(0);
}

void TemplateDialog::loadEntries() {
    m_entries.clear();
    m_list->clear();

    // 内置设备模板 (代码内建工序, 无文件; path = "builtin:<id>")
    for (const DeviceTemplate& t : DeviceTemplates::all()) {
        Entry e;
        e.title = t.name + QStringLiteral("（内置）");
        e.note = t.note;
        e.flowCount = 1;
        e.toolCount = t.tools.size();
        e.path = QStringLiteral("builtin:") + t.id;
        m_entries.append(e);
        auto* item = new QListWidgetItem(IconHelper::categoryIcon(ToolCategory::Special, 24),
                                         e.title, m_list);
        item->setData(Qt::UserRole, m_entries.size() - 1);
    }

    // 用户自定义模板: templates/*.vipj
    QDir dir(ProjectManager::templateDir());
    const QStringList files = dir.entryList({ QStringLiteral("*.vipj") }, QDir::Files, QDir::Name);
    for (const QString& f : files) {
        const QString path = dir.filePath(f);
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) continue;
        const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
        file.close();

        Entry e;
        e.path = path;
        e.title = json.value("projectName").toString(QFileInfo(path).completeBaseName());
        e.note = json.value("projectNote").toString();
        e.savedAt = json.value("savedAt").toString();
        const QJsonArray flows = json.value("flows").toArray();
        e.flowCount = flows.size();
        for (const QJsonValue& fv : flows)
            e.toolCount += fv.toObject().value("tools").toArray().size();
        if (e.toolCount == 0) continue; // 空模板不展示
        m_entries.append(e);
        auto* item = new QListWidgetItem(IconHelper::categoryIcon(ToolCategory::ImageProcess, 24), e.title, m_list);
        item->setData(Qt::UserRole, m_entries.size() - 1);
    }
}

void TemplateDialog::updateDetail() {
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_entries.size()) { m_detail->clear(); return; }
    const Entry& e = m_entries[row];
    QString text = QStringLiteral("%1\n流程数：%2    工具数：%3")
                       .arg(e.note.isEmpty() ? QStringLiteral("（无说明）") : e.note)
                       .arg(e.flowCount).arg(e.toolCount);
    if (!e.savedAt.isEmpty())
        text += QStringLiteral("    保存时间：%1").arg(e.savedAt);
    m_detail->setText(text);
}

TemplateDialog::Entry TemplateDialog::selected() const {
    const int row = m_list->currentRow();
    if (row < 0 || row >= m_entries.size()) return {};
    return m_entries[row];
}

} // namespace VisionInspector
