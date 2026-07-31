#include "workflowpages.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSlider>
#include <QTableWidget>
#include <QVBoxLayout>

#include <utility>

namespace {

QLabel *label(const QString &text, const char *name = nullptr)
{
    auto *result = new QLabel(text);
    if (name)
        result->setObjectName(name);
    result->setWordWrap(true);
    return result;
}

QFrame *line()
{
    auto *separator = new QFrame;
    separator->setObjectName("sectionLine");
    separator->setFrameShape(QFrame::HLine);
    return separator;
}

QPushButton *secondary(const QString &text)
{
    auto *button = new QPushButton(text);
    button->setObjectName("secondaryButton");
    return button;
}

QPushButton *primary(const QString &text, const char *name)
{
    auto *button = new QPushButton(text);
    button->setObjectName("primaryButton");
    button->setProperty("automationName", name);
    button->setAccessibleName(name);
    button->setMinimumWidth(260);
    return button;
}

QWidget *pageHeader(const QString &eyebrow, const QString &titleText, const QString &subtitle)
{
    auto *header = new QWidget;
    auto *layout = new QVBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(label(eyebrow, "workflowEyebrow"));
    layout->addWidget(label(titleText, "workflowTitle"));
    layout->addWidget(label(subtitle, "workflowSubtitle"));
    return header;
}

QWidget *infoRow(const QString &name, const QString &value, const QString &state = {})
{
    auto *row = new QWidget;
    row->setObjectName("workflowInfoRow");
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 9, 0, 9);
    layout->setSpacing(10);
    layout->addWidget(label(name, "infoName"));
    layout->addStretch();
    if (!state.isEmpty())
        layout->addWidget(label(state, state == "正常" || state == "通过" ? "okText" : "warningText"));
    layout->addWidget(label(value, "infoValue"));
    return row;
}

QWidget *deviceState(const QString &name, const QString &detail, const QString &value)
{
    auto *row = new QFrame;
    row->setObjectName("deviceCheckRow");
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(13, 11, 13, 11);
    auto *text = new QWidget;
    auto *textLayout = new QVBoxLayout(text);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);
    textLayout->addWidget(label(name, "sectionTitle"));
    textLayout->addWidget(label(detail, "mutedText"));
    layout->addWidget(text, 1);
    layout->addWidget(label(value, "okPill"));
    return row;
}

class ScanRangeCanvas final : public QWidget
{
public:
    explicit ScanRangeCanvas(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setObjectName("scanRangeCanvas");
        setMinimumSize(620, 520);
    }

    void setUpper(int value)
    {
        m_upper = value;
        update();
    }

