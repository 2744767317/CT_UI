#include "acquisitionpanel.h"

#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QTabWidget>
#include <QTabBar>
#include <QTreeWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

namespace {

QLabel *label(const QString &text, const char *name = nullptr)
{
    auto *result = new QLabel(text);
    if (name)
        result->setObjectName(name);
    return result;
}

QFrame *line()
{
    auto *separator = new QFrame;
    separator->setObjectName("sectionLine");
    separator->setFrameShape(QFrame::HLine);
    return separator;
}

QPushButton *secondary(const QString &text, bool checkable = false)
{
    auto *button = new QPushButton(text);
    button->setObjectName("secondaryButton");
    button->setCheckable(checkable);
    return button;
}

QWidget *stepHeader(const QString &number, const QString &title, const QString &status)
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    auto *badge = label(number, "stepBadge");
    badge->setProperty("active", number == "3");
    layout->addWidget(badge);
    layout->addWidget(label(title, "sectionTitle"));
    layout->addStretch();
    layout->addWidget(label(status, status == "待校验" ? "warningText" : "okText"));
    return row;
}

} // namespace

AcquisitionPanel::AcquisitionPanel(QWidget *parent)
    : QFrame(parent)
{
    setObjectName("sidePanel");
    setMinimumWidth(300);
    setMaximumWidth(390);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 12);
    layout->setSpacing(8);

    layout->addWidget(label("检查与数据", "panelEyebrow"));
    layout->addWidget(label("当前检查", "panelTitle"));

    auto *study = new QFrame;
    study->setObjectName("studySummary");
    auto *studyLayout = new QVBoxLayout(study);
    studyLayout->setContentsMargins(12, 10, 12, 10);
    studyLayout->setSpacing(3);
    auto *patientRow = new QHBoxLayout;
    patientRow->addWidget(label("李明", "patientName"));
    patientRow->addStretch();
    auto *change = secondary("更换");
    change->setMaximumWidth(62);
    patientRow->addWidget(change);
    studyLayout->addLayout(patientRow);
    studyLayout->addWidget(label("P20260725018  ·  男  ·  38 岁", "mutedText"));
    studyLayout->addWidget(label("ACC-2026-0725-003  ·  今日 10:30", "mutedText"));
    layout->addWidget(study);

    m_tabs = new QTabWidget;
    m_tabs->setObjectName("sideTabs");
    m_tabs->addTab(buildPreparationPage(), "检查准备");
    m_tabs->addTab(buildDataPage(), "影像数据");
    layout->addWidget(m_tabs, 1);

    m_timer = new QTimer(this);
    m_timer->setInterval(70);
    connect(m_timer, &QTimer::timeout, this, [this] {
        const int next = qMin(100, m_progress->value() + 2);
        m_progress->setValue(next);
        m_progressLabel->setText(next < 76 ? QString("采集 AP/LAT · %1%").arg(next) :
                                             QString("生成预览体数据 · %1%").arg(next));
        if (next >= 100) {
            m_timer->stop();
            m_preparationStage = 3;
            m_stageLabel->setText("重建预览完成");
            m_stageHint->setText("影像和派生数据已加入当前场景");
            m_progressLabel->setText("CT_RECON_1 已生成 · 512 × 512 × 684");
            m_primaryButton->setText("查看影像与重建结果");
            m_primaryButton->setEnabled(true);
            if (m_workflowChangedCallback)
                m_workflowChangedCallback("查看结果", "预览重建完成，等待图像质量确认");
        }
    });
}

void AcquisitionPanel::setWorkflowChangedCallback(std::function<void(const QString &, const QString &)> callback)
{
    m_workflowChangedCallback = std::move(callback);
}

void AcquisitionPanel::setEditingMode()
{
    m_tabs->setCurrentIndex(1);
    m_tabs->tabBar()->hide();
}

