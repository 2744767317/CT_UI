#include "mainwindow.h"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

namespace {

constexpr int kRailWidth = 304;
constexpr int kContextWidth = 328;

class CompactStackedWidget final : public QStackedWidget
{
public:
    using QStackedWidget::QStackedWidget;

    QSize minimumSizeHint() const override { return {0, 0}; }
    QSize sizeHint() const override { return {800, 640}; }
};

QLabel *makeLabel(const QString &text, const char *name = nullptr)
{
    auto *label = new QLabel(text);
    if (name)
        label->setObjectName(name);
    return label;
}

QFrame *separator()
{
    auto *line = new QFrame;
    line->setObjectName("separator");
    line->setFrameShape(QFrame::HLine);
    return line;
}

QWidget *statusChip(const QString &title, const QString &value, const QString &tone = "ok")
{
    auto *chip = new QWidget;
    chip->setObjectName("statusChip");
    chip->setProperty("tone", tone);
    auto *layout = new QVBoxLayout(chip);
    layout->setContentsMargins(12, 7, 12, 7);
    layout->setSpacing(1);
    auto *caption = makeLabel(title, "chipCaption");
    auto *content = makeLabel(value, "chipValue");
    layout->addWidget(caption);
    layout->addWidget(content);
    return chip;
}

QPushButton *secondaryButton(const QString &text)
{
    auto *button = new QPushButton(text);
    button->setObjectName("secondaryButton");
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(44);
    return button;
}

QToolButton *iconButton(QWidget *owner, QStyle::StandardPixmap icon, const QString &tip)
{
    auto *button = new QToolButton;
    button->setObjectName("iconButton");
    button->setIcon(owner->style()->standardIcon(icon));
    button->setIconSize(QSize(22, 22));
    button->setToolTip(tip);
    button->setAccessibleName(tip);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QWidget *metricRow(const QString &label, const QString &value, const QString &note = {})
{
    auto *row = new QWidget;
    row->setObjectName("metricRow");
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 7, 0, 7);
    layout->setSpacing(8);
    auto *name = makeLabel(label, "metricLabel");
    auto *result = makeLabel(value, "metricValue");
    layout->addWidget(name);
    layout->addStretch();
    if (!note.isEmpty())
        layout->addWidget(makeLabel(note, "metricNote"));
    layout->addWidget(result);
    return row;
}

QWidget *callout(const QString &title, const QString &body, const QString &tone = "neutral")
{
    auto *box = new QFrame;
    box->setObjectName("callout");
    box->setProperty("tone", tone);
    auto *layout = new QVBoxLayout(box);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(4);
    layout->addWidget(makeLabel(title, "calloutTitle"));
    auto *content = makeLabel(body, "calloutBody");
    content->setWordWrap(true);
    layout->addWidget(content);
    return box;
}

class ScanCanvas final : public QWidget
{
public:
    enum class Mode { Positioning, Acquisition, Frontal, Lateral };

    explicit ScanCanvas(Mode mode, QWidget *parent = nullptr)
        : QWidget(parent), m_mode(mode)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMinimumSize(360, 420);
    }

    void setProgress(int progress)
    {
        m_progress = progress;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#080b0e"));

        const QRectF film = rect().adjusted(28, 22, -28, -22);
        QLinearGradient bg(film.topLeft(), film.bottomRight());
        bg.setColorAt(0, QColor(35, 41, 45));
        bg.setColorAt(0.45, QColor(7, 10, 12));
        bg.setColorAt(1, QColor(27, 31, 34));
        p.fillRect(film, bg);

        drawBody(p, film);
        drawMarkers(p, film);

        if (m_mode == Mode::Positioning || m_mode == Mode::Acquisition) {
            const qreal top = film.top() + film.height() * .17;
            const qreal bottom = film.bottom() - film.height() * .12;
            p.setPen(QPen(QColor("#e28a38"), 2));
            p.drawLine(film.left() + 16, top, film.right() - 16, top);
            p.drawLine(film.left() + 16, bottom, film.right() - 16, bottom);
            p.setPen(QPen(QColor(226, 138, 56, 130), 1, Qt::DashLine));
            p.drawLine(film.center().x(), film.top() + 12, film.center().x(), film.bottom() - 12);
            p.drawLine(film.left() + 12, film.center().y(), film.right() - 12, film.center().y());

            if (m_mode == Mode::Acquisition) {
                const qreal scanY = top + (bottom - top) * (m_progress / 100.0);
                p.fillRect(QRectF(film.left(), scanY - 13, film.width(), 26), QColor(226, 138, 56, 30));
                p.setPen(QPen(QColor("#f39a45"), 3));
                p.drawLine(film.left(), scanY, film.right(), scanY);
            }
        }
    }

private:
    void drawBody(QPainter &p, const QRectF &r)
    {
        p.save();
        p.setClipRect(r);
        p.translate(r.left(), r.top());
        p.scale(r.width() / 420.0, r.height() / 760.0);

        const QColor soft(153, 166, 171, 34);
        const QColor bone(218, 225, 226, 116);
        const bool lateral = m_mode == Mode::Lateral;
        p.setPen(Qt::NoPen);
        p.setBrush(soft);
        QPainterPath body;
        if (lateral) {
            body.moveTo(224, 18);
            body.cubicTo(310, 40, 304, 130, 265, 165);
            body.cubicTo(322, 224, 326, 360, 282, 474);
            body.cubicTo(260, 548, 304, 642, 245, 748);
            body.lineTo(134, 748);
            body.cubicTo(108, 590, 128, 520, 106, 424);
            body.cubicTo(76, 286, 116, 198, 163, 157);
            body.cubicTo(133, 102, 146, 38, 224, 18);
        } else {
            body.moveTo(210, 18);
            body.cubicTo(130, 22, 137, 112, 165, 151);
            body.cubicTo(82, 185, 75, 323, 112, 438);
            body.cubicTo(132, 502, 108, 590, 96, 750);
            body.lineTo(324, 750);
            body.cubicTo(312, 590, 288, 502, 308, 438);
            body.cubicTo(345, 323, 338, 185, 255, 151);
            body.cubicTo(283, 112, 290, 22, 210, 18);
        }
        p.drawPath(body);

        p.setBrush(bone);
        p.setPen(QPen(QColor(232, 238, 239, 120), 4));
        p.drawEllipse(lateral ? QRectF(164, 24, 108, 126) : QRectF(158, 22, 104, 122));
        if (lateral) {
            QPainterPath spine;
            spine.moveTo(220, 145);
            spine.cubicTo(188, 255, 247, 365, 210, 492);
            spine.cubicTo(187, 560, 225, 626, 204, 692);
            p.drawPath(spine);
            for (int y = 160; y < 650; y += 28)
                p.drawRoundedRect(QRectF(204 + 14 * std::sin(y / 60.0), y, 40, 12), 3, 3);
        } else {
            p.drawLine(QPointF(210, 145), QPointF(210, 655));
            for (int y = 154; y < 650; y += 27)
                p.drawRoundedRect(QRectF(195, y, 30, 11), 3, 3);
            for (int i = 0; i < 9; ++i) {
                const qreal y = 178 + i * 28;
                const qreal w = 102 - i * 3;
                p.drawArc(QRectF(210 - w, y, w, 86), 20 * 16, 140 * 16);
                p.drawArc(QRectF(210, y, w, 86), 20 * 16, 140 * 16);
            }
            p.drawEllipse(QRectF(135, 595, 150, 108));
            p.drawLine(QPointF(161, 688), QPointF(146, 760));
            p.drawLine(QPointF(259, 688), QPointF(274, 760));
        }
        p.restore();
    }

