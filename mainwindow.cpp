#include "mainwindow.h"

#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDateEdit>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

constexpr auto kOrange = "#f28a13";
constexpr auto kDarkOrange = "#c96700";
constexpr auto kPanel = "#dfe3e8";
constexpr auto kBorder = "#aeb5bd";

QFrame *lineSeparator(Qt::Orientation orientation)
{
    auto *line = new QFrame;
    line->setFrameShape(orientation == Qt::Horizontal ? QFrame::HLine : QFrame::VLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

QLabel *sectionTitle(const QString &text)
{
    auto *label = new QLabel(text);
    label->setObjectName("sectionTitle");
    return label;
}

QPushButton *orangeButton(const QString &text)
{
    auto *button = new QPushButton(text);
    button->setObjectName("orangeButton");
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QPushButton *toolButton(const QString &text, const QString &tip = {})
{
    auto *button = new QPushButton(text);
    button->setObjectName("toolButton");
    button->setMinimumSize(46, 34);
    button->setToolTip(tip.isEmpty() ? text : tip);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QWidget *labeledField(const QString &labelText, QWidget *field)
{
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto *label = new QLabel(labelText);
    label->setObjectName("fieldLabel");
    layout->addWidget(label);
    layout->addWidget(field);
    return widget;
}

class RadiographWidget final : public QWidget
{
public:
    enum class View { Frontal, Lateral, Positioning };

    explicit RadiographWidget(View view, QWidget *parent = nullptr)
        : QWidget(parent), m_view(view)
    {
        setMinimumSize(260, 420);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setActive(bool active)
    {
        m_active = active;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QRectF outer = rect().adjusted(2, 2, -2, -2);
        QLinearGradient background(outer.topLeft(), outer.bottomRight());
        background.setColorAt(0.0, QColor(24, 28, 34));
        background.setColorAt(0.55, QColor(8, 11, 15));
        background.setColorAt(1.0, QColor(20, 23, 28));
        p.fillRect(outer, background);

        drawFilm(p, outer.adjusted(18, 18, -28, -18));

        if (m_view != View::Positioning) {
            p.setPen(QPen(m_active ? QColor(220, 55, 39) : QColor(112, 119, 128), m_active ? 3 : 1));
            p.drawRect(outer);
            drawOverlay(p, outer);
        } else {
            p.setPen(QPen(QColor(213, 73, 41), 2));
            p.drawRect(outer.adjusted(6, 76, -7, -62));
            p.setPen(QColor(238, 111, 53));
            for (int y = 32; y < height() - 20; y += 34) {
                p.drawLine(5, y, 13, y);
                p.drawLine(width() - 14, y, width() - 6, y);
            }
        }
    }

private:
    void drawFilm(QPainter &p, const QRectF &r)
    {
        p.save();
        p.setClipRect(r);
        const QColor bone(213, 221, 226, 115);
        const QColor soft(155, 170, 179, 38);
        p.translate(r.left(), r.top());
        p.scale(r.width() / 420.0, r.height() / 760.0);

        if (m_view == View::Lateral) {
            p.setPen(QPen(soft, 65));
            QPainterPath body;
            body.moveTo(230, 20);
            body.cubicTo(320, 55, 312, 140, 270, 176);
            body.cubicTo(330, 235, 330, 365, 286, 478);
            body.cubicTo(268, 540, 312, 640, 250, 744);
            body.lineTo(130, 744);
            body.cubicTo(110, 590, 128, 530, 105, 430);
            body.cubicTo(70, 280, 120, 188, 165, 156);
            body.cubicTo(132, 105, 147, 42, 230, 20);
            p.setBrush(soft);
            p.drawPath(body);

            p.setBrush(bone);
            p.setPen(QPen(QColor(235, 240, 242, 120), 5));
            p.drawEllipse(QRectF(155, 25, 112, 130));
            QPainterPath spine;
            spine.moveTo(220, 145);
            spine.cubicTo(185, 250, 252, 350, 208, 485);
            spine.cubicTo(185, 555, 225, 620, 204, 690);
            p.drawPath(spine);
            for (int y = 155; y < 650; y += 27) {
                const int x = 215 + static_cast<int>(18 * std::sin(y / 65.0));
                p.drawRoundedRect(QRectF(x - 15, y, 42, 13), 4, 4);
            }
            p.setPen(QPen(bone, 5));
            for (int i = 0; i < 10; ++i) {
                const qreal y = 185 + i * 26;
                p.drawArc(QRectF(95, y, 210, 120), 15 * 16, 135 * 16);
            }
            p.drawEllipse(QRectF(145, 620, 135, 94));
        } else {
            p.setBrush(soft);
            p.setPen(Qt::NoPen);
            QPainterPath body;
            body.moveTo(210, 20);
            body.cubicTo(128, 24, 137, 120, 165, 153);
            body.cubicTo(84, 184, 72, 325, 113, 440);
            body.cubicTo(130, 488, 112, 552, 96, 744);
            body.lineTo(324, 744);
            body.cubicTo(308, 552, 290, 488, 307, 440);
            body.cubicTo(348, 325, 336, 184, 255, 153);
            body.cubicTo(283, 120, 292, 24, 210, 20);
            p.drawPath(body);

            p.setBrush(bone);
            p.setPen(QPen(QColor(235, 240, 242, 120), 4));
            p.drawEllipse(QRectF(158, 22, 104, 122));
            p.drawLine(QPointF(210, 143), QPointF(210, 654));
            for (int y = 150; y < 655; y += 25)
                p.drawRoundedRect(QRectF(194, y, 32, 12), 4, 4);

            p.setPen(QPen(bone, 5));
            for (int i = 0; i < 10; ++i) {
                const qreal y = 175 + i * 27;
                const qreal width = 105 - i * 3;
                p.drawArc(QRectF(210 - width, y, width, 90), 20 * 16, 140 * 16);
                p.drawArc(QRectF(210, y, width, 90), 20 * 16, 140 * 16);
            }
            p.drawLine(QPointF(125, 163), QPointF(210, 195));
            p.drawLine(QPointF(295, 163), QPointF(210, 195));
            p.drawEllipse(QRectF(132, 595, 156, 108));
            p.drawLine(QPointF(159, 686), QPointF(145, 758));
            p.drawLine(QPointF(261, 686), QPointF(275, 758));
        }
        p.restore();
    }

    void drawOverlay(QPainter &p, const QRectF &r)
    {
        p.save();
        p.setPen(QColor(231, 235, 239));
        QFont font = p.font();
        font.setPixelSize(13);
        p.setFont(font);
        const QString info = QStringLiteral("LI MING\nP20260725018\nACC-2026-0725-003\n25/07/2026  10:32");
        p.drawText(r.adjusted(12, 10, -10, -10), Qt::AlignLeft | Qt::AlignTop, info);

        font.setBold(true);
        font.setPixelSize(15);
        p.setFont(font);
        p.drawText(QRectF(r.left() + 9, r.center().y(), 25, 25), m_view == View::Lateral ? "A" : "R");

        p.setPen(QPen(QColor(215, 179, 81), 1));
        for (int y = 65; y < height() - 26; y += 12) {
            const int length = (y % 60 == 5) ? 12 : 6;
            p.drawLine(width() - 18 - length, y, width() - 18, y);
        }
        p.restore();
    }

    View m_view;
    bool m_active = false;
};

class ReferenceWidget final : public QWidget
{
public:
    explicit ReferenceWidget(bool orientation = false, QWidget *parent = nullptr)
        : QWidget(parent), m_orientation(orientation)
    {
        setMinimumHeight(170);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = rect().adjusted(22, 8, -22, -8);
        QLinearGradient g(r.topLeft(), r.bottomRight());
        g.setColorAt(0, QColor(248, 164, 33));
        g.setColorAt(1, QColor(178, 82, 0));
        p.setBrush(g);
        p.setPen(QPen(QColor(158, 92, 21), 2));
        p.drawRoundedRect(r, 8, 8);
        p.setPen(QPen(QColor(255, 238, 194), 3));

        if (m_orientation) {
            p.drawText(r.adjusted(10, 6, -10, -6), Qt::AlignTop | Qt::AlignHCenter, "R");
            p.drawText(r.adjusted(10, 6, -10, -6), Qt::AlignVCenter | Qt::AlignLeft, "A");
            p.drawText(r.adjusted(10, 6, -10, -6), Qt::AlignVCenter | Qt::AlignRight, "P");
            QPainterPath feet;
            feet.addEllipse(QRectF(r.center().x() - 54, r.center().y() - 4, 40, 78));
            feet.addEllipse(QRectF(r.center().x() + 14, r.center().y() - 4, 40, 78));
            p.drawPath(feet);
        } else {
            p.drawLine(QPointF(r.left() + 22, r.center().y()), QPointF(r.right() - 22, r.center().y()));
            p.drawLine(QPointF(r.center().x(), r.top() + 22), QPointF(r.center().x(), r.bottom() - 22));
            p.drawEllipse(r.center(), 36, 36);
            for (int i = -4; i <= 4; ++i) {
                p.drawPoint(QPointF(r.center().x() + i * 17, r.center().y() - 55));
                p.drawPoint(QPointF(r.center().x() + i * 17, r.center().y() + 55));
            }
        }
    }

private:
    bool m_orientation;
};

QWidget *makePatientPage()
{
    auto *page = new QWidget;
    page->setObjectName("workspace");
    auto *root = new QHBoxLayout(page);
    root->setContentsMargins(18, 16, 18, 18);
    root->setSpacing(18);

    auto *formCard = new QFrame;
    formCard->setObjectName("card");
    formCard->setMinimumWidth(390);
    formCard->setMaximumWidth(520);
    auto *form = new QVBoxLayout(formCard);
    form->setContentsMargins(22, 18, 22, 20);
    form->setSpacing(12);

    auto *logo = new QLabel("EOS");
    logo->setObjectName("logo");
    form->addWidget(logo);
    form->addWidget(sectionTitle("Patient information"));

    auto *fields = new QGridLayout;
    fields->setHorizontalSpacing(14);
    fields->setVerticalSpacing(11);

    auto *lastName = new QLineEdit("Li");
    auto *firstName = new QLineEdit("Ming");
    auto *patientId = new QLineEdit("P20260725018");
    auto *accession = new QLineEdit("ACC-2026-0725-003");
    auto *birth = new QDateEdit(QDate(1988, 6, 16));
    birth->setDisplayFormat("yyyy-MM-dd");
    birth->setCalendarPopup(true);
    auto *sex = new QComboBox;
    sex->addItems({"Male", "Female", "Other"});
    auto *height = new QSpinBox;
    height->setRange(30, 250);
    height->setValue(172);
    height->setSuffix(" cm");
    auto *weight = new QSpinBox;
    weight->setRange(1, 300);
    weight->setValue(65);
    weight->setSuffix(" kg");

    fields->addWidget(labeledField("Last name *", lastName), 0, 0);
    fields->addWidget(labeledField("First name *", firstName), 0, 1);
    fields->addWidget(labeledField("Patient ID *", patientId), 1, 0);
    fields->addWidget(labeledField("Accession number", accession), 1, 1);
    fields->addWidget(labeledField("Date of birth *", birth), 2, 0);
    fields->addWidget(labeledField("Sex *", sex), 2, 1);
    fields->addWidget(labeledField("Height", height), 3, 0);
    fields->addWidget(labeledField("Weight", weight), 3, 1);
    form->addLayout(fields);

    auto *physician = new QLineEdit("Dr. Chen");
    form->addWidget(labeledField("Referring physician", physician));
    auto *study = new QComboBox;
    study->addItems({"Full body AP/LAT", "Spine AP/LAT", "Lower limbs AP"});
    form->addWidget(labeledField("Requested procedure", study));
    auto *comment = new QLineEdit("Standing examination");
    form->addWidget(labeledField("Clinical notes", comment));
    form->addStretch();

    auto *formActions = new QHBoxLayout;
    formActions->addWidget(orangeButton("New patient"));
    formActions->addStretch();
    formActions->addWidget(orangeButton("Save patient"));
    form->addLayout(formActions);

    auto *listCard = new QFrame;
    listCard->setObjectName("card");
    auto *listLayout = new QVBoxLayout(listCard);
    listLayout->setContentsMargins(18, 18, 18, 18);
    listLayout->setSpacing(12);

    auto *listTop = new QHBoxLayout;
    listTop->addWidget(sectionTitle("Patient worklist"));
    listTop->addStretch();
    auto *search = new QLineEdit;
    search->setPlaceholderText("Patient name / ID / accession number");
    search->setMinimumWidth(330);
    listTop->addWidget(search);
    listTop->addWidget(orangeButton("Search"));
    listTop->addWidget(orangeButton("Refresh"));
    listLayout->addLayout(listTop);

    auto *table = new QTableWidget(10, 7);
    table->setObjectName("worklist");
    table->setHorizontalHeaderLabels({"Patient name", "Patient ID", "Sex", "Birth date", "Accession", "Procedure", "Status"});
    table->verticalHeader()->hide();
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    const QList<QStringList> rows = {
        {"Li Ming", "P20260725018", "M", "1988-06-16", "ACC-0725-003", "Full body AP/LAT", "Scheduled"},
        {"Wang Rui", "P20260725017", "F", "1996-02-08", "ACC-0725-002", "Spine AP/LAT", "Ready"},
        {"Zhao An", "P20260725016", "M", "1974-11-23", "ACC-0725-001", "Lower limbs AP", "Completed"},
        {"Chen Yu", "P20260724031", "F", "2001-04-12", "ACC-0724-031", "Full body AP/LAT", "Completed"},
        {"Sun Lin", "P20260724030", "F", "1983-09-20", "ACC-0724-030", "Spine AP/LAT", "Completed"},
        {"Gao Jie", "P20260724029", "M", "1990-01-03", "ACC-0724-029", "Full body AP/LAT", "Completed"},
        {"He Ping", "P20260724028", "M", "1965-07-18", "ACC-0724-028", "Spine AP/LAT", "Completed"},
        {"Liu Xin", "P20260724027", "F", "2004-12-29", "ACC-0724-027", "Lower limbs AP", "Completed"},
        {"Xu Yan", "P20260724026", "F", "1979-05-08", "ACC-0724-026", "Full body AP/LAT", "Completed"},
        {"Zhou Kai", "P20260724025", "M", "1987-10-14", "ACC-0724-025", "Spine AP/LAT", "Completed"}
    };
    for (int row = 0; row < rows.size(); ++row) {
        for (int col = 0; col < rows.at(row).size(); ++col)
            table->setItem(row, col, new QTableWidgetItem(rows.at(row).at(col)));
    }
    table->selectRow(0);
    listLayout->addWidget(table, 1);

    auto *listActions = new QHBoxLayout;
    listActions->addWidget(orangeButton("Import worklist"));
    listActions->addStretch();
    listActions->addWidget(orangeButton("Edit"));
    listActions->addWidget(orangeButton("Delete"));
    auto *select = orangeButton("Select patient  >");
    select->setMinimumWidth(160);
    listActions->addWidget(select);
    listLayout->addLayout(listActions);

    root->addWidget(formCard, 3);
    root->addWidget(listCard, 7);
    return page;
}

QWidget *makeAcquisitionLeftPanel()
{
    auto *scroll = new QScrollArea;
    scroll->setObjectName("sideScroll");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumWidth(295);
    scroll->setMaximumWidth(335);

    auto *panel = new QWidget;
    panel->setObjectName("sidePanel");
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto *logo = new QLabel("EOS");
    logo->setObjectName("logoSmall");
    layout->addWidget(logo);
    auto *user = new QLabel("EOS_ADMIN");
    user->setObjectName("userTag");
    user->setAlignment(Qt::AlignCenter);
    layout->addWidget(user);
    layout->addWidget(orangeButton("New patient"));

    auto *patient = new QFrame;
    patient->setObjectName("subCard");
    auto *patientLayout = new QFormLayout(patient);
    patientLayout->addRow("Last name", new QLabel("Li"));
    patientLayout->addRow("First name", new QLabel("Ming"));
    patientLayout->addRow("Patient ID", new QLabel("P20260725018"));
    layout->addWidget(patient);

    layout->addWidget(sectionTitle("Select planes"));
    auto *planes = new QHBoxLayout;
    planes->addStretch();
    planes->addWidget(toolButton("AP", "Frontal plane"));
    planes->addWidget(toolButton("LAT", "Lateral plane"));
    planes->addStretch();
    layout->addLayout(planes);

    layout->addWidget(sectionTitle("Morphotype"));
    auto *types = new QHBoxLayout;
    types->addWidget(toolButton("Slim"));
    types->addWidget(toolButton("Medium"));
    types->addWidget(toolButton("Large"));
    layout->addLayout(types);

    layout->addWidget(sectionTitle("Anatomical area"));
    auto *protocol = new QComboBox;
    protocol->addItems({"Fast Full body", "Spine", "Lower limbs"});
    layout->addWidget(protocol);
    layout->addWidget(orangeButton("μDose"));

    auto *density = new QHBoxLayout;
    auto *frontDensity = new QComboBox;
    frontDensity->addItems({"Soft", "Normal", "Hard"});
    frontDensity->setCurrentText("Hard");
    auto *sideDensity = new QComboBox;
    sideDensity->addItems({"Soft", "Normal", "Hard"});
    sideDensity->setCurrentText("Hard");
    density->addWidget(frontDensity);
    density->addWidget(sideDensity);
    layout->addLayout(density);

    layout->addWidget(sectionTitle("Scan speed"));
    auto *speed = new QSlider(Qt::Horizontal);
    speed->setRange(1, 8);
    speed->setValue(2);
    speed->setTickInterval(1);
    speed->setTickPosition(QSlider::TicksAbove);
    layout->addWidget(speed);
    auto *speedLabels = new QHBoxLayout;
    speedLabels->addWidget(new QLabel("Fast"));
    speedLabels->addStretch();
    speedLabels->addWidget(new QLabel("Slow"));
    layout->addLayout(speedLabels);

    auto makeExposure = [](const QString &title, int kv, int ma, const QString &dose) {
        auto *box = new QGroupBox(title);
        auto *grid = new QGridLayout(box);
        grid->addWidget(new QLabel("kV"), 0, 0);
        auto *kvSpin = new QSpinBox;
        kvSpin->setRange(40, 150);
        kvSpin->setValue(kv);
        grid->addWidget(kvSpin, 0, 1);
        grid->addWidget(new QLabel("mA"), 1, 0);
        auto *maSpin = new QSpinBox;
        maSpin->setRange(10, 500);
        maSpin->setValue(ma);
        grid->addWidget(maSpin, 1, 1);
        grid->addWidget(new QLabel("0.1 mm Cu"), 2, 0, 1, 2);
        grid->addWidget(new QLabel("Dose"), 3, 0);
        grid->addWidget(new QLabel(dose + " mGy·cm²"), 3, 1);
        return box;
    };
    layout->addWidget(makeExposure("Frontal plane", 83, 320, "215.66"));
    layout->addWidget(makeExposure("Lateral plane", 102, 250, "263.10"));
    layout->addStretch();

    scroll->setWidget(panel);
    return scroll;
}

QWidget *makeAcquisitionPage()
{
    auto *page = new QWidget;
    page->setObjectName("workspace");
    auto *root = new QHBoxLayout(page);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);
    root->addWidget(makeAcquisitionLeftPanel());

    auto *center = new QFrame;
    center->setObjectName("card");
    auto *centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(14, 14, 14, 12);
    centerLayout->setSpacing(10);
    centerLayout->addWidget(sectionTitle("Scan positioning"));

    auto *views = new QHBoxLayout;
    views->setSpacing(12);
    auto *front = new RadiographWidget(RadiographWidget::View::Positioning);
    auto *side = new RadiographWidget(RadiographWidget::View::Lateral);
    views->addWidget(front, 1);
    views->addWidget(side, 1);
    centerLayout->addLayout(views, 1);

    auto *limits = new QGridLayout;
    const QStringList names = {"Left limit", "Right limit", "Upper limit", "Lower limit"};
    for (int i = 0; i < names.size(); ++i) {
        auto *value = new QSpinBox;
        value->setRange(0, 250);
        value->setValue(i < 2 ? 22 : (i == 2 ? 166 : 66));
        value->setSuffix(" cm");
        limits->addWidget(new QLabel(names.at(i)), 0, i);
        limits->addWidget(value, 1, i);
    }
    centerLayout->addLayout(limits);

    auto *bottom = new QHBoxLayout;
    auto *ready = new QLabel("●  System ready");
    ready->setObjectName("readyLabel");
    bottom->addWidget(ready);
    bottom->addStretch();
    bottom->addWidget(orangeButton("Preheat"));
    bottom->addWidget(orangeButton("Calibration"));
    bottom->addWidget(orangeButton("Preview"));
    auto *acquire = orangeButton("ACQUIRE");
    acquire->setObjectName("primaryButton");
    acquire->setMinimumWidth(150);
    bottom->addWidget(acquire);
    centerLayout->addLayout(bottom);
    root->addWidget(center, 1);

    auto *right = new QFrame;
    right->setObjectName("sidePanel");
    right->setMinimumWidth(300);
    right->setMaximumWidth(365);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(14, 14, 14, 14);
    rightLayout->setSpacing(10);

    rightLayout->addWidget(sectionTitle("Reference planes"));
    auto *refTop = new QHBoxLayout;
    refTop->addWidget(orangeButton("Init"));
    refTop->addStretch();
    refTop->addWidget(new QLabel("0.0 cm"));
    rightLayout->addLayout(refTop);
    rightLayout->addWidget(new ReferenceWidget(false), 1);
    rightLayout->addWidget(orangeButton("<  Reference planes OK  >"));
    rightLayout->addWidget(lineSeparator(Qt::Horizontal));
    rightLayout->addWidget(sectionTitle("Patient orientation"));
    rightLayout->addWidget(new ReferenceWidget(true), 1);
    rightLayout->addWidget(orangeButton("<  Orientation OK  >"));
    rightLayout->addWidget(lineSeparator(Qt::Horizontal));
    rightLayout->addWidget(sectionTitle("System and status messages"));
    auto *message = new QLabel("Set the reference planes and patient orientation.");
    message->setObjectName("statusMessage");
    message->setWordWrap(true);
    rightLayout->addWidget(message);
    root->addWidget(right);
    return page;
}

QWidget *makeToolSection(const QString &title, const QStringList &tools, int columns = 4)
{
    auto *section = new QWidget;
    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    layout->addWidget(sectionTitle(title));
    auto *grid = new QGridLayout;
    grid->setSpacing(6);
    for (int i = 0; i < tools.size(); ++i)
        grid->addWidget(toolButton(tools.at(i)), i / columns, i % columns);
    layout->addLayout(grid);
    return section;
}

QWidget *makeViewerPage()
{
    auto *page = new QWidget;
    page->setObjectName("viewerWorkspace");
    auto *root = new QHBoxLayout(page);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto *scroll = new QScrollArea;
    scroll->setObjectName("viewerTools");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumWidth(220);
    scroll->setMaximumWidth(250);
    auto *tools = new QWidget;
    tools->setObjectName("sidePanel");
    auto *toolsLayout = new QVBoxLayout(tools);
    toolsLayout->setContentsMargins(10, 10, 10, 10);
    toolsLayout->setSpacing(8);
    toolsLayout->addWidget(makeToolSection("Patient list", {"List"}, 1));
    toolsLayout->addWidget(makeToolSection("Accessory capture", {"Capture", "Grid", "ROI", "Save"}, 4));

    auto *windowing = new QWidget;
    auto *windowLayout = new QGridLayout(windowing);
    windowLayout->setContentsMargins(0, 0, 0, 0);
    windowLayout->addWidget(sectionTitle("Windowing"), 0, 0, 1, 3);
    windowLayout->addWidget(new QLabel("Level"), 1, 0);
    auto *level = new QSpinBox;
    level->setRange(-32768, 32767);
    level->setValue(12767);
    windowLayout->addWidget(level, 1, 1);
    windowLayout->addWidget(new QLabel("Width"), 2, 0);
    auto *width = new QSpinBox;
    width->setRange(1, 65535);
    width->setValue(40575);
    windowLayout->addWidget(width, 2, 1);
    windowLayout->addWidget(toolButton("Auto"), 1, 2);
    windowLayout->addWidget(toolButton("Reset"), 2, 2);
    toolsLayout->addWidget(windowing);

    auto *gamma = new QWidget;
    auto *gammaLayout = new QHBoxLayout(gamma);
    gammaLayout->setContentsMargins(0, 0, 0, 0);
    gammaLayout->addWidget(new QLabel("Gamma"));
    auto *gammaSlider = new QSlider(Qt::Horizontal);
    gammaSlider->setRange(25, 250);
    gammaSlider->setValue(100);
    gammaLayout->addWidget(gammaSlider);
    gammaLayout->addWidget(new QLabel("1.00"));
    toolsLayout->addWidget(gamma);
    toolsLayout->addWidget(makeToolSection("Image", {"Prev", "Next", "1×1", "2×1"}, 4));
    toolsLayout->addWidget(makeToolSection("Processing", {"Pan", "Zoom", "Fit", "Rotate", "Flip H", "Flip V", "Invert", "1:1"}, 4));
    toolsLayout->addWidget(makeToolSection("Annotations", {"Line", "Angle", "Text", "Arrow", "Cobb", "Circle", "Ruler", "Clear"}, 4));
    toolsLayout->addWidget(makeToolSection("Print", {"Print", "Preview"}, 2));
    toolsLayout->addWidget(makeToolSection("Send and close", {"Send", "Close"}, 2));
    toolsLayout->addStretch();
    scroll->setWidget(tools);
    root->addWidget(scroll);

    auto *frontal = new RadiographWidget(RadiographWidget::View::Frontal);
    auto *lateral = new RadiographWidget(RadiographWidget::View::Lateral);
    frontal->setActive(false);
    lateral->setActive(true);
    root->addWidget(frontal, 1);
    root->addWidget(lateral, 1);
    return page;
}

QString applicationStyle()
{
    return QStringLiteral(R"(
        QMainWindow, QWidget#workspace { background: #cbd0d6; color: #222831; }
        QWidget#viewerWorkspace { background: #111419; }
        QMenuBar { background: #e7e9ec; border-bottom: 1px solid #9fa6ae; padding: 2px; }
        QMenuBar::item { padding: 4px 10px; background: transparent; }
        QMenuBar::item:selected { background: #f6aa45; }
        QStatusBar { background: #d9dde2; color: #4e555d; }
        QFrame#card { background: #e6e9ed; border: 1px solid #aeb5bd; border-radius: 2px; }
        QWidget#sidePanel, QFrame#sidePanel { background: #dfe3e8; border: 1px solid #aeb5bd; }
        QFrame#subCard { background: #e9ecef; border: 1px solid #bdc3ca; }
        QScrollArea#sideScroll, QScrollArea#viewerTools { border: 0; background: #dfe3e8; }
        QLabel#logo { color: #f07816; font-size: 55px; font-weight: 300; letter-spacing: 4px; }
        QLabel#logoSmall { color: #f07816; font-size: 40px; font-weight: 300; }
        QLabel#userTag { background: #e9ecef; border: 1px solid #aeb5bd; padding: 5px; }
        QLabel#sectionTitle { color: #4a5058; font-size: 12px; font-weight: 600; padding: 2px 0; }
        QLabel#fieldLabel { color: #555d66; font-size: 11px; }
        QLabel#readyLabel { color: #278a46; font-weight: 600; }
        QLabel#statusMessage { background: #edf0f3; border: 1px solid #b9c0c7; padding: 8px; color: #555c64; }
        QLineEdit, QComboBox, QSpinBox, QDateEdit {
            min-height: 28px; background: #f9fafb; border: 1px solid #aab1b9;
            border-radius: 2px; padding: 1px 7px; selection-background-color: #f28a13;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDateEdit:focus { border: 1px solid #e77c0b; }
        QGroupBox { border: 1px solid #b6bdc5; margin-top: 10px; padding-top: 8px; font-weight: 600; }
        QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }
        QPushButton#orangeButton, QPushButton#primaryButton {
            color: white; font-weight: 600; min-height: 28px; padding: 2px 14px;
            border: 1px solid #b85c00; border-radius: 4px;
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #ffb34d, stop:0.48 #f28a13, stop:1 #cf6500);
        }
        QPushButton#orangeButton:hover, QPushButton#primaryButton:hover { background: #ff9d25; }
        QPushButton#orangeButton:pressed, QPushButton#primaryButton:pressed { background: #bf5900; padding-top: 4px; }
        QPushButton#primaryButton { background: #e35424; border-color: #a33714; min-height: 34px; }
        QPushButton#toolButton {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #ffc66e, stop:1 #e98512);
            border: 1px solid #ad670e; border-radius: 4px; color: #4d3312; font-size: 10px; padding: 2px;
        }
        QPushButton#toolButton:hover { background: #ffd08a; }
        QPushButton#modeButton {
            color: white; background: #16191d; border: 0; border-right: 1px solid #555a60;
            min-height: 31px; font-size: 14px; font-weight: 600;
        }
        QPushButton#modeButton[active="true"] { color: #4b2c00; background: #f5a000; }
        QTableWidget#worklist { background: #f5f6f7; alternate-background-color: #e8ebee; gridline-color: #c3c8ce; border: 1px solid #aeb5bd; }
        QTableWidget#worklist::item { padding: 7px; }
        QTableWidget#worklist::item:selected { background: #f39a2d; color: #1f2429; }
        QHeaderView::section { background: #d4d9de; border: 0; border-right: 1px solid #afb6bd; border-bottom: 1px solid #a5adb5; padding: 7px; font-weight: 600; }
        QSlider::groove:horizontal { height: 5px; background: #b3bac1; border-radius: 2px; }
        QSlider::handle:horizontal { width: 14px; margin: -5px 0; background: #f28a13; border: 1px solid #a75b08; border-radius: 7px; }
        QScrollBar:vertical { background: #d6dbe0; width: 12px; }
        QScrollBar::handle:vertical { background: #aab1b8; min-height: 30px; border-radius: 5px; }
    )");
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("EOS Administration — UI Prototype");
    resize(1600, 960);
    setMinimumSize(1180, 720);
    setStyleSheet(applicationStyle());

    menuBar()->addMenu("File");
    menuBar()->addMenu("Acquisition");
    menuBar()->addMenu("View");
    menuBar()->addMenu("Tools");
    menuBar()->addMenu("Preferences");
    menuBar()->addMenu("Help");

    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *modeBar = new QWidget;
    modeBar->setObjectName("modeBar");
    auto *modeLayout = new QHBoxLayout(modeBar);
    modeLayout->setContentsMargins(0, 0, 0, 0);
    modeLayout->setSpacing(0);
    const QStringList modes = {"Patient Info Mode", "Acquisition Mode", "Viewer Mode"};
    for (int i = 0; i < modes.size(); ++i) {
        auto *button = new QPushButton(modes.at(i));
        button->setObjectName("modeButton");
        button->setProperty("active", i == 0);
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, [this, i] { setMode(i); });
        modeLayout->addWidget(button, 1);
        m_modeButtons[i] = button;
    }
    layout->addWidget(modeBar);

    m_pages = new QStackedWidget;
    m_pages->addWidget(makePatientPage());
    m_pages->addWidget(makeAcquisitionPage());
    m_pages->addWidget(makeViewerPage());
    layout->addWidget(m_pages, 1);
    setCentralWidget(central);

    statusBar()->showMessage("EOS UI prototype · Static interface · No acquisition logic connected");
    setMode(0);
}

void MainWindow::setMode(int index)
{
    if (!m_pages || index < 0 || index >= m_pages->count())
        return;
    m_pages->setCurrentIndex(index);
    for (int i = 0; i < 3; ++i) {
        m_modeButtons[i]->setProperty("active", i == index);
        m_modeButtons[i]->style()->unpolish(m_modeButtons[i]);
        m_modeButtons[i]->style()->polish(m_modeButtons[i]);
        m_modeButtons[i]->update();
    }
    const QStringList messages = {
        "Patient information and worklist",
        "Acquisition setup — prototype controls only",
        "Viewer — prototype tools only"
    };
    statusBar()->showMessage(messages.at(index));
}

