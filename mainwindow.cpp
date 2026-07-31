#include "mainwindow.h"

#include "ui/acquisitionpanel.h"
#include "ui/appstyle.h"
#include "ui/dicomdialog.h"
#include "ui/inspectorpanel.h"
#include "ui/safetylockdialog.h"
#include "ui/viewportgrid.h"
#include "ui/workflowpages.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QLabel *label(const QString &text, const char *name = nullptr)
{
    auto *result = new QLabel(text);
    if (name)
        result->setObjectName(name);
    return result;
}

QWidget *statusItem(const QString &caption, const QString &value, const QString &tone = "normal")
{
    auto *item = new QWidget;
    item->setObjectName("topStatusItem");
    item->setProperty("tone", tone);
    auto *layout = new QVBoxLayout(item);
    layout->setContentsMargins(10, 5, 10, 5);
    layout->setSpacing(0);
    layout->addWidget(label(caption, "topStatusCaption"));
    layout->addWidget(label(value, "topStatusValue"));
    return item;
}

QToolButton *tool(QWidget *owner, QStyle::StandardPixmap icon, const QString &text,
                  const QString &tip, bool checkable = false)
{
    auto *button = new QToolButton;
    button->setObjectName("globalTool");
    button->setIcon(owner->style()->standardIcon(icon));
    button->setIconSize(QSize(19, 19));
    button->setText(text);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setToolTip(tip);
    button->setCheckable(checkable);
    return button;
}

QFrame *vSeparator()
{
    auto *line = new QFrame;
    line->setObjectName("toolbarSeparator");
    line->setFrameShape(QFrame::VLine);
    return line;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("光索科技正交投影 CT | 影像工作站");
    resize(1920, 1080);
    setMinimumSize(1440, 820);
    setStyleSheet(AppStyle::styleSheet());

    auto *root = new QWidget;
    root->setObjectName("appRoot");
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(buildTopBar());
    rootLayout->addWidget(buildWorkflowBar());
    m_globalToolbar = buildGlobalToolbar();
    rootLayout->addWidget(m_globalToolbar);

    m_pages = new QStackedWidget;
    m_pages->setObjectName("workflowPages");
    auto *patientPage = new PatientConfirmationPage;
    auto *safetyPage = new SafetyCheckPage;
    auto *rangePage = new ScanRangePage;
    patientPage->setContinueCallback([this] { setWorkflowPage(1, true); });
    safetyPage->setBackCallback([this] { setWorkflowPage(0); });
    safetyPage->setContinueCallback([this] { setWorkflowPage(2, true); });
    rangePage->setBackCallback([this] { setWorkflowPage(1); });
    rangePage->setContinueCallback([this] { setWorkflowPage(3, true); });
    m_pages->addWidget(patientPage);
    m_pages->addWidget(safetyPage);
    m_pages->addWidget(rangePage);
    m_pages->addWidget(buildImagingWorkspace());
    rootLayout->addWidget(m_pages, 1);

    m_bottomStatusBar = buildStatusBar();
    rootLayout->addWidget(m_bottomStatusBar);
    setCentralWidget(root);
    setWorkflowPage(0);
}

QWidget *MainWindow::buildTopBar()
{
    auto *bar = new QWidget;
    bar->setObjectName("topBar");
    bar->setFixedHeight(68);
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(18, 7, 14, 7);
    layout->setSpacing(12);

    auto *brand = new QWidget;
    auto *brandLayout = new QVBoxLayout(brand);
    brandLayout->setContentsMargins(0, 0, 0, 0);
    brandLayout->setSpacing(0);
    brandLayout->addWidget(label("光索科技", "brand"));
    brandLayout->addWidget(label("正交投影 CT 影像工作站", "brandSub"));
    layout->addWidget(brand);
    layout->addSpacing(22);

    auto *study = new QWidget;
    auto *studyLayout = new QVBoxLayout(study);
    studyLayout->setContentsMargins(0, 0, 0, 0);
    studyLayout->setSpacing(1);
    studyLayout->addWidget(label("李明  ·  P20260725018", "activeStudy"));
    studyLayout->addWidget(label("全脊柱 AP/LAT  ·  ACC-2026-0725-003", "activeStudyMeta"));
    layout->addWidget(study);
    layout->addStretch();

    auto *workflow = statusItem("检查状态", "准备检查", "accent");
    m_workflowStatus = workflow->findChild<QLabel *>("topStatusValue");
    layout->addWidget(workflow);
    layout->addWidget(statusItem("设备", "在线 · 就绪"));
    layout->addWidget(statusItem("联锁 / 急停", "闭合 · 正常"));
    layout->addWidget(statusItem("DICOM / PACS", "已连接"));

    auto *settings = new QToolButton;
    settings->setObjectName("topIconButton");
    settings->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    settings->setIconSize(QSize(22, 22));
    settings->setToolTip("系统与设备设置");
    layout->addWidget(settings);
    return bar;
}

