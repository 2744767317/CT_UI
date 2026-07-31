#include "inspectorpanel.h"

#include "dicomdialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTreeWidget>
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

QFrame *line()
{
    auto *separator = new QFrame;
    separator->setObjectName("sectionLine");
    separator->setFrameShape(QFrame::HLine);
    return separator;
}

QPushButton *button(const QString &text, bool checkable = false)
{
    auto *result = new QPushButton(text);
    result->setObjectName("secondaryButton");
    result->setCheckable(checkable);
    return result;
}

QPushButton *tile(const QString &text, bool checkable = true)
{
    auto *result = new QPushButton(text);
    result->setObjectName("toolTile");
    result->setCheckable(checkable);
    return result;
}

QSlider *slider(int minimum, int maximum, int value)
{
    auto *result = new QSlider(Qt::Horizontal);
    result->setRange(minimum, maximum);
    result->setValue(value);
    return result;
}

QWidget *scrollPage(QVBoxLayout *&layout)
{
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    layout = new QVBoxLayout(content);
    layout->setContentsMargins(4, 12, 4, 8);
    layout->setSpacing(9);
    scroll->setWidget(content);
    return scroll;
}

QWidget *valueRow(const QString &name, const QString &value)
{
    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 3, 0, 3);
    layout->addWidget(label(name, "mutedText"));
    layout->addStretch();
    layout->addWidget(label(value, "valueLabel"));
    return row;
}

} // namespace

InspectorPanel::InspectorPanel(QWidget *parent)
    : QFrame(parent)
{
    setObjectName("sidePanel");
    setMinimumWidth(320);
    setMaximumWidth(410);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 14, 12, 10);
    layout->setSpacing(7);
    layout->addWidget(label("上下文属性", "panelEyebrow"));
    m_titleLabel = label("AXIAL  轴状位", "panelTitle");
    m_metaLabel = label("CT_RECON_1  ·  Slice 412 / 684", "mutedText");
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_metaLabel);

    m_tabs = new QTabWidget;
    m_tabs->setObjectName("inspectorTabs");
    m_tabs->addTab(buildDisplayPage(), "显示");
    m_tabs->addTab(buildReconstructionPage(), "重建");
    m_tabs->addTab(buildSegmentationPage(), "分割");
    m_tabs->addTab(buildMeasurementPage(), "测量");
    m_tabs->addTab(buildDicomPage(), "DICOM");
    layout->addWidget(m_tabs, 1);
}

void InspectorPanel::showModule(int index)
{
    if (index >= 0 && index < m_tabs->count())
        m_tabs->setCurrentIndex(index);
}

void InspectorPanel::setActiveView(const QString &viewName)
{
    m_titleLabel->setText(viewName);
    if (viewName.startsWith("3D"))
        m_metaLabel->setText("CT_RECON_1  ·  Volume rendering");
    else if (viewName.contains("PROJECTION"))
        m_metaLabel->setText("Original projection  ·  512 × 2048");
    else
        m_metaLabel->setText("CT_RECON_1  ·  MPR linked view");
}

QWidget *InspectorPanel::buildDisplayPage()
{
    QVBoxLayout *layout = nullptr;
    auto *page = scrollPage(layout);
    layout->addWidget(label("显示节点", "sectionTitle"));
    auto *node = new QComboBox;
    node->addItems({"CT_RECON_1", "AP Projection", "LAT Projection", "Bone Segmentation"});
    layout->addWidget(node);
    layout->addWidget(line());

    layout->addWidget(label("窗宽 / 窗位", "sectionTitle"));
    auto *preset = new QComboBox;
    preset->addItems({"骨窗  W 1800 / L 450", "软组织  W 400 / L 40", "肺窗  W 1500 / L -600", "自定义"});
    layout->addWidget(preset);
    auto *windowForm = new QFormLayout;
    auto *window = new QSpinBox;
    window->setRange(1, 65535);
    window->setValue(1800);
    auto *level = new QSpinBox;
    level->setRange(-32768, 32767);
    level->setValue(450);
    windowForm->addRow("窗宽 W", window);
    windowForm->addRow("窗位 L", level);
    layout->addLayout(windowForm);
    layout->addWidget(button("自动窗宽窗位"));
    layout->addWidget(line());

    layout->addWidget(label("体绘制", "sectionTitle"));
    auto *volume = new QCheckBox("显示 3D 体绘制");
    volume->setChecked(true);
    layout->addWidget(volume);
    layout->addWidget(label("预设", "mutedText"));
    auto *volumePreset = new QComboBox;
    volumePreset->addItems({"骨骼 + 软组织", "骨骼", "血管增强", "透明皮肤", "MIP"});
    layout->addWidget(volumePreset);
    layout->addWidget(valueRow("整体不透明度", "72%"));
    layout->addWidget(slider(0, 100, 72));
    layout->addWidget(valueRow("采样质量", "交互"));
    layout->addWidget(slider(1, 5, 3));
    auto *interpolation = new QCheckBox("线性插值");
    interpolation->setChecked(true);
    layout->addWidget(interpolation);
    layout->addWidget(line());
    layout->addWidget(label("3D 裁剪", "sectionTitle"));
    auto *clipBox = new QCheckBox("启用裁剪盒");
    layout->addWidget(clipBox);
    layout->addWidget(valueRow("裁剪范围", "完整体数据"));
    layout->addWidget(button("适配当前 3D 视图"));
    layout->addStretch();
    return page;
}

