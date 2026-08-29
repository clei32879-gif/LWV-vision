/** @file StatusPanel.h - 状态指示面板（参考华晨智能） */
#pragma once
#include <QWidget>
#include <QLabel>
#include <QGridLayout>
#include <QVector>

namespace VisionInspector {

/**
 * 状态指示器 - 显示单个工位/相机的 OK/NG/PAUSE 状态
 */
class StatusIndicator : public QWidget {
    Q_OBJECT
public:
    enum State { Idle, OK, NG, Pause, Error };
    explicit StatusIndicator(const QString& name, QWidget* parent = nullptr);
    void setState(State state);
    State state() const { return m_state; }
    QString name() const { return m_name; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QString m_name;
    State m_state = Idle;
};

/**
 * 状态面板 - 显示所有工位/相机的状态概览
 */
class StatusPanel : public QWidget {
    Q_OBJECT
public:
    explicit StatusPanel(QWidget* parent = nullptr);

    /** 设置工位数量 */
    void setStationCount(int count);

    /** 设置某个工位的状态 */
    void setStationState(int index, StatusIndicator::State state);

    /** 获取某个工位的状态 */
    StatusIndicator::State stationState(int index) const;

    /** 获取OK/NG计数 */
    int okCount() const;
    int ngCount() const;

    /** 清零计数 */
    void resetCounts();

signals:
    void stationClicked(int index);

private:
    QGridLayout* m_gridLayout;
    QVector<StatusIndicator*> m_indicators;
    QLabel* m_summaryLabel;
    int m_okCount = 0;
    int m_ngCount = 0;
};

} // namespace VisionInspector