QWidget *MainWindow::buildGlobalToolbar()
{
    auto *bar = new QWidget;
    bar->setObjectName("globalToolbar");
    bar->setFixedHeight(52);
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 5, 12, 5);
    layout->setSpacing(4);

    auto *importDicom = tool(this, QStyle::SP_DirOpenIcon, "导入", "导入 DICOM 文件或目录");
    auto *pacs = tool(this, QStyle::SP_DriveNetIcon, "PACS", "查询并接收 PACS 检查");
    connect(importDicom, &QToolButton::clicked, this, [this] {
        DicomDialog(DicomDialog::InitialPage::Import, this).exec();
    });
    connect(pacs, &QToolButton::clicked, this, [this] {
        DicomDialog(DicomDialog::InitialPage::Pacs, this).exec();
    });
    layout->addWidget(importDicom);
    layout->addWidget(pacs);
    layout->addWidget(vSeparator());

    auto *toolGroup = new QButtonGroup(bar);
    toolGroup->setExclusive(true);
    auto *cursor = tool(this, QStyle::SP_ArrowRight, "选择", "选择对象", true);
    cursor->setShortcut(QKeySequence("1"));
    cursor->setChecked(true);
    layout->addWidget(cursor);
    auto *crosshair = tool(this, QStyle::SP_TitleBarShadeButton, "十字线", "定位并联动三个切片", true);
    auto *windowing = tool(this, QStyle::SP_BrowserReload, "窗宽窗位", "拖动调整窗宽窗位", true);
    auto *pan = tool(this, QStyle::SP_ArrowUp, "平移", "平移当前视图", true);
    auto *zoom = tool(this, QStyle::SP_DesktopIcon, "缩放", "缩放当前视图", true);
    crosshair->setShortcut(QKeySequence("2"));
    windowing->setShortcut(QKeySequence("3"));
    pan->setShortcut(QKeySequence("4"));
    zoom->setShortcut(QKeySequence("5"));
    toolGroup->addButton(cursor);
    toolGroup->addButton(crosshair);
    toolGroup->addButton(windowing);
    toolGroup->addButton(pan);
    toolGroup->addButton(zoom);
    layout->addWidget(crosshair);
    layout->addWidget(windowing);
    layout->addWidget(pan);
    layout->addWidget(zoom);
    connect(toolGroup, &QButtonGroup::buttonClicked, this, [this](QAbstractButton *button) {
        if (m_viewports)
            m_viewports->setToolMode(button->text());
    });
    layout->addWidget(vSeparator());

    auto *measurement = tool(this, QStyle::SP_FileDialogDetailedView, "测量", "打开测量工具");
    measurement->setShortcut(QKeySequence("M"));
    connect(measurement, &QToolButton::clicked, this, [this] {
        if (m_inspector) {
            m_inspector->show();
            m_inspector->showModule(3);
        }
    });
    auto *undo = tool(this, QStyle::SP_DialogResetButton, "撤销", "撤销上一步编辑");
    auto *redo = tool(this, QStyle::SP_DialogApplyButton, "重做", "恢复上一步编辑");
    undo->setShortcut(QKeySequence::Undo);
    redo->setShortcut(QKeySequence::Redo);
    layout->addWidget(measurement);
    layout->addWidget(undo);
    layout->addWidget(redo);
    auto *safety = tool(this, QStyle::SP_MessageBoxWarning, "安全", "查看异常与联锁状态演示");
    connect(safety, &QToolButton::clicked, this, [this] {
        const QString previousState = m_workflowStatus ? m_workflowStatus->text() : QString();
        if (m_workflowStatus)
            m_workflowStatus->setText("异常 / 锁定");
        SafetyLockDialog(this).exec();
        if (m_workflowStatus)
            m_workflowStatus->setText(previousState);
    });
    layout->addWidget(safety);
    layout->addWidget(vSeparator());
    auto *leftPanel = tool(this, QStyle::SP_ArrowLeft, "检查栏", "显示或隐藏检查与数据面板", true);
    auto *rightPanel = tool(this, QStyle::SP_ArrowRight, "属性栏", "显示或隐藏上下文属性面板", true);
    leftPanel->setChecked(true);
    rightPanel->setChecked(true);
    connect(leftPanel, &QToolButton::toggled, this, [this](bool visible) {
        if (m_acquisition)
            m_acquisition->setVisible(visible);
    });
    connect(rightPanel, &QToolButton::toggled, this, [this](bool visible) {
        if (m_inspector)
            m_inspector->setVisible(visible);
    });
    layout->addWidget(leftPanel);
    layout->addWidget(rightPanel);
    layout->addStretch();

    layout->addWidget(label("布局", "toolbarLabel"));
    auto *layoutSelector = new QComboBox;
    layoutSelector->setObjectName("layoutSelector");
    layoutSelector->addItems({"四视图  2 × 2", "三切片 + 大 3D", "仅 3D", "正交投影双视图"});
    layoutSelector->setMinimumWidth(172);
    connect(layoutSelector, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_viewports)
            m_viewports->setLayoutMode(index);
    });
    layout->addWidget(layoutSelector);
    layout->addWidget(tool(this, QStyle::SP_BrowserReload, "适窗", "全部视图恢复适窗"));
    return bar;
}