    void setLower(int value)
    {
        m_lower = value;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#070a0c"));
        const QRectF left = QRectF(rect()).adjusted(14, 38, -width() / 2 - 5, -18);
        const QRectF right = QRectF(rect()).adjusted(width() / 2 + 5, 38, -14, -18);
        drawView(p, left, false, "AP 定位画面");
        drawView(p, right, true, "LAT 定位画面");
    }

private:
    void drawView(QPainter &p, const QRectF &area, bool lateral, const QString &titleText)
    {
        p.setPen(QPen(QColor("#343c41"), 1));
        p.setBrush(QColor("#050708"));
        p.drawRect(area);
        p.fillRect(QRectF(area.left(), area.top() - 28, area.width(), 28), QColor("#1a1f22"));
        p.setPen(QColor("#9ca7ac"));
        p.drawText(QRectF(area.left() + 9, area.top() - 28, area.width() - 18, 28), Qt::AlignVCenter | Qt::AlignLeft, titleText);

        p.save();
        p.setClipRect(area);
        const qreal scale = qMin(area.width() / 360.0, area.height() / 720.0);
        p.translate(area.center());
        p.scale(scale, scale);
        const QColor soft(150, 163, 168, 40);
        const QColor bone(221, 228, 229, 130);
        QPainterPath body;
        if (lateral) {
            body.moveTo(20, -340);
            body.cubicTo(88, -328, 90, -265, 55, -228);
            body.cubicTo(105, -170, 106, -30, 68, 80);
            body.cubicTo(48, 150, 76, 238, 34, 340);
            body.lineTo(-64, 340);
            body.cubicTo(-84, 200, -70, 125, -91, 25);
            body.cubicTo(-120, -105, -80, -194, -40, -232);
            body.cubicTo(-63, -290, -47, -330, 20, -340);
        } else {
            body.moveTo(0, -345);
            body.cubicTo(-70, -338, -67, -272, -43, -230);
            body.cubicTo(-118, -198, -126, -70, -92, 42);
            body.cubicTo(-72, 115, -90, 205, -98, 340);
            body.lineTo(98, 340);
            body.cubicTo(90, 205, 72, 115, 92, 42);
            body.cubicTo(126, -70, 118, -198, 43, -230);
            body.cubicTo(67, -272, 70, -338, 0, -345);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(soft);
        p.drawPath(body);
        p.setPen(QPen(bone, 5));
        p.setBrush(QColor(220, 226, 227, 48));
        p.drawEllipse(lateral ? QRectF(-28, -335, 94, 104) : QRectF(-47, -336, 94, 104));
        if (lateral) {
            QPainterPath spine;
            spine.moveTo(14, -225);
            spine.cubicTo(-8, -115, 34, -5, 5, 120);
            spine.cubicTo(-8, 185, 17, 255, 2, 320);
            p.drawPath(spine);
        } else {
            p.drawLine(QPointF(0, -225), QPointF(0, 310));
            for (int i = 0; i < 9; ++i) {
                const qreal y = -190 + i * 28;
                p.drawArc(QRectF(-92, y, 92, 82), 20 * 16, 140 * 16);
                p.drawArc(QRectF(0, y, 92, 82), 20 * 16, 140 * 16);
            }
            p.drawEllipse(QRectF(-70, 200, 140, 86));
        }
        p.restore();

        const qreal upperY = area.bottom() - area.height() * ((m_upper - 40) / 150.0);
        const qreal lowerY = area.bottom() - area.height() * ((m_lower - 40) / 150.0);
        p.fillRect(QRectF(area.left(), upperY, area.width(), lowerY - upperY), QColor(220, 128, 49, 16));
        p.setPen(QPen(QColor("#df8536"), 2));
        p.drawLine(QPointF(area.left(), upperY), QPointF(area.right(), upperY));
        p.drawLine(QPointF(area.left(), lowerY), QPointF(area.right(), lowerY));
        p.setPen(QColor("#e69a58"));
        p.drawText(QPointF(area.left() + 8, upperY - 6), QString("上界 %1 cm").arg(m_upper));
        p.drawText(QPointF(area.left() + 8, lowerY + 17), QString("下界 %1 cm").arg(m_lower));
    }

    int m_upper = 166;
    int m_lower = 66;
};

} // namespace

PatientConfirmationPage::PatientConfirmationPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("workflowPage");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 24, 32, 24);
    layout->setSpacing(16);
    layout->addWidget(pageHeader("STEP 01 / 04", "确认患者信息", "选择当前检查，并使用姓名与患者 ID 完成双标识核对。"));

    auto *content = new QHBoxLayout;
    content->setSpacing(22);
    auto *worklist = new QWidget;
    auto *worklistLayout = new QVBoxLayout(worklist);
    worklistLayout->setContentsMargins(0, 0, 0, 0);
    worklistLayout->setSpacing(10);
    auto *search = new QLineEdit;
    search->setPlaceholderText("搜索患者姓名 / ID / 检查号");
    worklistLayout->addWidget(search);
    auto *table = new QTableWidget(5, 5);
    table->setHorizontalHeaderLabels({"患者", "患者 ID", "检查项目", "预约时间", "状态"});
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    const QList<QStringList> rows = {
        {"李明", "P20260725018", "全脊柱 AP/LAT", "10:30", "待检查"},
        {"王蕊", "P20260725017", "下肢全长 AP", "10:15", "待检查"},
        {"赵安", "P20260725016", "全脊柱 AP/LAT", "09:50", "已完成"},
        {"陈雨", "P20260725014", "骨盆正位", "11:00", "待检查"},
        {"孙林", "P20260725012", "下肢全长 AP", "11:20", "待检查"}
    };
    for (int row = 0; row < rows.size(); ++row)
        for (int column = 0; column < rows.at(row).size(); ++column)
            table->setItem(row, column, new QTableWidgetItem(rows.at(row).at(column)));
    table->selectRow(0);
    worklistLayout->addWidget(table, 1);
    content->addWidget(worklist, 3);

    auto *details = new QFrame;
    details->setObjectName("workflowSidePanel");
    details->setMinimumWidth(360);
    details->setMaximumWidth(430);
    auto *detailsLayout = new QVBoxLayout(details);
    detailsLayout->setContentsMargins(20, 18, 20, 18);
    detailsLayout->setSpacing(8);
    detailsLayout->addWidget(label("待核对患者", "panelEyebrow"));
    detailsLayout->addWidget(label("李明", "workflowPatientName"));
    detailsLayout->addWidget(infoRow("患者 ID", "P20260725018"));
    detailsLayout->addWidget(infoRow("出生日期", "1988-06-16"));
    detailsLayout->addWidget(infoRow("性别 / 年龄", "男 / 38 岁"));
    detailsLayout->addWidget(infoRow("检查号", "ACC-2026-0725-003"));
    detailsLayout->addWidget(infoRow("申请项目", "全脊柱 AP/LAT"));
    detailsLayout->addWidget(line());
    detailsLayout->addWidget(label("双标识核对", "sectionTitle"));
    auto *nameCheck = new QCheckBox("患者已口述或确认姓名");
    auto *idCheck = new QCheckBox("腕带患者 ID 与屏幕一致");
    nameCheck->setChecked(true);
    idCheck->setChecked(true);
    detailsLayout->addWidget(nameCheck);
    detailsLayout->addWidget(idCheck);
    detailsLayout->addWidget(label("不得使用床号或检查室作为患者标识。", "warningText"));
    detailsLayout->addStretch();
    content->addWidget(details);
    layout->addLayout(content, 1);

    auto *footer = new QHBoxLayout;
    footer->addWidget(label("今日工作列表 · 5 项", "mutedText"));
    footer->addStretch();
    auto *next = primary("确认患者并继续", "patientNextButton");
    connect(next, &QPushButton::clicked, this, [this] {
        if (m_continueCallback)
            m_continueCallback();
    });
    footer->addWidget(next);
    layout->addLayout(footer);
}

