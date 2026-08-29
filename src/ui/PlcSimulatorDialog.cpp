/**
 * @file PlcSimulatorDialog.cpp
 * @brief PLC模拟器对话框实现
 */

#include "PlcSimulatorDialog.h"
#include "../utils/Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QGroupBox>
#include <QFormLayout>
#include <QTime>

namespace VisionInspector {

PlcSimulatorDialog* PlcSimulatorDialog::s_instance = nullptr;

PlcSimulatorDialog* PlcSimulatorDialog::instance(QWidget* parent) {
    if (!s_instance)
        s_instance = new PlcSimulatorDialog(parent);
    return s_instance;
}

bool PlcSimulatorDialog::simulatorRunning() {
    return s_instance && s_instance->m_slave && s_instance->m_slave->isRunning();
}

PlcSimulatorDialog::PlcSimulatorDialog(QWidget* parent)
    : QDialog(parent)
    , m_slave(new ModbusTcpSlave(this))
{
    setupUi();

    connect(m_slave, &ModbusTcpSlave::registerWritten,
            this, &PlcSimulatorDialog::onRegisterWritten);
    connect(m_slave, &ModbusTcpSlave::coilWritten,
            this, &PlcSimulatorDialog::onCoilWritten);
    connect(m_slave, &ModbusTcpSlave::clientConnected, this, [this](const QString& peer) {
        logLine(QString("客户端接入: %1").arg(peer));
    });

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(500);
    connect(m_refreshTimer, &QTimer::timeout, this, &PlcSimulatorDialog::onRefresh);
    m_refreshTimer->start();
}

void PlcSimulatorDialog::setupUi() {
    setWindowTitle(QStringLiteral("PLC 模拟器 (Modbus TCP 从站)"));
    resize(560, 560);

    auto* mainLayout = new QVBoxLayout(this);

    // 连接设置区
    auto* connGroup = new QGroupBox(QStringLiteral("服务设置"), this);
    auto* form = new QFormLayout(connGroup);
    m_addrEdit = new QLineEdit(QStringLiteral("127.0.0.1"), connGroup);
    m_portEdit = new QLineEdit(QStringLiteral("502"), connGroup);
    form->addRow(QStringLiteral("监听地址:"), m_addrEdit);
    form->addRow(QStringLiteral("端口:"), m_portEdit);

    auto* row = new QHBoxLayout();
    m_startBtn = new QPushButton(QStringLiteral("启动模拟器"), connGroup);
    connect(m_startBtn, &QPushButton::clicked, this, &PlcSimulatorDialog::onStartStop);
    m_stateLabel = new QLabel(QStringLiteral("已停止"), connGroup);
    row->addWidget(m_startBtn);
    row->addWidget(m_stateLabel, 1);
    form->addRow(row);

    m_autoIncCheck = new QCheckBox(QStringLiteral("R0 自动自增 (模拟产量计数)"), connGroup);
    form->addRow(m_autoIncCheck);

    mainLayout->addWidget(connGroup);

    // 寄存器表
    m_regTable = new QTableWidget(24, 3, this);
    m_regTable->setHorizontalHeaderLabels({QStringLiteral("地址"), QStringLiteral("值(十进制)"),
                                           QStringLiteral("值(十六进制)")});
    m_regTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_regTable->setVerticalHeaderLabels({});
    for (int i = 0; i < m_regTable->rowCount(); ++i) {
        auto* addrItem = new QTableWidgetItem(QString("R%1").arg(i));
        addrItem->setFlags(addrItem->flags() & ~Qt::ItemIsEditable);
        m_regTable->setItem(i, 0, addrItem);
        m_regTable->setItem(i, 1, new QTableWidgetItem("0"));
        auto* hexItem = new QTableWidgetItem("0x0000");
        hexItem->setFlags(hexItem->flags() & ~Qt::ItemIsEditable);
        m_regTable->setItem(i, 2, hexItem);
    }
    connect(m_regTable, &QTableWidget::cellChanged,
            this, &PlcSimulatorDialog::onRegisterChanged);
    mainLayout->addWidget(new QLabel(QStringLiteral("保持寄存器 (双击'值'列可改写):"), this));
    mainLayout->addWidget(m_regTable, 2);

    // 写入日志
    mainLayout->addWidget(new QLabel(QStringLiteral("动作记录:"), this));
    m_logList = new QListWidget(this);
    mainLayout->addWidget(m_logList, 1);
}

void PlcSimulatorDialog::onStartStop() {
    if (m_slave->isRunning()) {
        m_slave->stop();
        m_stateLabel->setText(QStringLiteral("已停止"));
        m_startBtn->setText(QStringLiteral("启动模拟器"));
        logLine(QStringLiteral("模拟器已停止"));
        return;
    }

    QString err;
    const QString addr = m_addrEdit->text().trimmed();
    const quint16 port = quint16(m_portEdit->text().toUShort());
    if (!m_slave->start(addr, port, &err)) {
        m_stateLabel->setText(QStringLiteral("启动失败"));
        logLine(err);
        return;
    }
    m_slave->setAutoIncrement(m_autoIncCheck->isChecked() ? 0 : -1);
    m_stateLabel->setText(QStringLiteral("运行中 %1:%2").arg(addr).arg(port));
    m_startBtn->setText(QStringLiteral("停止模拟器"));
    logLine(QString("模拟器已启动: %1:%2 (从站ID=1)").arg(addr).arg(port));
}

void PlcSimulatorDialog::onRefresh() {
    if (!m_slave->isRunning()) return;
    m_updatingTable = true;
    for (int i = 0; i < m_regTable->rowCount(); ++i) {
        const quint16 v = m_slave->holding(i);
        m_regTable->item(i, 1)->setText(QString::number(v));
        m_regTable->item(i, 2)->setText(QString("0x%1").arg(v, 4, 16, QChar('0')));
    }
    m_stateLabel->setText(m_stateLabel->text().split(" | ").first() +
                          QString(" | 连接数: %1").arg(m_slave->connectionCount()));
    m_updatingTable = false;
}

void PlcSimulatorDialog::onRegisterChanged(int row, int column) {
    if (m_updatingTable || column != 1 || !m_slave->isRunning()) return;
    const int value = m_regTable->item(row, 1)->text().toInt();
    m_slave->setHolding(row, quint16(value));
    logLine(QString("手动写入 R%1 = %2").arg(row).arg(value));
}

void PlcSimulatorDialog::onRegisterWritten(int addr, quint16 value) {
    logLine(QString("主软件写入 R%1 = %2").arg(addr).arg(value));
}

void PlcSimulatorDialog::onCoilWritten(int addr, bool on) {
    logLine(QString("主软件写入 C%1 = %2").arg(addr).arg(on ? "ON" : "OFF"));
}

void PlcSimulatorDialog::logLine(const QString& s) {
    m_logList->insertItem(0, QString("[%1] %2")
        .arg(QTime::currentTime().toString("hh:mm:ss"), s));
    if (m_logList->count() > 200)
        delete m_logList->takeItem(m_logList->count() - 1);
}

} // namespace VisionInspector