QWidget *MainWindow::buildWorkflowBar()
{
    auto *bar = new QWidget;
    bar->setObjectName("workflowBar");
    bar->setFixedHeight(54);
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(18, 5, 18, 5);
    layout->setSpacing(0);
    const QStringList steps = {
        "01  患者确认", "02  联锁检查", "03  扫描范围", "04  影像编辑"
    };
    for (int index = 0; index < steps.size(); ++index) {
        auto *button = new QPushButton(steps.at(index));
        button->setObjectName("workflowStepButton");
        button->setProperty("step", index);
        button->setProperty("state", index == 0 ? "active" : "pending");
        connect(button, &QPushButton::clicked, this, [this, index] {
            if (index <= m_maxReachedPage)
                setWorkflowPage(index);
        });
        layout->addWidget(button, 1);
        m_stepButtons[index] = button;
    }
    return bar;
}

QWidget *MainWindow::buildImagingWorkspace()
{
    auto *workspace = new QWidget;
    auto *layout = new QVBoxLayout(workspace);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->setObjectName("workspaceSplitter");
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(3);
    m_acquisition = new AcquisitionPanel;
    m_acquisition->setEditingMode();
    m_viewports = new ViewportGrid;
    m_inspector = new InspectorPanel;
    m_viewports->setActiveViewChangedCallback([this](const QString &viewName) {
        m_inspector->setActiveView(viewName);
        if (m_coordinateStatus)
            m_coordinateStatus->setText(viewName + "  ·  RAS: -12.4, 36.8, 104.2 mm");
    });
    m_splitter->addWidget(m_acquisition);
    m_splitter->addWidget(m_viewports);
    m_splitter->addWidget(m_inspector);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);
    m_splitter->setSizes({330, 1240, 350});
    layout->addWidget(m_splitter);
    return workspace;
}

void MainWindow::setWorkflowPage(int index, bool advance)
{
    if (!m_pages || index < 0 || index >= m_pages->count())
        return;
    if (advance)
        m_maxReachedPage = qMax(m_maxReachedPage, index);
    if (index > m_maxReachedPage)
        return;

    m_currentPage = index;
    m_pages->setCurrentIndex(index);
    const QStringList states = {"患者确认", "联锁检查", "扫描范围", "影像编辑"};
    const QStringList details = {
        "核对姓名和患者 ID", "检查设备联锁与曝光条件",
        "设置采集上界与下界", "查看、分割、测量和导出影像"
    };
    if (m_workflowStatus) {
        m_workflowStatus->setText(states.at(index));
        m_workflowStatus->setToolTip(details.at(index));
    }
    if (m_globalToolbar)
        m_globalToolbar->setVisible(index == 3);
    if (m_bottomStatusBar)
        m_bottomStatusBar->setVisible(index == 3);

    for (int step = 0; step < 4; ++step) {
        const QString state = step == index ? "active" : step < m_maxReachedPage ? "done" : "pending";
        m_stepButtons[step]->setProperty("state", state);
        m_stepButtons[step]->setEnabled(step <= m_maxReachedPage);
        m_stepButtons[step]->style()->unpolish(m_stepButtons[step]);
        m_stepButtons[step]->style()->polish(m_stepButtons[step]);
    }
}

QWidget *MainWindow::buildStatusBar()
{
    auto *bar = new QWidget;
    bar->setObjectName("bottomStatusBar");
    bar->setFixedHeight(28);
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(18);
    layout->addWidget(label("Series: CT_RECON_1  ·  512 × 512 × 684", "statusText"));
    layout->addWidget(label("Spacing: 0.74 × 0.74 × 0.80 mm", "statusText"));
    layout->addStretch();
    m_coordinateStatus = label("AXIAL 轴状位  ·  RAS: -12.4, 36.8, 104.2 mm", "statusText");
    layout->addWidget(m_coordinateStatus);
    layout->addWidget(label("Value:  842 HU", "statusStrong"));
    layout->addWidget(label("GPU: 就绪", "statusOk"));
    return bar;
}