void PatientConfirmationPage::setContinueCallback(std::function<void()> callback)
{
    m_continueCallback = std::move(callback);
}

SafetyCheckPage::SafetyCheckPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("workflowPage");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 26, 40, 24);
    layout->setSpacing(18);
    layout->addWidget(pageHeader("STEP 02 / 04", "联锁确认与设备检查", "曝光前确认安全回路、设备状态和机房条件。异常项会阻止进入下一步。"));

    auto *content = new QHBoxLayout;
    content->setSpacing(28);
    auto *devices = new QWidget;
    auto *deviceLayout = new QVBoxLayout(devices);
    deviceLayout->setContentsMargins(0, 0, 0, 0);
    deviceLayout->setSpacing(9);
    deviceLayout->addWidget(label("设备联锁", "sectionTitle"));
    deviceLayout->addWidget(deviceState("机房门控", "门控回路闭合", "正常"));
    deviceLayout->addWidget(deviceState("急停回路", "机架与控制台急停均释放", "正常"));
    deviceLayout->addWidget(deviceState("高压发生器", "自检通过，曝光许可待命", "正常"));
    deviceLayout->addWidget(deviceState("机架运动", "编码器与运动范围正常", "正常"));
    deviceLayout->addWidget(deviceState("探测器同步", "AP / LAT 探测器在线", "正常"));
    deviceLayout->addWidget(deviceState("剂量监测", "DAP 计量单元已校准", "正常"));
    content->addWidget(devices, 3);

    auto *checks = new QFrame;
    checks->setObjectName("workflowSidePanel");
    checks->setMinimumWidth(380);
    checks->setMaximumWidth(460);
    auto *checkLayout = new QVBoxLayout(checks);
    checkLayout->setContentsMargins(20, 18, 20, 18);
    checkLayout->setSpacing(10);
    checkLayout->addWidget(label("现场检查", "panelEyebrow"));
    checkLayout->addWidget(label("曝光条件", "panelTitle"));
    const QStringList checklist = {
        "患者身份已经确认", "患者可保持站立体位", "机房内无无关人员",
        "定位区域无金属遮挡", "门控指示与屏幕状态一致"
    };
    for (const QString &text : checklist) {
        auto *item = new QCheckBox(text);
        item->setChecked(true);
        checkLayout->addWidget(item);
    }
    checkLayout->addWidget(line());
    checkLayout->addWidget(infoRow("协议参数", "范围内", "通过"));
    checkLayout->addWidget(infoRow("预计 DAP", "478.8 mGy·cm²", "通过"));
    checkLayout->addWidget(infoRow("联锁结果", "6 / 6", "通过"));
    checkLayout->addStretch();
    checkLayout->addWidget(label("任何联锁变化都会撤销本页检查结果。", "warningText"));
    content->addWidget(checks);
    layout->addLayout(content, 1);

    auto *footer = new QHBoxLayout;
    auto *back = secondary("返回患者确认");
    connect(back, &QPushButton::clicked, this, [this] { if (m_backCallback) m_backCallback(); });
    footer->addWidget(back);
    footer->addStretch();
    auto *next = primary("完成联锁检查", "safetyNextButton");
    connect(next, &QPushButton::clicked, this, [this] { if (m_continueCallback) m_continueCallback(); });
    footer->addWidget(next);
    layout->addLayout(footer);
}