    void drawMarkers(QPainter &p, const QRectF &r)
    {
        p.setPen(QColor("#d8dedf"));
        QFont f = p.font();
        f.setPixelSize(13);
        p.setFont(f);
        p.drawText(r.adjusted(12, 10, -12, -10), Qt::AlignTop | Qt::AlignLeft,
                   "LI MING  P20260725018\nAP / STANDING");
        f.setBold(true);
        f.setPixelSize(17);
        p.setFont(f);
        p.drawText(r.adjusted(13, 0, -13, -13), Qt::AlignBottom | Qt::AlignLeft,
                   m_mode == Mode::Lateral ? "A" : "R");
    }

    Mode m_mode;
    int m_progress = 0;
};

QString applicationStyle()
{
    return QStringLiteral(R"(
        * { font-family: "Microsoft YaHei UI"; font-size: 14px; color: #d7dcdf; }
        QMainWindow, QWidget#appRoot { background: #15191c; }
        QWidget#topBar { background: #202529; border-bottom: 1px solid #363d42; }
        QLabel#brand { color: #f1f3f4; font-size: 22px; font-weight: 700; }
        QLabel#brandSub { color: #8f999f; font-size: 12px; }
        QLabel#stateLabel { color: #f2a052; font-size: 17px; font-weight: 700; }
        QLabel#headerHint { color: #9da7ac; font-size: 12px; }
        QWidget#statusChip { background: #292f33; border: 1px solid #394146; border-radius: 4px; min-width: 112px; }
        QLabel#chipCaption { color: #849096; font-size: 11px; }
        QLabel#chipValue { color: #cfd6d9; font-size: 13px; font-weight: 600; }
        QFrame#leftRail, QFrame#rightRail { background: #1b2024; border: 0; }
        QFrame#leftRail { border-right: 1px solid #343b40; }
        QFrame#rightRail { border-left: 1px solid #343b40; }
        QLabel#railCaption, QLabel#eyebrow { color: #7f8a90; font-size: 11px; font-weight: 700; }
        QLabel#patientName { color: #f0f2f3; font-size: 21px; font-weight: 700; }
        QLabel#patientMeta { color: #9da7ac; font-size: 13px; }
        QLabel#protocolValue { color: #d6dadd; font-size: 14px; font-weight: 600; }
        QLabel#step { color: #6f797f; min-height: 30px; padding-left: 8px; border-left: 2px solid #3a4247; }
        QLabel#step[status="done"] { color: #aeb7bb; border-left-color: #6e8f7b; }
        QLabel#step[status="active"] { color: #f3a154; font-weight: 700; border-left: 3px solid #e38b3b; }
        QFrame#separator { color: #343b40; background: #343b40; max-height: 1px; border: 0; }
        QWidget#workspace { background: #111518; }
        QWidget#page { background: #111518; }
        QLabel#pageTitle { color: #f0f2f3; font-size: 24px; font-weight: 700; }
        QLabel#pageSubtitle { color: #8f999f; font-size: 13px; }
        QFrame#actionBar { background: #1b2024; border-top: 1px solid #343b40; }
        QPushButton#primaryAction { background: #dd8335; color: #171a1c; border: 1px solid #ef9b50; border-radius: 4px; min-height: 52px; min-width: 270px; padding: 0 24px; font-size: 16px; font-weight: 700; }
        QPushButton#primaryAction:hover { background: #ec9549; }
        QPushButton#primaryAction:pressed { background: #c97029; }
        QPushButton#primaryAction:disabled { background: #333a3f; color: #778087; border-color: #414a4f; }
        QPushButton#secondaryButton, QToolButton#iconButton { background: #292f33; border: 1px solid #424a4f; border-radius: 4px; color: #d2d7da; padding: 0 14px; }
        QPushButton#secondaryButton:hover, QToolButton#iconButton:hover { border-color: #7b858b; background: #32393e; }
        QPushButton#secondaryButton:checked { border: 2px solid #df8739; color: #f1a057; background: #2d2b28; }
        QToolButton#iconButton { min-width: 46px; min-height: 46px; padding: 0; }
        QPushButton#dangerGhost { background: transparent; color: #d4a079; border: 1px solid #76533b; border-radius: 4px; min-height: 38px; padding: 0 12px; }
        QLineEdit, QComboBox, QSpinBox { background: #252b2f; border: 1px solid #41494e; border-radius: 3px; min-height: 40px; padding: 0 10px; selection-background-color: #c9752f; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border-color: #db863a; }
        QTableWidget { background: #171c20; alternate-background-color: #1c2226; border: 1px solid #343c41; gridline-color: #2d3438; selection-background-color: #41362c; selection-color: #f1f2f3; outline: 0; }
        QTableWidget::item { padding: 10px 8px; border-bottom: 1px solid #2c3337; }
        QHeaderView::section { background: #252b2f; color: #9da7ac; border: 0; border-right: 1px solid #353c41; padding: 10px 8px; font-weight: 600; }
        QFrame#protocolCard { background: #1c2125; border: 1px solid #3a4247; border-radius: 5px; }
        QLabel#protocolName { color: #e9edef; font-size: 18px; font-weight: 700; }
        QLabel#protocolMeta { color: #939da2; font-size: 12px; }
        QPushButton#protocolSelect { background: #292f33; border: 1px solid #4a5358; border-radius: 4px; min-height: 44px; font-weight: 600; }
        QPushButton#protocolSelect:checked { color: #f0a45f; border: 2px solid #dd8335; background: #332d28; }
        QFrame#callout { background: #20262a; border: 1px solid #3b4449; border-left: 3px solid #637179; border-radius: 3px; }
        QFrame#callout[tone="ok"] { border-left-color: #668d76; }
        QFrame#callout[tone="warning"] { border-left-color: #df8739; background: #29241f; }
        QFrame#callout[tone="danger"] { border-left-color: #c85f4e; background: #2a2020; }
        QLabel#calloutTitle { color: #e5e9eb; font-weight: 700; }
        QLabel#calloutBody { color: #9da7ac; font-size: 12px; }
        QWidget#metricRow { border-bottom: 1px solid #30373b; }
        QLabel#metricLabel { color: #96a0a5; }
        QLabel#metricValue { color: #e1e5e7; font-weight: 700; }
        QLabel#metricNote { color: #718078; font-size: 11px; }
        QLabel#contextTitle { color: #eff1f2; font-size: 18px; font-weight: 700; }
        QLabel#contextBody { color: #99a3a8; line-height: 1.4; }
        QLabel#sectionTitle { color: #cfd5d8; font-size: 14px; font-weight: 700; padding-top: 4px; }
        QCheckBox { spacing: 10px; min-height: 36px; }
        QCheckBox::indicator { width: 22px; height: 22px; border: 1px solid #626d73; background: #20262a; }
        QCheckBox::indicator:checked { background: #d77f32; border-color: #ed9b50; }
        QSlider::groove:horizontal { height: 5px; background: #3b4348; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: #a66b39; }
        QSlider::handle:horizontal { width: 18px; margin: -7px 0; background: #d7dcdf; border: 1px solid #879197; border-radius: 9px; }
        QProgressBar { background: #252b2f; border: 1px solid #3b4449; border-radius: 3px; min-height: 12px; max-height: 12px; text-align: center; }
        QProgressBar::chunk { background: #d98236; }
        QLabel#acquisitionPercent { color: #f0a45f; font-size: 42px; font-weight: 700; }
        QLabel#exposureBadge { color: #171a1c; background: #e18a3d; border-radius: 3px; padding: 7px 12px; font-weight: 800; }
        QTabWidget::pane { border: 0; top: -1px; }
        QTabBar::tab { background: #22282c; color: #8f999f; min-width: 64px; min-height: 40px; border-bottom: 2px solid #343b40; }
        QTabBar::tab:selected { color: #f0a45f; border-bottom-color: #dd8335; }
        QLabel#lockedCode { color: #d98576; font-size: 19px; font-weight: 700; }
        QLabel#auditLabel { color: #788389; font-size: 11px; }
        QScrollBar:vertical { background: #20262a; width: 12px; }
        QScrollBar::handle:vertical { background: #4a5358; min-height: 32px; }
    )");
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("正交投影 CT 系统 | 操作控制台");
    resize(1920, 1080);
    setMinimumSize(1440, 860);
    setStyleSheet(applicationStyle());

    auto *root = new QWidget;
    root->setObjectName("appRoot");
    auto *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(buildTopBar());

    auto *body = new QWidget;
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    bodyLayout->addWidget(buildPatientRail());
    bodyLayout->addWidget(buildCenterWorkspace(), 1);
    bodyLayout->addWidget(buildContextRail());
    rootLayout->addWidget(body, 1);
    setCentralWidget(root);

    m_acquisitionTimer = new QTimer(this);
    m_acquisitionTimer->setInterval(320);
    connect(m_acquisitionTimer, &QTimer::timeout, this, [this] {
        m_acquisitionProgress = qMin(100, m_acquisitionProgress + 5);
        m_acquisitionBar->setValue(m_acquisitionProgress);
        m_acquisitionPercent->setText(QString::number(m_acquisitionProgress) + "%");
        if (auto *canvas = findChild<QWidget *>("acquisitionCanvas"))
            static_cast<ScanCanvas *>(canvas)->setProgress(m_acquisitionProgress);
        if (m_acquisitionProgress >= 100) {
            m_acquisitionTimer->stop();
            setState(WorkflowState::Reviewing, "采集完成，系统生成 AP/LAT 正交图像");
        }
    });

    connect(m_primaryAction, &QPushButton::clicked, this, [this] {
        switch (m_state) {
        case WorkflowState::NoPatient: {
            const auto result = QMessageBox::question(
                this, "患者身份确认",
                "请核对患者腕带与屏幕信息：\n\n李明  男  38 岁\n患者 ID：P20260725018\n出生日期：1988-06-16\n\n以上信息是否与患者本人一致？",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (result == QMessageBox::Yes) {
                m_patientVerified = true;
                m_patientName->setText("李明");
                m_patientMeta->setText("男 · 38 岁\nP20260725018\nACC-2026-0725-003");
                setState(WorkflowState::PatientConfirmed, "双标识核对完成：姓名 + 患者 ID");
            }
            break;
        }
        case WorkflowState::PatientConfirmed:
            break;
        case WorkflowState::ProtocolSelected:
            m_protocolLocked = true;
            setState(WorkflowState::Positioning, "协议已锁定：全脊柱 AP/LAT · 成人低剂量");
            break;
        case WorkflowState::Positioning:
            setState(WorkflowState::Ready, "定位校验通过：参考平面与扫描范围已确认");
            break;
        case WorkflowState::Ready: {
            const auto result = QMessageBox::warning(
                this, "确认开始曝光",
                "即将启动 X 射线曝光与同步采集。\n\n请确认患者保持站立、机房已清场，并持续按住物理曝光开关。",
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (result == QMessageBox::Yes)
                beginAcquisition();
            break;
        }
        case WorkflowState::Acquiring:
            break;
        case WorkflowState::Reviewing: {
            const auto result = QMessageBox::question(
                this, "完成检查", "确认图像可诊断并完成本次检查？\n结果将写入 PACS 队列与操作日志。",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (result == QMessageBox::Yes) {
                m_patientVerified = false;
                m_protocolLocked = false;
                m_protocolValue->setText("未选择");
                m_patientName->setText("未选择患者");
                m_patientMeta->setText("从工作列表选择并完成双标识核对");
                setState(WorkflowState::NoPatient, "检查归档完成，工作站等待下一位患者");
            }
            break;
        }
        case WorkflowState::Locked:
            setState(m_stateBeforeLock, "设备联锁复核完成，操作界面已解锁");
            break;
        }
    });

    setState(WorkflowState::NoPatient, "系统自检通过，工作站就绪");
}

QWidget *MainWindow::buildTopBar()
{
    auto *bar = new QWidget;
    bar->setObjectName("topBar");
    bar->setFixedHeight(76);
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(20, 8, 16, 8);
    layout->setSpacing(12);

    auto *brandBox = new QWidget;
    auto *brandLayout = new QVBoxLayout(brandBox);
    brandLayout->setContentsMargins(0, 0, 0, 0);
    brandLayout->setSpacing(0);
    brandLayout->addWidget(makeLabel("光索科技", "brand"));
    brandLayout->addWidget(makeLabel("正交投影成像系统", "brandSub"));
    layout->addWidget(brandBox);
    layout->addSpacing(24);

    auto *stateBox = new QWidget;
    auto *stateLayout = new QVBoxLayout(stateBox);
    stateLayout->setContentsMargins(0, 0, 0, 0);
    stateLayout->setSpacing(1);
    m_stateLabel = makeLabel("未选患者", "stateLabel");
    m_headerHint = makeLabel("选择工作列表中的患者并核对身份", "headerHint");
    stateLayout->addWidget(m_stateLabel);
    stateLayout->addWidget(m_headerHint);
    layout->addWidget(stateBox, 1);

    layout->addWidget(statusChip("设备", "在线 · 自检通过"));
    layout->addWidget(statusChip("机架联锁", "闭合"));
    layout->addWidget(statusChip("急停", "正常"));
    layout->addWidget(statusChip("网络 / PACS", "已连接"));

    m_interlockButton = new QPushButton("模拟联锁");
    m_interlockButton->setObjectName("dangerGhost");
    m_interlockButton->setToolTip("原型演示：触发或解除设备联锁");
    connect(m_interlockButton, &QPushButton::clicked, this, &MainWindow::toggleInterlock);
    layout->addWidget(m_interlockButton);
    layout->addWidget(iconButton(this, QStyle::SP_ComputerIcon, "设备与系统设置"));
    return bar;
}

QWidget *MainWindow::buildPatientRail()
{
    auto *rail = new QFrame;
    rail->setObjectName("leftRail");
    rail->setFixedWidth(kRailWidth);
    auto *layout = new QVBoxLayout(rail);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(12);

    layout->addWidget(makeLabel("当前患者", "railCaption"));
    m_patientName = makeLabel("未选择患者", "patientName");
    m_patientMeta = makeLabel("从工作列表选择并完成双标识核对", "patientMeta");
    m_patientMeta->setWordWrap(true);
    layout->addWidget(m_patientName);
    layout->addWidget(m_patientMeta);
    layout->addWidget(separator());
    layout->addWidget(makeLabel("检查任务", "railCaption"));
    layout->addWidget(metricRow("申请项目", "全脊柱正侧位"));
    layout->addWidget(metricRow("检查号", "0725-003"));
    auto *protocolRow = new QWidget;
    auto *protocolLayout = new QVBoxLayout(protocolRow);
    protocolLayout->setContentsMargins(0, 5, 0, 5);
    protocolLayout->setSpacing(4);
    protocolLayout->addWidget(makeLabel("当前协议", "metricLabel"));
    m_protocolValue = makeLabel("未选择", "protocolValue");
    m_protocolValue->setWordWrap(true);
    protocolLayout->addWidget(m_protocolValue);
    layout->addWidget(protocolRow);
    layout->addWidget(separator());
    layout->addWidget(makeLabel("任务进度", "railCaption"));

    const QStringList steps = {
        "01  患者确认", "02  协议选择", "03  体位与参考平面",
        "04  曝光准备", "05  图像采集", "06  结果与导出"
    };
    for (int i = 0; i < steps.size(); ++i) {
        auto *step = makeLabel(steps.at(i), "step");
        step->setProperty("order", i);
        step->setProperty("status", "pending");
        layout->addWidget(step);
    }
    layout->addStretch();
    layout->addWidget(separator());
    layout->addWidget(makeLabel("最近操作", "railCaption"));
    m_auditLabel = makeLabel("等待操作", "auditLabel");
    m_auditLabel->setWordWrap(true);
    layout->addWidget(m_auditLabel);
    auto *log = secondaryButton("查看完整操作日志");
    log->setToolTip("显示带时间戳、操作者和设备事件的审计记录");
    connect(log, &QPushButton::clicked, this, [this] {
        QMessageBox::information(
            this, "本次会话操作日志",
            "10:08:45  SYSTEM   设备自检通过，工作站就绪\n"
            "10:08:45  SYSTEM   门控、急停、高压与探测器状态正常\n"
            "--:--:--  TECH     等待患者双标识核对\n\n"
            "正式系统将记录操作者、角色、患者、设备状态、参数前后值和事件 ID。",
            QMessageBox::Ok);
    });
    layout->addWidget(log);
    return rail;
}

QWidget *MainWindow::buildCenterWorkspace()
{
    auto *workspace = new QWidget;
    workspace->setObjectName("workspace");
    auto *layout = new QVBoxLayout(workspace);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_pages = new CompactStackedWidget;
    m_pages->addWidget(buildWorklistPage());
    m_pages->addWidget(buildProtocolPage());
    m_pages->addWidget(buildPositioningPage());
    m_pages->addWidget(buildReadyPage());
    m_pages->addWidget(buildAcquisitionPage());
    m_pages->addWidget(buildViewerPage());
    m_pages->addWidget(buildLockedPage());
    layout->addWidget(m_pages, 1);

    auto *actionBar = new QFrame;
    actionBar->setObjectName("actionBar");
    actionBar->setFixedHeight(84);
    auto *actionLayout = new QHBoxLayout(actionBar);
    actionLayout->setContentsMargins(24, 14, 24, 14);
    auto *safety = makeLabel("所有关键操作均写入审计日志", "pageSubtitle");
    actionLayout->addWidget(safety);
    actionLayout->addStretch();
    m_primaryAction = new QPushButton("确认患者身份");
    m_primaryAction->setObjectName("primaryAction");
    m_primaryAction->setCursor(Qt::PointingHandCursor);
    actionLayout->addWidget(m_primaryAction);
    layout->addWidget(actionBar);
    return workspace;
}

QWidget *MainWindow::buildWorklistPage()
{
    auto *page = new QWidget;
    page->setObjectName("page");
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 22, 24, 20);
    layout->setSpacing(16);
    layout->addWidget(makeLabel("患者确认", "pageTitle"));
    layout->addWidget(makeLabel("从今日工作列表选择患者；进入下一步前必须完成姓名与患者 ID 双标识核对。", "pageSubtitle"));

    auto *searchRow = new QHBoxLayout;
    auto *search = new QLineEdit;
    search->setPlaceholderText("搜索姓名 / 患者 ID / 检查号");
    search->setMinimumWidth(360);
    searchRow->addWidget(search);
    auto *filter = new QComboBox;
    filter->addItems({"今日全部", "待检查", "已完成"});
    filter->setMinimumWidth(140);
    searchRow->addWidget(filter);
    searchRow->addStretch();
    searchRow->addWidget(makeLabel("工作列表  6 项", "pageSubtitle"));
    layout->addLayout(searchRow);

    m_worklist = new QTableWidget(6, 7);
    m_worklist->setHorizontalHeaderLabels({"患者", "患者 ID", "性别 / 年龄", "检查号", "申请项目", "预约时间", "状态"});
    m_worklist->verticalHeader()->hide();
    m_worklist->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_worklist->setSelectionMode(QAbstractItemView::SingleSelection);
    m_worklist->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_worklist->setAlternatingRowColors(true);
    m_worklist->setShowGrid(false);
    m_worklist->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_worklist->verticalHeader()->setDefaultSectionSize(58);
    const QList<QStringList> rows = {
        {"李明", "P20260725018", "男 / 38", "ACC-0725-003", "全脊柱 AP/LAT", "10:30", "待检查"},
        {"王蕊", "P20260725017", "女 / 30", "ACC-0725-002", "下肢全长 AP", "10:15", "待检查"},
        {"赵安", "P20260725016", "男 / 51", "ACC-0725-001", "全脊柱 AP/LAT", "09:50", "已完成"},
        {"陈雨", "P20260725014", "女 / 25", "ACC-0725-014", "骨盆正位", "11:00", "待检查"},
        {"孙林", "P20260725012", "女 / 42", "ACC-0725-012", "下肢全长 AP", "11:20", "待检查"},
        {"高杰", "P20260725009", "男 / 36", "ACC-0725-009", "全脊柱 AP/LAT", "11:40", "待检查"}
    };
    for (int row = 0; row < rows.size(); ++row)
        for (int col = 0; col < rows.at(row).size(); ++col)
            m_worklist->setItem(row, col, new QTableWidgetItem(rows.at(row).at(col)));
    m_worklist->selectRow(0);
    layout->addWidget(m_worklist, 1);
    return page;
}

QWidget *MainWindow::buildProtocolPage()
{
    auto *page = new QWidget;
    page->setObjectName("page");
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 22, 24, 20);
    layout->setSpacing(16);
    layout->addWidget(makeLabel("选择检查协议", "pageTitle"));
    layout->addWidget(makeLabel("协议来自申请项目匹配结果。选择后核对扫描范围与剂量等级，再执行锁定。", "pageSubtitle"));
    layout->addWidget(callout("申请匹配", "申请项目“全脊柱 AP/LAT”已匹配 3 个设备协议。推荐项基于患者身高 172 cm、体重 65 kg。", "ok"));

    auto *cards = new QHBoxLayout;
    cards->setSpacing(16);
    auto *group = new QButtonGroup(page);
    group->setExclusive(true);
    const QList<QStringList> protocols = {
        {"全脊柱 AP/LAT", "成人 · 低剂量", "推荐", "83 / 102 kV", "预计 DAP 478.8 mGy·cm²"},
        {"全脊柱 AP/LAT", "成人 · 标准", "常规", "90 / 110 kV", "预计 DAP 562.4 mGy·cm²"},
        {"全脊柱 AP", "成人 · 单平面", "备选", "85 kV", "预计 DAP 241.7 mGy·cm²"}
    };
    for (int i = 0; i < protocols.size(); ++i) {
        auto *card = new QFrame;
        card->setObjectName("protocolCard");
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(18, 18, 18, 18);
        cardLayout->setSpacing(8);
        cardLayout->addWidget(makeLabel(protocols.at(i).at(2), "eyebrow"));
        cardLayout->addWidget(makeLabel(protocols.at(i).at(0), "protocolName"));
        cardLayout->addWidget(makeLabel(protocols.at(i).at(1), "protocolMeta"));
        cardLayout->addSpacing(12);
        cardLayout->addWidget(metricRow("曝光", protocols.at(i).at(3)));
        cardLayout->addWidget(metricRow("剂量预估", protocols.at(i).at(4)));
        cardLayout->addStretch();
        auto *choose = new QPushButton("选择此协议");
        choose->setObjectName("protocolSelect");
        choose->setCheckable(true);
        group->addButton(choose, i);
        cardLayout->addWidget(choose);
        cards->addWidget(card, 1);
    }
    connect(group, &QButtonGroup::idClicked, this, [this](int id) {
        m_protocolValue->setText(id == 0 ? "全脊柱 AP/LAT\n成人 · 低剂量" :
                                 id == 1 ? "全脊柱 AP/LAT\n成人 · 标准" : "全脊柱 AP\n成人 · 单平面");
        setState(WorkflowState::ProtocolSelected, "已选择协议，等待锁定");
    });
    layout->addLayout(cards, 1);
    return page;
}

QWidget *MainWindow::buildPositioningPage()
{
    auto *page = new QWidget;
    page->setObjectName("page");
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 18);
    layout->setSpacing(12);
    auto *titleRow = new QHBoxLayout;
    auto *titles = new QWidget;
    auto *titlesLayout = new QVBoxLayout(titles);
    titlesLayout->setContentsMargins(0, 0, 0, 0);
    titlesLayout->addWidget(makeLabel("体位与参考平面定位", "pageTitle"));
    titlesLayout->addWidget(makeLabel("调整扫描上下界与正中矢状面；橙色线表示当前采集范围。", "pageSubtitle"));
    titleRow->addWidget(titles);
    titleRow->addStretch();
    titleRow->addWidget(callout("定位相机", "实时 · 30 fps", "ok"));
    layout->addLayout(titleRow);
    layout->addWidget(new ScanCanvas(ScanCanvas::Mode::Positioning), 1);
    return page;
}

QWidget *MainWindow::buildReadyPage()
{
    auto *page = new QWidget;
    page->setObjectName("page");
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 28, 32, 24);
    layout->setSpacing(16);
    layout->addWidget(makeLabel("曝光准备", "pageTitle"));
    layout->addWidget(makeLabel("系统已完成参数校验。开始曝光前，请按清单进行最后一次现场确认。", "pageSubtitle"));
    layout->addWidget(callout("设备已就绪", "机架、探测器、急停、门控与高压发生器状态正常。协议参数未发生越界。", "ok"));

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(32);
    grid->setVerticalSpacing(4);
    grid->addWidget(makeLabel("安全确认", "sectionTitle"), 0, 0);
    grid->addWidget(makeLabel("曝光摘要", "sectionTitle"), 0, 1);
    auto *identity = new QCheckBox("患者姓名与患者 ID 已再次核对");
    identity->setObjectName("readyIdentity");
    auto *position = new QCheckBox("患者体位稳定，扫描范围无遮挡");
    position->setChecked(true);
    auto *room = new QCheckBox("机房已清场，门控联锁闭合");
    room->setChecked(true);
    grid->addWidget(identity, 1, 0);
    grid->addWidget(position, 2, 0);
    grid->addWidget(room, 3, 0);
    grid->addWidget(metricRow("协议", "全脊柱 AP/LAT · 低剂量", "已锁定"), 1, 1);
    grid->addWidget(metricRow("AP", "83 kV · 320 mA"), 2, 1);
    grid->addWidget(metricRow("LAT", "102 kV · 250 mA"), 3, 1);
    grid->addWidget(metricRow("预计 DAP", "478.8 mGy·cm²", "范围内"), 4, 1);
    layout->addLayout(grid);
    layout->addStretch();
    layout->addWidget(callout("曝光操作", "点击“开始曝光与采集”后，系统仍要求持续按住物理曝光开关；松开将立即中止高压。", "warning"));
    connect(identity, &QCheckBox::toggled, this, [this](bool) { updateWorkflowUi(); });
    return page;
}

QWidget *MainWindow::buildAcquisitionPage()
{
    auto *page = new QWidget;
    page->setObjectName("page");
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(20);
    auto *canvas = new ScanCanvas(ScanCanvas::Mode::Acquisition);
    canvas->setObjectName("acquisitionCanvas");
    layout->addWidget(canvas, 1);

    auto *monitor = new QWidget;
    monitor->setFixedWidth(270);
    auto *monitorLayout = new QVBoxLayout(monitor);
    monitorLayout->setContentsMargins(0, 8, 0, 8);
    monitorLayout->setSpacing(12);
    m_exposureBadge = makeLabel("X-RAY ON", "exposureBadge");
    m_exposureBadge->setAlignment(Qt::AlignCenter);
    monitorLayout->addWidget(m_exposureBadge);
    monitorLayout->addWidget(makeLabel("正在采集", "pageTitle"));
    monitorLayout->addWidget(makeLabel("请保持物理曝光开关按下，并持续观察患者。", "pageSubtitle"));
    m_acquisitionPercent = makeLabel("0%", "acquisitionPercent");
    monitorLayout->addWidget(m_acquisitionPercent);
    m_acquisitionBar = new QProgressBar;
    m_acquisitionBar->setRange(0, 100);
    m_acquisitionBar->setTextVisible(false);
    monitorLayout->addWidget(m_acquisitionBar);
    monitorLayout->addWidget(metricRow("当前平面", "AP"));
    monitorLayout->addWidget(metricRow("机架位置", "128.4 cm"));
    monitorLayout->addWidget(metricRow("探测器", "同步"));
    monitorLayout->addWidget(metricRow("实时 DAP", "126.3"));
    monitorLayout->addStretch();
    monitorLayout->addWidget(callout("中止条件", "松开曝光开关、门控断开或触发急停将立即停止高压并锁定流程。", "warning"));
    layout->addWidget(monitor);
    return page;
}

QWidget *MainWindow::buildViewerPage()
{
    auto *page = new QWidget;
    page->setObjectName("page");
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *tools = new QHBoxLayout;
    tools->setContentsMargins(4, 0, 4, 0);
    tools->setSpacing(6);
    tools->addWidget(iconButton(this, QStyle::SP_ArrowBack, "上一幅图像"));
    tools->addWidget(iconButton(this, QStyle::SP_ArrowForward, "下一幅图像"));
    tools->addSpacing(8);
    tools->addWidget(iconButton(this, QStyle::SP_BrowserReload, "恢复适窗"));
    tools->addWidget(iconButton(this, QStyle::SP_DesktopIcon, "1:1 显示"));
    tools->addStretch();
    tools->addWidget(makeLabel("2 幅图像  ·  AP 选中", "pageSubtitle"));
    layout->addLayout(tools);

    auto *images = new QHBoxLayout;
    images->setSpacing(6);
    images->addWidget(new ScanCanvas(ScanCanvas::Mode::Frontal), 1);
    images->addWidget(new ScanCanvas(ScanCanvas::Mode::Lateral), 1);
    layout->addLayout(images, 1);
    return page;
}

QWidget *MainWindow::buildLockedPage()
{
    auto *page = new QWidget;
    page->setObjectName("page");
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(56, 48, 56, 48);
    layout->setSpacing(16);
    layout->addStretch();
    layout->addWidget(makeLabel("流程已锁定", "pageTitle"));
    layout->addWidget(makeLabel("INTERLOCK-DOOR-021", "lockedCode"));
    layout->addWidget(callout("机房门控联锁断开", "系统已禁止高压与机架运动。患者和检查上下文已保留，不会丢失当前定位参数。", "danger"));
    layout->addWidget(makeLabel("处理步骤", "sectionTitle"));
    layout->addWidget(metricRow("01", "确认机房内人员安全"));
    layout->addWidget(metricRow("02", "关闭机房门并检查门控指示"));
    layout->addWidget(metricRow("03", "复核设备面板无其他报警"));
    layout->addStretch(2);
    return page;
}

QWidget *MainWindow::buildContextPage(const QString &title, const QString &body)
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(makeLabel(title, "sectionTitle"));
    auto *content = makeLabel(body, "contextBody");
    content->setWordWrap(true);
    layout->addWidget(content);
    layout->addStretch();
    return page;
}

QWidget *MainWindow::buildContextRail()
{
    auto *rail = new QFrame;
    rail->setObjectName("rightRail");
    rail->setFixedWidth(kContextWidth);
    auto *layout = new QVBoxLayout(rail);
    layout->setContentsMargins(20, 20, 20, 18);
    layout->setSpacing(12);
    layout->addWidget(makeLabel("上下文工具", "railCaption"));
    m_contextTitle = makeLabel("患者核对", "contextTitle");
    layout->addWidget(m_contextTitle);
    layout->addWidget(separator());

    m_contextPages = new CompactStackedWidget;

    auto *patient = buildContextPage("身份安全", "必须使用至少两个独立标识确认患者。不要使用床号或检查室作为患者标识。");
    patient->layout()->addWidget(callout("本次核对项", "姓名 · 患者 ID · 出生日期", "warning"));
    m_contextPages->addWidget(patient);

    auto *protocol = buildContextPage("协议摘要", "仅显示与申请项目匹配的协议。剂量等级、采集平面或扫描范围变化后必须重新锁定。");
    protocol->layout()->addWidget(callout("参数校验", "所有候选协议均在设备额定范围内。", "ok"));
    m_contextPages->addWidget(protocol);

    auto *positioning = new QWidget;
    auto *positioningLayout = new QVBoxLayout(positioning);
    positioningLayout->setContentsMargins(0, 0, 0, 0);
    positioningLayout->setSpacing(10);
    positioningLayout->addWidget(makeLabel("参考平面", "sectionTitle"));
    positioningLayout->addWidget(metricRow("正中矢状面", "0.0 cm", "居中"));
    positioningLayout->addWidget(metricRow("扫描上界", "166.0 cm"));
    auto *upper = new QSlider(Qt::Horizontal);
    upper->setRange(120, 190);
    upper->setValue(166);
    positioningLayout->addWidget(upper);
    positioningLayout->addWidget(metricRow("扫描下界", "66.0 cm"));
    auto *lower = new QSlider(Qt::Horizontal);
    lower->setRange(30, 100);
    lower->setValue(66);
    positioningLayout->addWidget(lower);
    positioningLayout->addWidget(separator());
    positioningLayout->addWidget(makeLabel("患者朝向", "sectionTitle"));
    auto *orientation = secondaryButton("面向设备 · 足先进");
    orientation->setCheckable(true);
    orientation->setChecked(true);
    positioningLayout->addWidget(orientation);
    positioningLayout->addStretch();
    positioningLayout->addWidget(callout("自动校验", "范围覆盖 T1 至骶骨，左右边界无截断风险。", "ok"));
    m_contextPages->addWidget(positioning);

    auto *ready = buildContextPage("设备联锁", "高压发生器、机架、探测器、门控和急停状态必须全部正常，任一状态变化都会撤销就绪。");
    ready->layout()->addWidget(callout("5 / 5 项通过", "协议锁定 · 参数范围 · 机架 · 门控 · 急停", "ok"));
    m_contextPages->addWidget(ready);

    auto *acquiring = buildContextPage("实时设备状态", "采集中禁止修改协议、曝光参数、体位方向和患者信息。异常会立即停止高压并保留事件记录。");
    acquiring->layout()->addWidget(callout("物理曝光开关", "按下 · 高压允许", "warning"));
    m_contextPages->addWidget(acquiring);

    auto *viewer = new QWidget;
    auto *viewerLayout = new QVBoxLayout(viewer);
    viewerLayout->setContentsMargins(0, 0, 0, 0);
    viewerLayout->setSpacing(8);
    auto *tabs = new QTabWidget;
    const QStringList tabNames = {"查看", "测量", "标注", "导出"};
    for (const QString &tabName : tabNames) {
        auto *tab = new QWidget;
        auto *tabLayout = new QVBoxLayout(tab);
        tabLayout->setContentsMargins(0, 16, 0, 0);
        tabLayout->setSpacing(8);
        if (tabName == "查看") {
            tabLayout->addWidget(makeLabel("窗宽 / 窗位", "sectionTitle"));
            tabLayout->addWidget(metricRow("窗宽", "40575"));
            tabLayout->addWidget(metricRow("窗位", "12767"));
            tabLayout->addWidget(secondaryButton("自动窗宽窗位"));
            tabLayout->addWidget(secondaryButton("反相"));
        } else if (tabName == "测量") {
            tabLayout->addWidget(secondaryButton("长度测量"));
            tabLayout->addWidget(secondaryButton("角度测量"));
            tabLayout->addWidget(secondaryButton("Cobb 角"));
            tabLayout->addWidget(secondaryButton("圆形 ROI"));
        } else if (tabName == "标注") {
            tabLayout->addWidget(secondaryButton("箭头"));
            tabLayout->addWidget(secondaryButton("文本"));
            tabLayout->addWidget(secondaryButton("左右标记"));
            tabLayout->addWidget(secondaryButton("删除选中标注"));
        } else {
            tabLayout->addWidget(callout("PACS", "ORTHO_PACS_01 · 已连接", "ok"));
            tabLayout->addWidget(secondaryButton("发送至 PACS"));
            tabLayout->addWidget(secondaryButton("导出 DICOM"));
            tabLayout->addWidget(secondaryButton("导出报告截图"));
        }
        tabLayout->addStretch();
        tabs->addTab(tab, tabName);
    }
    viewerLayout->addWidget(tabs);
    m_contextPages->addWidget(viewer);

    auto *locked = buildContextPage("异常处理", "先确保患者和现场安全，再排查联锁。禁止绕过门控或通过重启设备规避错误。");
    locked->layout()->addWidget(callout("操作已审计", "报警发生时间、设备状态和操作者将写入日志。", "danger"));
    m_contextPages->addWidget(locked);

    layout->addWidget(m_contextPages, 1);
    return rail;
}

void MainWindow::setState(WorkflowState state, const QString &auditMessage)
{
    m_state = state;
    if (!auditMessage.isEmpty())
        appendAudit(auditMessage);
    updateWorkflowUi();
}

void MainWindow::updateWorkflowUi()
{
    int page = 0;
    int context = 0;
    int activeStep = 0;
    QString stateText;
    QString hint;
    QString action;
    bool actionEnabled = true;

    switch (m_state) {
    case WorkflowState::NoPatient:
        stateText = "未选患者";
        hint = "选择工作列表中的患者并核对身份";
        action = "确认患者身份";
        page = context = 0;
        activeStep = 0;
        break;
    case WorkflowState::PatientConfirmed:
        stateText = "已选患者";
        hint = "患者身份已确认 · 请选择与申请匹配的检查协议";
        action = "请选择一个协议";
        actionEnabled = false;
        page = context = 1;
        activeStep = 1;
        break;
    case WorkflowState::ProtocolSelected:
        stateText = "已选协议";
        hint = "核对剂量与采集平面后锁定协议";
        action = "锁定协议并进入定位";
        page = context = 1;
        activeStep = 1;
        break;
    case WorkflowState::Positioning:
        stateText = "定位中";
        hint = "调整参考平面、扫描范围与患者朝向";
        action = "确认定位并执行校验";
        page = context = 2;
        activeStep = 2;
        break;
    case WorkflowState::Ready: {
        stateText = "就绪";
        hint = "完成最后身份核对后可开始曝光";
        action = "开始曝光与采集";
        page = context = 3;
        activeStep = 3;
        const auto *identity = findChild<QCheckBox *>("readyIdentity");
        actionEnabled = identity && identity->isChecked();
        break;
    }
    case WorkflowState::Acquiring:
        stateText = "采集中";
        hint = "X 射线曝光进行中 · 保持患者稳定";
        action = "采集进行中";
        actionEnabled = false;
        page = context = 4;
        activeStep = 4;
        break;
    case WorkflowState::Reviewing:
        stateText = "查看结果";
        hint = "检查图像质量，完成必要测量并发送结果";
        action = "完成检查并归档";
        page = context = 5;
        activeStep = 5;
        break;
    case WorkflowState::Locked:
        stateText = "异常 / 锁定";
        hint = "门控联锁断开 · 高压与机架运动已禁止";
        action = "复核联锁并解锁";
        page = context = 6;
        activeStep = -1;
        break;
    }

    m_pages->setCurrentIndex(page);
    m_contextPages->setCurrentIndex(context);
    m_stateLabel->setText(stateText);
    m_headerHint->setText(hint);
    m_contextTitle->setText(stateText == "异常 / 锁定" ? "安全锁定" :
                            page == 5 ? "图像工具" :
                            page == 2 ? "定位工具" :
                            page == 4 ? "采集监控" : stateText);
    m_primaryAction->setText(action);
    m_primaryAction->setEnabled(actionEnabled);
    m_interlockButton->setText(m_state == WorkflowState::Locked ? "联锁已触发" : "模拟联锁");

    for (auto *step : findChildren<QLabel *>("step")) {
        const int order = step->property("order").toInt();
        const QString status = activeStep < 0 ? "pending" : order < activeStep ? "done" : order == activeStep ? "active" : "pending";
        step->setProperty("status", status);
        step->style()->unpolish(step);
        step->style()->polish(step);
    }
}

void MainWindow::appendAudit(const QString &message)
{
    if (!m_auditLabel)
        return;
    m_auditLabel->setText(QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + message);
}

void MainWindow::beginAcquisition()
{
    m_acquisitionProgress = 0;
    m_acquisitionBar->setValue(0);
    m_acquisitionPercent->setText("0%");
    if (auto *canvas = findChild<QWidget *>("acquisitionCanvas"))
        static_cast<ScanCanvas *>(canvas)->setProgress(0);
    setState(WorkflowState::Acquiring, "操作者确认曝光，采集序列启动");
    m_acquisitionTimer->start();
}

void MainWindow::toggleInterlock()
{
    if (m_state == WorkflowState::Locked)
        return;
    m_stateBeforeLock = m_state;
    if (m_acquisitionTimer && m_acquisitionTimer->isActive())
        m_acquisitionTimer->stop();
    setState(WorkflowState::Locked, "严重报警：机房门控联锁断开，流程锁定");
}
