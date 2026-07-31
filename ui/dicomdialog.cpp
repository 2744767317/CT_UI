#include "dicomdialog.h"

#include <QComboBox>
#include <QDateEdit>
#include <QFileDialog>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QLabel *label(const QString &text, const char *name = nullptr)
{
    auto *result = new QLabel(text);
    if (name)
        result->setObjectName(name);
    return result;
}

QPushButton *button(const QString &text, bool primary = false)
{
    auto *result = new QPushButton(text);
    result->setObjectName(primary ? "primaryButton" : "secondaryButton");
    return result;
}

QTableWidget *table(const QStringList &headers, const QList<QStringList> &rows)
{
    auto *result = new QTableWidget(rows.size(), headers.size());
    result->setHorizontalHeaderLabels(headers);
    result->verticalHeader()->hide();
    result->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    result->setSelectionBehavior(QAbstractItemView::SelectRows);
    result->setSelectionMode(QAbstractItemView::SingleSelection);
    result->setEditTriggers(QAbstractItemView::NoEditTriggers);
    result->setAlternatingRowColors(true);
    result->setShowGrid(false);
    for (int row = 0; row < rows.size(); ++row)
        for (int column = 0; column < rows.at(row).size(); ++column)
            result->setItem(row, column, new QTableWidgetItem(rows.at(row).at(column)));
    if (!rows.isEmpty())
        result->selectRow(0);
    return result;
}

} // namespace

DicomDialog::DicomDialog(InitialPage initialPage, QWidget *parent)
    : QDialog(parent)
{
    setObjectName("dicomDialog");
    setWindowTitle("DICOM 数据中心");
    resize(1100, 680);
    setMinimumSize(900, 580);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    auto *titleRow = new QHBoxLayout;
    auto *titles = new QWidget;
    auto *titleLayout = new QVBoxLayout(titles);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(1);
    titleLayout->addWidget(label("DICOM 数据中心", "dialogTitle"));
    titleLayout->addWidget(label("导入、查询、接收与发送医学影像数据", "mutedText"));
    titleRow->addWidget(titles);
    titleRow->addStretch();
    titleRow->addWidget(label("ORTHO_PACS_01 · 已连接", "okText"));
    layout->addLayout(titleRow);

    m_tabs = new QTabWidget;
    m_tabs->setObjectName("dialogTabs");
    m_tabs->addTab(buildImportPage(), "本地导入");
    m_tabs->addTab(buildPacsPage(), "PACS 查询");
    m_tabs->addTab(buildQueuePage(), "传输队列");
    m_tabs->setCurrentIndex(static_cast<int>(initialPage));
    layout->addWidget(m_tabs, 1);

    auto *footer = new QHBoxLayout;
    footer->addWidget(label("当前界面为 UI 演示，不读取或发送真实 DICOM 数据。", "mutedText"));
    footer->addStretch();
    auto *close = button("关闭");
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    footer->addWidget(close);
    layout->addLayout(footer);
}

QWidget *DicomDialog::buildImportPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(2, 14, 2, 2);
    layout->setSpacing(10);

    auto *drop = new QFrame;
    drop->setObjectName("dropZone");
    auto *dropLayout = new QVBoxLayout(drop);
    dropLayout->setContentsMargins(18, 18, 18, 18);
    dropLayout->addWidget(label("将 DICOM 文件或目录拖放到此处", "sectionTitle"), 0, Qt::AlignCenter);
    dropLayout->addWidget(label("支持文件、目录和 DICOMDIR；导入前先扫描患者与序列冲突。", "mutedText"), 0, Qt::AlignCenter);
    auto *choose = button("选择文件或目录");
    choose->setMaximumWidth(180);
    dropLayout->addWidget(choose, 0, Qt::AlignCenter);
    connect(choose, &QPushButton::clicked, this, [this] {
        QFileDialog dialog(this, "选择 DICOM 文件或目录");
        dialog.setFileMode(QFileDialog::ExistingFiles);
        dialog.exec();
    });
    layout->addWidget(drop);
    layout->addWidget(label("最近导入", "sectionTitle"));
    layout->addWidget(table(
        {"患者", "患者 ID", "检查日期", "Modality", "序列", "状态"},
        {{"李明", "P20260725018", "2026-07-25", "CT / DX", "4", "已载入"},
         {"王蕊", "P20260725017", "2026-07-25", "DX", "2", "本地"},
         {"匿名患者", "ANON-0042", "2026-07-24", "CT", "3", "本地"}}), 1);
    return page;
}

QWidget *DicomDialog::buildPacsPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(2, 14, 2, 2);
    layout->setSpacing(10);

    auto *filters = new QHBoxLayout;
    auto *patient = new QLineEdit;
    patient->setPlaceholderText("患者姓名 / ID / Accession Number");
    filters->addWidget(patient, 2);
    auto *date = new QComboBox;
    date->addItems({"今天", "最近 7 天", "最近 30 天", "自定义日期"});
    filters->addWidget(date);
    auto *modality = new QComboBox;
    modality->addItems({"全部模态", "CT", "DX", "CR", "MR"});
    filters->addWidget(modality);
    auto *search = button("查询 PACS", true);
    search->setMaximumWidth(140);
    filters->addWidget(search);
    layout->addLayout(filters);

    auto *results = table(
        {"患者", "患者 ID", "检查日期", "描述", "模态", "序列", "位置"},
        {{"李明", "P20260725018", "2026-07-25 10:30", "全脊柱正交投影 CT", "CT/DX", "4", "ORTHO_PACS_01"},
         {"李明", "P20260725018", "2025-12-10 09:15", "全脊柱随访", "DX", "2", "ORTHO_PACS_01"},
         {"王蕊", "P20260725017", "2026-07-25 10:15", "下肢全长", "DX", "2", "ORTHO_PACS_01"}});
    layout->addWidget(results, 1);
    auto *actions = new QHBoxLayout;
    actions->addWidget(label("查询结果 3 项", "mutedText"));
    actions->addStretch();
    auto *metadata = button("查看检查详情");
    auto *retrieve = button("接收并载入", true);
    retrieve->setMinimumWidth(150);
    actions->addWidget(metadata);
    actions->addWidget(retrieve);
    layout->addLayout(actions);
    connect(metadata, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, "检查详情", "Study Instance UID: 1.2.840...0725\nSeries: 4\nInstances: 688\nTransfer Syntax: Explicit VR Little Endian");
    });
    connect(retrieve, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, "已加入接收队列", "选中检查已加入传输队列。\n正式版本将在后台执行 C-MOVE/C-GET 并校验实例完整性。");
        m_tabs->setCurrentIndex(2);
    });
    return page;
}

QWidget *DicomDialog::buildQueuePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(2, 14, 2, 2);
    layout->setSpacing(10);
    layout->addWidget(label("DICOM 传输任务", "sectionTitle"));
    layout->addWidget(table(
        {"任务", "方向", "对象", "目标", "进度", "状态"},
        {{"TX-260725-018", "发送", "李明 · CT_RECON_1", "ORTHO_PACS_01", "100%", "完成"},
         {"RX-260725-011", "接收", "王蕊 · 下肢全长", "ORTHO_PACS_01", "64%", "传输中"},
         {"TX-260724-042", "发送", "匿名患者 · 测量结果", "RESEARCH_PACS", "0%", "等待"}}), 1);
    auto *actions = new QHBoxLayout;
    actions->addWidget(button("重试失败任务"));
    actions->addWidget(button("清除已完成"));
    actions->addStretch();
    actions->addWidget(label("队列运行中 · 1 个活动任务", "okText"));
    layout->addLayout(actions);
    return page;
}
