#include "safetylockdialog.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QLabel *label(const QString &text, const char *name = nullptr)
{
    auto *result = new QLabel(text);
    if (name)
        result->setObjectName(name);
    result->setWordWrap(true);
    return result;
}

QWidget *deviceState(const QString &name, const QString &value, const QString &tone)
{
    auto *row = new QFrame;
    row->setObjectName("deviceStateRow");
    row->setProperty("tone", tone);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->addWidget(label(name, "mutedText"));
    layout->addStretch();
    layout->addWidget(label(value, tone == "danger" ? "dangerText" : "okText"));
    return row;
}

} // namespace

SafetyLockDialog::SafetyLockDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName("safetyLockDialog");
    setWindowTitle("设备异常 / 流程锁定");
    setModal(true);
    resize(760, 520);
    setMinimumSize(680, 480);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 20);
    layout->setSpacing(13);

    layout->addWidget(label("流程已安全锁定", "dangerTitle"));
    layout->addWidget(label("INTERLOCK-DOOR-021  ·  2026-07-31 11:48:26", "dangerCode"));

    auto *callout = new QFrame;
    callout->setObjectName("dangerCallout");
    auto *calloutLayout = new QVBoxLayout(callout);
    calloutLayout->setContentsMargins(14, 12, 14, 12);
    calloutLayout->setSpacing(4);
    calloutLayout->addWidget(label("机房门控联锁断开", "sectionTitle"));
    calloutLayout->addWidget(label("系统已停止高压、禁止机架运动并冻结采集参数。当前患者、定位范围和已采集数据均已保留。", "mutedText"));
    layout->addWidget(callout);

    auto *states = new QGridLayout;
    states->setSpacing(7);
    states->addWidget(deviceState("机房门控", "断开", "danger"), 0, 0);
    states->addWidget(deviceState("高压发生器", "已禁止", "normal"), 0, 1);
    states->addWidget(deviceState("机架运动", "已停止", "normal"), 1, 0);
    states->addWidget(deviceState("急停回路", "正常", "normal"), 1, 1);
    layout->addLayout(states);

    layout->addWidget(label("恢复步骤", "sectionTitle"));
    layout->addWidget(label("01  确认患者与机房内人员安全", "recoveryStep"));
    layout->addWidget(label("02  关闭机房门并检查门控指示", "recoveryStep"));
    layout->addWidget(label("03  确认设备面板无其他报警，再执行联锁复核", "recoveryStep"));
    layout->addStretch();

    auto *footer = new QHBoxLayout;
    footer->addWidget(label("报警、设备状态与恢复操作将写入审计日志。", "mutedText"));
    footer->addStretch();
    auto *log = new QPushButton("查看事件日志");
    log->setObjectName("secondaryButton");
    connect(log, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, "联锁事件日志",
                                 "11:48:26.104  DOOR_INTERLOCK_OPEN\n"
                                 "11:48:26.112  XRAY_PERMISSION_REVOKED\n"
                                 "11:48:26.118  GANTRY_MOTION_STOPPED\n"
                                 "11:48:26.126  WORKFLOW_LOCKED\n\n"
                                 "Operator: TECH_023 · Study: ACC-2026-0725-003");
    });
    footer->addWidget(log);
    auto *recover = new QPushButton("复核联锁并恢复");
    recover->setObjectName("primaryButton");
    recover->setMinimumWidth(180);
    connect(recover, &QPushButton::clicked, this, &QDialog::accept);
    footer->addWidget(recover);
    layout->addLayout(footer);
}
