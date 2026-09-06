/** @file TemplateDialog.h - 模板库画廊 (从模板新建, 阶段6设备模板系统)
 *
 *  列出 内置模板 + templates/ 目录下的用户自定义模板 (*.vipj),
 *  选中显示流程/工具数/保存时间, 双击或确定返回所选条目。
 */
#pragma once
#include <QDialog>
#include <QVector>
class QListWidget;
class QListWidgetItem;
class QLabel;

namespace VisionInspector {

class TemplateDialog : public QDialog {
    Q_OBJECT
public:
    struct Entry {
        QString title;       // 显示名
        QString path;        // 模板文件路径; 空 = 内置筛选机模板
        QString note;        // 说明 (工程备注或内置描述)
        int toolCount = 0;   // 流程内工具总数
        int flowCount = 0;
        QString savedAt;     // 保存时间
    };

    explicit TemplateDialog(QWidget* parent = nullptr);

    Entry selected() const;

private:
    void loadEntries();
    void updateDetail();

    QListWidget* m_list = nullptr;
    QLabel* m_detail = nullptr;
    QVector<Entry> m_entries;
};

} // namespace VisionInspector