QWidget *InspectorPanel::buildReconstructionPage()
{
    QVBoxLayout *layout = nullptr;
    auto *page = scrollPage(layout);
    layout->addWidget(label("重建数据", "sectionTitle"));
    layout->addWidget(valueRow("输入", "AP + LAT Projection"));
    layout->addWidget(valueRow("当前输出", "CT_RECON_1"));
    layout->addWidget(line());

    layout->addWidget(label("重建方案", "sectionTitle"));
    auto *method = new QComboBox;
    method->addItems({"设备默认重建", "高分辨率骨重建", "低噪声软组织", "快速预览"});
    layout->addWidget(method);
    auto *kernel = new QComboBox;
    kernel->addItems({"Standard B30", "Sharp B70", "Soft B20"});
    auto *voxel = new QDoubleSpinBox;
    voxel->setRange(.2, 3.0);
    voxel->setSingleStep(.1);
    voxel->setValue(.8);
    voxel->setSuffix(" mm");
    auto *form = new QFormLayout;
    form->addRow("重建核", kernel);
    form->addRow("体素尺寸", voxel);
    layout->addLayout(form);
    layout->addWidget(valueRow("预计尺寸", "512 × 512 × 684"));
    layout->addWidget(valueRow("预计显存", "1.1 GB"));
    layout->addWidget(line());
    layout->addWidget(label("输出范围", "sectionTitle"));
    layout->addWidget(new QCheckBox("使用当前裁剪范围"));
    auto *progressLabel = label("", "mutedText");
    progressLabel->hide();
    auto *progress = new QProgressBar;
    progress->setRange(0, 100);
    progress->setTextVisible(false);
    progress->hide();
    auto *preview = button("更新重建预览");
    auto *timer = new QTimer(page);
    timer->setInterval(55);
    connect(preview, &QPushButton::clicked, page, [progress, progressLabel, preview, timer] {
        progress->setValue(0);
        progress->show();
        progressLabel->setText("准备重建任务 · 0%");
        progressLabel->show();
        preview->setEnabled(false);
        timer->start();
    });
    connect(timer, &QTimer::timeout, page, [progress, progressLabel, preview, timer] {
        const int next = qMin(100, progress->value() + 4);
        progress->setValue(next);
        progressLabel->setText(next < 100 ? QString("生成预览体数据 · %1%").arg(next) : "预览已更新 · CT_RECON_PREVIEW_2");
        if (next >= 100) {
            timer->stop();
            preview->setEnabled(true);
            preview->setText("重新生成预览");
        }
    });
    layout->addWidget(preview);
    layout->addWidget(progressLabel);
    layout->addWidget(progress);
    layout->addStretch();
    layout->addWidget(label("本阶段仅建立参数 UI，算法与任务队列后续接入。", "mutedText"));
    return page;
}

QWidget *InspectorPanel::buildSegmentationPage()
{
    QVBoxLayout *layout = nullptr;
    auto *page = scrollPage(layout);
    layout->addWidget(label("活动分割", "sectionTitle"));
    auto *segment = new QComboBox;
    segment->addItems({"Bone", "Soft tissue", "+ 新建分割"});
    layout->addWidget(segment);
    layout->addWidget(valueRow("颜色", "暖白  ■"));
    layout->addWidget(valueRow("可见性", "2D + 3D"));
    layout->addWidget(line());

    layout->addWidget(label("编辑工具", "sectionTitle"));
    auto *tools = new QGridLayout;
    tools->setSpacing(6);
    auto *toolGroup = new QButtonGroup(page);
    toolGroup->setExclusive(true);
    const QStringList names = {"画笔", "擦除", "阈值", "区域生长", "剪刀", "孤岛", "平滑", "填充"};
    for (int i = 0; i < names.size(); ++i) {
        auto *item = tile(names.at(i));
        if (i == 0)
            item->setChecked(true);
        toolGroup->addButton(item);
        tools->addWidget(item, i / 2, i % 2);
    }
    layout->addLayout(tools);
    layout->addWidget(line());
    layout->addWidget(valueRow("画笔直径", "8.0 mm"));
    layout->addWidget(slider(1, 50, 8));
    auto *sphere = new QCheckBox("三维球形画笔");
    sphere->setChecked(true);
    layout->addWidget(sphere);
    auto *applyStatus = label("", "okText");
    auto *apply = button("应用到当前分割");
    connect(apply, &QPushButton::clicked, page, [applyStatus] {
        applyStatus->setText("编辑已应用 · Undo 记录 #18");
    });
    layout->addWidget(apply);
    layout->addWidget(applyStatus);
    layout->addStretch();
    return page;
}