void SafetyCheckPage::setBackCallback(std::function<void()> callback) { m_backCallback = std::move(callback); }
void SafetyCheckPage::setContinueCallback(std::function<void()> callback) { m_continueCallback = std::move(callback); }

ScanRangePage::ScanRangePage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("workflowPage");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 22, 28, 22);
    layout->setSpacing(14);
    layout->addWidget(pageHeader("STEP 03 / 04", "设置扫描范围", "通过 AP / LAT 定位画面设置采集上界与下界，并核对患者朝向和参考平面。"));

    auto *content = new QHBoxLayout;
    content->setSpacing(20);
    auto *canvas = new ScanRangeCanvas;
    content->addWidget(canvas, 1);

    auto *controls = new QFrame;
    controls->setObjectName("workflowSidePanel");
    controls->setMinimumWidth(340);
    controls->setMaximumWidth(410);
    auto *controlLayout = new QVBoxLayout(controls);
    controlLayout->setContentsMargins(20, 18, 20, 18);
    controlLayout->setSpacing(10);
    controlLayout->addWidget(label("定位参数", "panelEyebrow"));
    controlLayout->addWidget(label("全脊柱 AP / LAT", "panelTitle"));
    auto *orientation = new QButtonGroup(controls);
    orientation->setExclusive(true);
    auto *front = secondary("面向设备");
    auto *back = secondary("背向设备");
    front->setCheckable(true);
    back->setCheckable(true);
    front->setChecked(true);
    orientation->addButton(front);
    orientation->addButton(back);
    auto *orientationRow = new QHBoxLayout;
    orientationRow->addWidget(front);
    orientationRow->addWidget(back);
    controlLayout->addLayout(orientationRow);
    controlLayout->addWidget(line());

    auto *upperValue = label("166 cm", "rangeValue");
    auto *upperRow = new QHBoxLayout;
    upperRow->addWidget(label("扫描上界", "sectionTitle"));
    upperRow->addStretch();
    upperRow->addWidget(upperValue);
    controlLayout->addLayout(upperRow);
    auto *upper = new QSlider(Qt::Horizontal);
    upper->setRange(130, 190);
    upper->setValue(166);
    controlLayout->addWidget(upper);

    auto *lowerValue = label("66 cm", "rangeValue");
    auto *lowerRow = new QHBoxLayout;
    lowerRow->addWidget(label("扫描下界", "sectionTitle"));
    lowerRow->addStretch();
    lowerRow->addWidget(lowerValue);
    controlLayout->addLayout(lowerRow);
    auto *lower = new QSlider(Qt::Horizontal);
    lower->setRange(40, 100);
    lower->setValue(66);
    controlLayout->addWidget(lower);
    connect(upper, &QSlider::valueChanged, this, [canvas, upperValue](int value) {
        upperValue->setText(QString::number(value) + " cm");
        canvas->setUpper(value);
    });
    connect(lower, &QSlider::valueChanged, this, [canvas, lowerValue](int value) {
        lowerValue->setText(QString::number(value) + " cm");
        canvas->setLower(value);
    });
    controlLayout->addWidget(line());
    controlLayout->addWidget(infoRow("覆盖范围", "T1 → 骶骨", "通过"));
    controlLayout->addWidget(infoRow("正中矢状面", "0.0 cm", "通过"));
    controlLayout->addWidget(infoRow("左右截断风险", "无", "通过"));
    auto *autoRange = secondary("恢复协议推荐范围");
    connect(autoRange, &QPushButton::clicked, this, [upper, lower] {
        upper->setValue(166);
        lower->setValue(66);
    });
    controlLayout->addWidget(autoRange);
    controlLayout->addStretch();
    controlLayout->addWidget(label("进入影像编辑后仍可查看原始定位参数，但修改范围需要重新执行联锁检查。", "warningText"));
    content->addWidget(controls);
    layout->addLayout(content, 1);

    auto *footer = new QHBoxLayout;
    auto *previous = secondary("返回联锁检查");
    connect(previous, &QPushButton::clicked, this, [this] { if (m_backCallback) m_backCallback(); });
    footer->addWidget(previous);
    footer->addStretch();
    auto *next = primary("确认范围并进入影像编辑", "rangeNextButton");
    connect(next, &QPushButton::clicked, this, [this] { if (m_continueCallback) m_continueCallback(); });
    footer->addWidget(next);
    layout->addLayout(footer);
}

void ScanRangePage::setBackCallback(std::function<void()> callback) { m_backCallback = std::move(callback); }
void ScanRangePage::setContinueCallback(std::function<void()> callback) { m_continueCallback = std::move(callback); }