QWidget *AcquisitionPanel::buildPreparationPage()
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(2, 12, 2, 2);
    layout->setSpacing(10);

    auto *stateRow = new QHBoxLayout;
    auto *stateText = new QWidget;
    auto *stateLayout = new QVBoxLayout(stateText);
    stateLayout->setContentsMargins(0, 0, 0, 0);
    stateLayout->setSpacing(1);
    m_stageLabel = label("准备检查", "patientName");
    m_stageHint = label("三项信息在同一页完成", "mutedText");
    stateLayout->addWidget(m_stageLabel);
    stateLayout->addWidget(m_stageHint);
    stateRow->addWidget(stateText);
    stateRow->addStretch();
    stateRow->addWidget(label("1 / 3", "warningText"));
    layout->addLayout(stateRow);
    layout->addWidget(line());

    layout->addWidget(stepHeader("1", "患者确认", "已核对"));
    auto *identity = new QCheckBox("姓名 + 患者 ID 双标识");
    identity->setChecked(true);
    identity->setEnabled(false);
    layout->addWidget(identity);

    layout->addWidget(line());
    layout->addWidget(stepHeader("2", "检查协议", "已匹配"));
    auto *protocol = new QComboBox;
    protocol->addItems({"全脊柱 AP/LAT · 成人低剂量", "全脊柱 AP/LAT · 成人标准", "全脊柱 AP · 单平面"});
    layout->addWidget(protocol);
    auto *protocolMeta = label("83 / 102 kV  ·  预计 DAP 478.8 mGy·cm²", "mutedText");
    protocolMeta->setWordWrap(true);
    layout->addWidget(protocolMeta);

    layout->addWidget(line());
    layout->addWidget(stepHeader("3", "定位与就绪", "待校验"));
    auto *orientation = new QHBoxLayout;
    auto *orientationGroup = new QButtonGroup(content);
    orientationGroup->setExclusive(true);
    auto *front = secondary("面向设备", true);
    front->setChecked(true);
    auto *back = secondary("背向设备", true);
    orientationGroup->addButton(front);
    orientationGroup->addButton(back);
    orientation->addWidget(front);
    orientation->addWidget(back);
    layout->addLayout(orientation);
    layout->addWidget(label("扫描范围", "mutedText"));
    layout->addWidget(label("T1  →  骶骨  ·  166.0 至 66.0 cm", "valueLabel"));
    m_positionStatus = label("定位画面已连接，等待范围校验", "warningText");
    m_positionStatus->setWordWrap(true);
    layout->addWidget(m_positionStatus);

    auto *ready = new QFrame;
    ready->setObjectName("readinessBox");
    auto *readyLayout = new QVBoxLayout(ready);
    readyLayout->setContentsMargins(10, 8, 10, 8);
    readyLayout->setSpacing(3);
    readyLayout->addWidget(label("设备联锁  5 / 5", "sectionTitle"));
    readyLayout->addWidget(label("机架 · 探测器 · 门控 · 急停 · 高压", "mutedText"));
    layout->addWidget(ready);
    m_progressLabel = label("", "mutedText");
    m_progressLabel->setWordWrap(true);
    m_progressLabel->hide();
    layout->addWidget(m_progressLabel);
    m_progress = new QProgressBar;
    m_progress->setRange(0, 100);
    m_progress->setTextVisible(false);
    m_progress->hide();
    layout->addWidget(m_progress);
    layout->addStretch();

    m_primaryButton = new QPushButton("校验定位并进入就绪");
    m_primaryButton->setObjectName("primaryButton");
    connect(m_primaryButton, &QPushButton::clicked, this, &AcquisitionPanel::advancePreparation);
    layout->addWidget(m_primaryButton);
    scroll->setWidget(content);
    return scroll;
}