QWidget *InspectorPanel::buildMeasurementPage()
{
    QVBoxLayout *layout = nullptr;
    auto *page = scrollPage(layout);
    layout->addWidget(label("测量工具", "sectionTitle"));
    auto *tools = new QGridLayout;
    tools->setSpacing(6);
    auto *toolGroup = new QButtonGroup(page);
    toolGroup->setExclusive(true);
    const QStringList names = {"长度", "角度", "Cobb 角", "圆形 ROI", "矩形 ROI", "体积"};
    for (int i = 0; i < names.size(); ++i) {
        auto *item = tile(names.at(i));
        toolGroup->addButton(item);
        tools->addWidget(item, i / 2, i % 2);
    }
    layout->addLayout(tools);
    layout->addWidget(line());
    layout->addWidget(label("当前检查测量", "sectionTitle"));

    auto *tree = new QTreeWidget;
    tree->setHeaderLabels({"名称", "结果"});
    tree->setMinimumHeight(170);
    new QTreeWidgetItem(tree, {"Cobb Angle 1", "17.4°"});
    new QTreeWidgetItem(tree, {"Length L1-L5", "142.8 mm"});
    new QTreeWidgetItem(tree, {"Bone ROI", "842 HU"});
    layout->addWidget(tree);
    auto *exportMeasurements = button("导出测量结果");
    connect(exportMeasurements, &QPushButton::clicked, page, [page] {
        QMessageBox::information(page, "测量结果", "已生成结构化测量预览。\n\n3 项测量 · 单位与坐标系校验通过。\n正式版本可导出 DICOM SR、CSV 或报告截图。");
    });
    layout->addWidget(exportMeasurements);
    layout->addStretch();
    return page;
}

QWidget *InspectorPanel::buildDicomPage()
{
    QVBoxLayout *layout = nullptr;
    auto *page = scrollPage(layout);
    layout->addWidget(label("DICOM 对象", "sectionTitle"));
    layout->addWidget(valueRow("Patient", "P20260725018"));
    layout->addWidget(valueRow("Study", "1.2.840...0725"));
    layout->addWidget(valueRow("Series", "CT_RECON_1"));
    layout->addWidget(valueRow("Modality", "CT"));
    layout->addWidget(line());
    layout->addWidget(label("关键标签", "sectionTitle"));

    auto *tree = new QTreeWidget;
    tree->setHeaderLabels({"Tag", "Value"});
    tree->setMinimumHeight(260);
    new QTreeWidgetItem(tree, {"(0010,0010)", "LI^MING"});
    new QTreeWidgetItem(tree, {"(0010,0020)", "P20260725018"});
    new QTreeWidgetItem(tree, {"(0008,0060)", "CT"});
    new QTreeWidgetItem(tree, {"(0028,0010)", "512"});
    new QTreeWidgetItem(tree, {"(0028,0011)", "512"});
    new QTreeWidgetItem(tree, {"(0028,0030)", "0.74\\0.74"});
    new QTreeWidgetItem(tree, {"(0018,0050)", "0.80"});
    layout->addWidget(tree);
    auto *allTags = button("查看全部标签");
    auto *anonymize = button("匿名化副本");
    auto *send = button("发送至 PACS");
    connect(allTags, &QPushButton::clicked, page, [page] {
        DicomDialog(DicomDialog::InitialPage::Import, page).exec();
    });
    connect(anonymize, &QPushButton::clicked, page, [page] {
        QMessageBox::warning(page, "创建匿名化副本", "将创建新的派生检查并移除患者身份字段。\n原始 DICOM 对象不会被修改。", QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
    });
    connect(send, &QPushButton::clicked, page, [page] {
        DicomDialog(DicomDialog::InitialPage::Queue, page).exec();
    });
    layout->addWidget(allTags);
    layout->addWidget(anonymize);
    layout->addWidget(send);
    layout->addStretch();
    return page;
}
