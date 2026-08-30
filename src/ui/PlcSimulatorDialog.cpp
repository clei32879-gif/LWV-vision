/**
 * @file PlcSimulatorDialog.cpp
 * @brief PLC模拟器对话框实现 (TCP/RTU 双通道)
 */

#include "PlcSimulatorDialog.h"
#include "../utils/Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QGroupBox>
#include <QFormLayout>
#include <QTime>
#include <QMessageBox>

namespace VisionInspector {

PlcSimulatorDialog* PlcSimulatorDialog::s_instance = nullptr;

PlcSimulatorDialog* PlcSimulatorDialog::instance(QWidget* parent) {
    if (!s_instance)
        s_instance = new PlcSimulatorDialog(parent);
    return s_instance;
}

bool PlcSimulatorDialog::simulatorRunning() {
    if (!s_instance) return false;
    if (s_instance->m_tcpSlave && s_instance->m_tcpSlave->isRunning()) return true;
    if (s_instance->m_rtuSlave && s_instance->m_rtuSlave->isRunning()) return true;
    return false;
}

PlcSimulatorDialog::PlcSimulatorDialog(QWidget* parent)
    : QDialog(parent)
    , m_tcpSlave(new ModbusTcpSlave(this))
    , m_rtuSlave(new ModbusRtuSlave(this))
{
    setupUi();
    applyPreset();   // 启动即显示转盘筛选机契约预设表

    connect(m_tcpSlave, &ModbusTcpSlave::registerWritten,
            this, &PlcSimulatorDialog::onRegisterWritten);
    connect(m_tcpSlave, &ModbusTcpSlave::coilWritten,
            this, &PlcSimulatorDialog::onCoilWritten);
    connect(m_rtuSlave, &ModbusRtuSlave::registerWritten,
            this, &PlcSimulatorDialog::onRegisterWritten);
    connect(m_rtuSlave, &ModbusRtuSlave::coilWritten,
            this, &PlcSimulatorDialog::onCoilWritten);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(500);
    connect(m_refreshTimer, &QTimer::timeout, this, &PlcSimulatorDialog::onRefresh);
    m_refreshTimer->start();
}

void PlcSimulatorDialog::setupUi() {
    setWindowTitle(QStringLiteral("PLC 模拟器 (Modbus TCP/RTU 从站)"));
    resize(620, 600);

    auto* mainLayout = new QVBoxLayout(this);

    // 连接设置区
    auto* connGroup = new QGroupBox(QStringLiteral("服务设置"), this);
    auto* form = new QFormLayout(connGroup);

    m_modeCombo = new QComboBox(connGroup);
    m_modeCombo->addItems({QStringLiteral("Modbus TCP (网络)"), QStringLiteral("Modbus RTU (串口)")});
    form->addRow(QStringLiteral("通讯方式:"), m_modeCombo);

    m_addrEdit = new QLineEdit(QStringLiteral("127.0.0.1"), connGroup);
    m_portEdit = new QLineEdit(QStringLiteral("502"), connGroup);
    form->addRow(QStringLiteral("监听地址:"), m_addrEdit);
    form->addRow(QStringLiteral("端口:"), m_portEdit);

    m_serialEdit = new QLineEdit(QStringLiteral("COM1"), connGroup);
    m_baudEdit = new QLineEdit(QStringLiteral("9600"), connGroup);
    form->addRow(QStringLiteral("串口号:"), m_serialEdit);
    form->addRow(QStringLiteral("波特率:"), m_baudEdit);

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

    // 寄存器表: 地址(数) | 名称 | 值(十进制) | 值(十六进制)
    m_regTable = new QTableWidget(24, 4, this);
    m_regTable->setHorizontalHeaderLabels({QStringLiteral("地址"),
                                           QStringLiteral("名称"),
                                           QStringLiteral("值(十进制)"),
                                           QStringLiteral("值(十六进制)")});
    m_regTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_regTable->setVerticalHeaderLabels({});
    for (int i = 0; i < m_regTable->rowCount(); ++i) {
        auto* addrItem = new QTableWidgetItem(QString::number(i));
        addrItem->setFlags(addrItem->flags() & ~Qt::ItemIsEditable);
        m_regTable->setItem(i, 0, addrItem);
        m_regTable->setItem(i, 1, new QTableWidgetItem(QStringLiteral("R%1").arg(i)));
        m_regTable->setItem(i, 2, new QTableWidgetItem("0"));
        auto* hexItem = new QTableWidgetItem("0x0000");
        hexItem->setFlags(hexItem->flags() & ~Qt::ItemIsEditable);
        m_regTable->setItem(i, 3, hexItem);
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

/** 应用转盘筛选机契约预设 — 表格显示契约地址与含义 (保持寄存器=41088+D号) */
void PlcSimulatorDialog::applyPreset() {
    m_regTable->setRowCount(0);
    struct Entry { int addr; QString label; };
    const QVector<Entry> entries = {
        {41088, "HD0 手动速度"},   {41094, "D6/HD6 上位机命令/无料报警延时"},
        {41096, "HD8 自动速度"},   {41102, "HD14 NG吹气时间"},
        {41106, "HD18 OK吹气时间"}, {41198, "HD110 停止延时"},
        {41210, "HD122 良率"},     {41258, "HD170 OK产量"},
        {41260, "HD172 NG产量"},   {41262, "HD174 总产量"},
        {41264, "HD176 重测"},     {41284, "D196 产品UPH"},
        {41288, "HD200 相机1位置"},{41290, "HD202 相机2位置"},
        {41292, "HD204 相机3位置"},{41294, "HD206 相机4位置"},
        {41296, "HD208 相机5位置"},{41298, "HD210 相机6位置"},
        {41300, "HD212 相机7位置"},{41302, "HD214 相机8位置"},
        {41488, "HD400 OK剔除位置"},{41490, "HD402 NG剔除位置"},
    };
    m_regTable->setRowCount(entries.size());
    for (int i = 0; i < entries.size(); ++i) {
        auto* addrItem = new QTableWidgetItem(QString::number(entries[i].addr));
        addrItem->setFlags(addrItem->flags() & ~Qt::ItemIsEditable);
        m_regTable->setItem(i, 0, addrItem);
        auto* nameItem = new QTableWidgetItem(entries[i].label);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m_regTable->setItem(i, 1, nameItem);
        m_regTable->setItem(i, 2, new QTableWidgetItem("0"));
        auto* hexItem = new QTableWidgetItem("0x0000");
        hexItem->setFlags(hexItem->flags() & ~Qt::ItemIsEditable);
        m_regTable->setItem(i, 3, hexItem);
    }
}

void PlcSimulatorDialog::onStartStop() {
    const bool running = m_tcpSlave->isRunning() || m_rtuSlave->isRunning();
    if (running) {
        m_tcpSlave->stop();
        m_rtuSlave->stop();
        m_stateLabel->setText(QStringLiteral("已停止"));
        m_startBtn->setText(QStringLiteral("启动模拟器"));
        logLine(QStringLiteral("模拟器已停止"));
        return;
    }

    QString err;
    const int mode = m_modeCombo->currentIndex();
    if (mode == 0) {
        const QString addr = m_addrEdit->text().trimmed();
        const quint16 port = quint16(m_portEdit->text().toUShort());
        if (!m_tcpSlave->start(addr, port, &err)) {
            m_stateLabel->setText(QStringLiteral("启动失败"));
            logLine(err);
            return;
        }
        m_tcpSlave->setAutoIncrement(m_autoIncCheck->isChecked() ? 0 : -1);
        m_stateLabel->setText(QStringLiteral("运行中 %1:%2").arg(addr).arg(port));
        logLine(QString("TCP 模拟器已启动: %1:%2 (从站ID=1)").arg(addr).arg(port));
    } else {
        const QString portName = m_serialEdit->text().trimmed();
        const int baud = m_baudEdit->text().toInt();
        if (!m_rtuSlave->start(portName, baud, 8, 1, QStringLiteral("无"), &err)) {
            m_stateLabel->setText(QStringLiteral("启动失败"));
            logLine(err);
            return;
        }
        m_rtuSlave->setAutoIncrement(m_autoIncCheck->isChecked() ? 0 : -1);
        m_stateLabel->setText(QStringLiteral("运行中 %1@%2").arg(portName).arg(baud));
        logLine(QString("RTU 模拟器已启动: %1 @ %2,8,N,1 (从站ID=1)").arg(portName).arg(baud));
    }
}

void PlcSimulatorDialog::onRefresh() {
    if (!m_tcpSlave->isRunning() && !m_rtuSlave->isRunning()) return;
    m_updatingTable = true;
    for (int i = 0; i < m_regTable->rowCount(); ++i) {
        const int addr = m_regTable->item(i, 0)->text().toInt();
        quint16 v = 0;
        if (m_tcpSlave->isRunning()) v = m_tcpSlave->holding(addr);
        else if (m_rtuSlave->isRunning()) v = m_rtuSlave->holding(addr);
        m_regTable->item(i, 2)->setText(QString::number(v));
        m_regTable->item(i, 3)->setText(QString("0x%1").arg(v, 4, 16, QChar('0')));
    }
    m_updatingTable = false;
}

void PlcSimulatorDialog::onRegisterChanged(int row, int column) {
    if (m_updatingTable || column != 2) return;
    const int addr = m_regTable->item(row, 0)->text().toInt();
    const int value = m_regTable->item(row, 2)->text().toInt();
    if (m_tcpSlave->isRunning()) m_tcpSlave->setHolding(addr, quint16(value));
    else if (m_rtuSlave->isRunning()) m_rtuSlave->setHolding(addr, quint16(value));
    logLine(QString("手动写入 R%1 = %2").arg(addr).arg(value));
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