QWidget *AcquisitionPanel::buildDataPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(2, 12, 2, 2);
    layout->setSpacing(8);

    auto *actions = new QHBoxLayout;
    actions->addWidget(secondary("导入 DICOM"));
    actions->addWidget(secondary("PACS 查询"));
    layout->addLayout(actions);

    auto *tree = new QTreeWidget;
    tree->setContextMenuPolicy(Qt::ActionsContextMenu);
    tree->setHeaderLabels({"场景 / 序列", "状态"});
    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    auto *patient = new QTreeWidgetItem(tree, {"李明 · P20260725018", ""});
    patient->setExpanded(true);
    auto *study = new QTreeWidgetItem(patient, {"全脊柱 CT · 2026-07-25", ""});
    study->setExpanded(true);
    auto *projections = new QTreeWidgetItem(study, {"正交投影 AP / LAT", "2"});
    projections->setExpanded(true);
    auto *ap = new QTreeWidgetItem(projections, {"AP Projection", "512×2048"});
    auto *lat = new QTreeWidgetItem(projections, {"LAT Projection", "512×2048"});
    auto *volume = new QTreeWidgetItem(study, {"CT_RECON_1", "684"});
    volume->setExpanded(true);
    auto *volumeNode = new QTreeWidgetItem(volume, {"Volume", "显示"});
    auto *segments = new QTreeWidgetItem(volume, {"Segmentation", "2"});
    auto *bone = new QTreeWidgetItem(segments, {"Bone", "可见"});
    auto *soft = new QTreeWidgetItem(segments, {"Soft tissue", "隐藏"});
    new QTreeWidgetItem(study, {"Measurements", "3"});
    tree->setCurrentItem(volume);
    for (auto *item : {ap, lat, volumeNode, bone, soft}) {
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
        item->setCheckState(0, item == soft ? Qt::Unchecked : Qt::Checked);
    }
    connect(tree, &QTreeWidget::itemChanged, this, [](QTreeWidgetItem *item, int column) {
        if (column == 0 && item->flags().testFlag(Qt::ItemIsUserCheckable))
            item->setText(1, item->checkState(0) == Qt::Checked ? "可见" : "隐藏");
    });
    auto *renameAction = tree->addAction("重命名节点");
    connect(renameAction, &QAction::triggered, tree, [tree] {
        if (tree->currentItem())
            tree->editItem(tree->currentItem(), 0);
    });
    auto *fitAction = tree->addAction("在全部视图中显示");
    connect(fitAction, &QAction::triggered, tree, [tree] {
        if (tree->currentItem()) {
            tree->currentItem()->setCheckState(0, Qt::Checked);
            tree->currentItem()->setText(1, "可见");
        }
    });
    layout->addWidget(tree, 1);
    layout->addWidget(label("场景树为 UI 占位，第二阶段补充显示开关、颜色与拖放。", "mutedText"));
    return page;
}

void AcquisitionPanel::advancePreparation()
{
    if (m_preparationStage == 0) {
        m_preparationStage = 1;
        m_stageLabel->setText("曝光就绪");
        m_stageHint->setText("患者、协议、定位与设备状态已聚合校验");
        m_positionStatus->setObjectName("okText");
        m_positionStatus->setText("扫描范围与参考平面校验通过");
        m_primaryButton->setText("开始曝光与采集");
        if (m_workflowChangedCallback)
            m_workflowChangedCallback("曝光就绪", "患者、协议、定位和设备联锁校验通过");
        style()->unpolish(m_positionStatus);
        style()->polish(m_positionStatus);
        return;
    }

    if (m_preparationStage == 1) {
        const auto result = QMessageBox::warning(
            this, "确认开始曝光",
            "即将启动 X 射线曝光与采集。\n\n请确认患者体位稳定、机房已清场，并持续按住物理曝光开关。",
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (result != QMessageBox::Yes)
            return;
        m_preparationStage = 2;
        m_stageLabel->setText("采集与预览重建");
        m_stageHint->setText("当前流程保持在同一工作站，不切换整页");
        m_primaryButton->setText("采集进行中");
        m_primaryButton->setEnabled(false);
        m_progress->setValue(0);
        m_progress->show();
        m_progressLabel->setText("采集 AP/LAT · 0%");
        m_progressLabel->show();
        if (m_workflowChangedCallback)
            m_workflowChangedCallback("采集中", "X 射线曝光与同步采集正在进行");
        m_timer->start();
        return;
    }

    if (m_preparationStage == 3) {
        m_tabs->setCurrentIndex(1);
        if (m_workflowChangedCallback)
            m_workflowChangedCallback("影像与重建", "当前检查结果已载入工作站场景");
    }
}
