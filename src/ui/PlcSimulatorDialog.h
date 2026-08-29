/**
 * @file PlcSimulatorDialog.h
 * @brief PLC模拟器对话框 — 无真实PLC时联调Modbus工具
 *
 * 提供 Modbus TCP 从站 (自研 ModbusTcpSlave):
 *   - 启停服务, 查看寄存器实时值, 手动改写
 *   - 可选自动行为 (产量自增计数)
 *   - 实时记录主软件的写入动作 (如 OK/NG 信号)
 */
#pragma once

#include "../hal/ModbusTcpSlave.h"
#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include <QTimer>

namespace VisionInspector {

class PlcSimulatorDialog : public QDialog {
    Q_OBJECT

public:
    static PlcSimulatorDialog* instance(QWidget* parent = nullptr);

    /** 模拟器是否在运行 (供状态栏显示) */
    static bool simulatorRunning();

private slots:
    void onStartStop();
    void onRefresh();
    void onRegisterChanged(int row, int column);
    void onRegisterWritten(int addr, quint16 value);
    void onCoilWritten(int addr, bool on);

private:
    explicit PlcSimulatorDialog(QWidget* parent = nullptr);
    void setupUi();
    void logLine(const QString& s);

    static PlcSimulatorDialog* s_instance;

    ModbusTcpSlave* m_slave = nullptr;
    QLineEdit* m_addrEdit = nullptr;
    QLineEdit* m_portEdit = nullptr;
    QPushButton* m_startBtn = nullptr;
    QLabel* m_stateLabel = nullptr;
    QCheckBox* m_autoIncCheck = nullptr;
    QTableWidget* m_regTable = nullptr;
    QListWidget* m_logList = nullptr;
    QTimer* m_refreshTimer = nullptr;
    bool m_updatingTable = false;
};

} // namespace VisionInspector
