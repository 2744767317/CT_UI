#include "imagingviewport.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>

#include <cmath>
#include <utility>

namespace {

QColor accent(ImagingViewport::ViewType type)
{
    switch (type) {
    case ImagingViewport::ViewType::Axial: return QColor("#c75850");
    case ImagingViewport::ViewType::Coronal: return QColor("#62a178");
    case ImagingViewport::ViewType::Sagittal: return QColor("#c5a549");
    case ImagingViewport::ViewType::Volume3D: return QColor("#6c8fb0");
    case ImagingViewport::ViewType::ProjectionAP: return QColor("#8aa0ad");
    case ImagingViewport::ViewType::ProjectionLAT: return QColor("#8aa0ad");
    }
    return QColor("#8c969c");
}

QString title(ImagingViewport::ViewType type)
{
    switch (type) {
    case ImagingViewport::ViewType::Axial: return "AXIAL  轴状位";
    case ImagingViewport::ViewType::Coronal: return "CORONAL  冠状位";
    case ImagingViewport::ViewType::Sagittal: return "SAGITTAL  矢状位";
    case ImagingViewport::ViewType::Volume3D: return "3D  VOLUME RENDERING";
    case ImagingViewport::ViewType::ProjectionAP: return "PROJECTION  AP";
    case ImagingViewport::ViewType::ProjectionLAT: return "PROJECTION  LAT";
    }
    return {};
}

QString coordinate(ImagingViewport::ViewType type)
{
    switch (type) {
    case ImagingViewport::ViewType::Axial: return "S: 104.2 mm  ·  412 / 684";
    case ImagingViewport::ViewType::Coronal: return "A: 36.8 mm  ·  286 / 512";
    case ImagingViewport::ViewType::Sagittal: return "R: -12.4 mm  ·  248 / 512";
    case ImagingViewport::ViewType::Volume3D: return "Preset: Bone + Soft Tissue";
    case ImagingViewport::ViewType::ProjectionAP: return "Original · 512 × 2048";
    case ImagingViewport::ViewType::ProjectionLAT: return "Original · 512 × 2048";
    }
    return {};
}

} // namespace

ImagingViewport::ImagingViewport(ViewType type, QWidget *parent)
    : QWidget(parent), m_type(type)
{
    setObjectName("viewport");
    setProperty("selected", false);
    setMinimumSize(320, 260);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::CrossCursor);
}

void ImagingViewport::setSelected(bool selected)
{
    m_selected = selected;
    setProperty("selected", selected);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void ImagingViewport::setToolMode(const QString &toolMode)
{
    m_toolMode = toolMode;
    setCursor(toolMode == "十字线" ? Qt::CrossCursor : toolMode == "平移" ? Qt::SizeAllCursor : Qt::ArrowCursor);
    update();
}

QString ImagingViewport::displayName() const
{
    return title(m_type);
}

void ImagingViewport::setActivatedCallback(std::function<void(ImagingViewport *)> callback)
{
    m_activatedCallback = std::move(callback);
}

void ImagingViewport::setMaximizeCallback(std::function<void(ImagingViewport *)> callback)
{
    m_maximizeCallback = std::move(callback);
}

void ImagingViewport::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#050708"));

    const QColor viewAccent = accent(m_type);
    p.fillRect(QRect(0, 0, width(), 32), QColor("#1a1f22"));
    p.fillRect(QRect(0, 0, width(), 3), viewAccent);

    QFont font = p.font();
    font.setPixelSize(12);
    font.setBold(true);
    p.setFont(font);
    p.setPen(viewAccent.lighter(120));
    p.drawText(QRect(10, 3, width() - 190, 29), Qt::AlignVCenter | Qt::AlignLeft, title(m_type));
    font.setBold(false);
    font.setPixelSize(10);
    p.setFont(font);
    p.setPen(QColor("#8f999e"));
    p.drawText(QRect(width() - 210, 3, 174, 29), Qt::AlignVCenter | Qt::AlignRight, coordinate(m_type));
    p.setPen(QColor("#b7c0c4"));
    p.drawText(QRect(width() - 30, 3, 22, 29), Qt::AlignCenter, "□");

    const QRectF area = QRectF(rect()).adjusted(8, 38, -8, -25);
    QLinearGradient background(area.topLeft(), area.bottomRight());
    background.setColorAt(0, QColor("#101417"));
    background.setColorAt(0.55, QColor("#030405"));
    background.setColorAt(1, QColor("#0b0e10"));
    p.fillRect(area, background);

    if (m_type == ViewType::Volume3D)
        drawVolume(p, area);
    else if (m_type == ViewType::ProjectionAP || m_type == ViewType::ProjectionLAT)
        drawProjection(p, area);
    else
        drawSlice(p, area);
    drawOverlay(p, area);

    p.fillRect(QRectF(8, height() - 15, width() - 16, 3), QColor("#30383c"));
    const qreal fraction = m_type == ViewType::Axial ? .60 : m_type == ViewType::Coronal ? .55 : .48;
    p.fillRect(QRectF(8, height() - 15, (width() - 16) * fraction, 3), QColor(viewAccent.red(), viewAccent.green(), viewAccent.blue(), 165));
    p.setBrush(QColor("#d9dddf"));
    p.setPen(QPen(QColor("#69747a"), 1));
    p.drawEllipse(QPointF(8 + (width() - 16) * fraction, height() - 13.5), 5, 5);
    if (m_selected) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#d98339"), 2));
        p.drawRect(rect().adjusted(1, 1, -2, -2));
    }
}

void ImagingViewport::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_activatedCallback)
            m_activatedCallback(this);
        else
            setSelected(true);
    }
    QWidget::mousePressEvent(event);
}

void ImagingViewport::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_maximizeCallback)
        m_maximizeCallback(this);
    QWidget::mouseDoubleClickEvent(event);
}

void ImagingViewport::drawSlice(QPainter &p, const QRectF &area)
{
    p.save();
    const qreal scale = qMin(area.width() / 520.0, area.height() / 520.0);
    p.translate(area.center());
    p.scale(scale, scale);

    const QColor tissue(150, 158, 160, 55);
    const QColor tissue2(195, 200, 201, 60);
    const QColor bone(226, 231, 231, 150);
    const QColor dark(13, 16, 18, 210);
    p.setPen(QPen(QColor(210, 217, 219, 90), 2));

    if (m_type == ViewType::Axial) {
        QRadialGradient body(QPointF(-24, -28), 220);
        body.setColorAt(0, QColor(205, 211, 211, 100));
        body.setColorAt(.55, QColor(116, 126, 129, 75));
        body.setColorAt(1, QColor(38, 44, 47, 90));
        p.setBrush(body);
        p.drawEllipse(QRectF(-205, -174, 410, 348));
        p.setBrush(dark);
        p.drawEllipse(QRectF(-142, -90, 118, 150));
        p.drawEllipse(QRectF(24, -90, 118, 150));
        p.setBrush(tissue2);
        p.drawEllipse(QRectF(-62, 55, 124, 84));
        p.setBrush(bone);
        p.drawEllipse(QRectF(-35, 66, 70, 53));
        p.setBrush(QColor(8, 10, 11, 220));
        p.drawEllipse(QRectF(-13, 80, 26, 19));
        p.setPen(QPen(QColor(219, 225, 226, 105), 3));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(-180, -148, 360, 285), 18 * 16, 145 * 16);
        p.drawArc(QRectF(-180, -148, 360, 285), 198 * 16, 145 * 16);
    } else if (m_type == ViewType::Coronal) {
        QPainterPath body;
        body.moveTo(0, -235);
        body.cubicTo(-68, -230, -72, -155, -48, -122);
        body.cubicTo(-145, -90, -165, 22, -118, 132);
        body.cubicTo(-92, 184, -95, 220, -82, 245);
        body.lineTo(82, 245);
        body.cubicTo(95, 220, 92, 184, 118, 132);
        body.cubicTo(165, 22, 145, -90, 48, -122);
        body.cubicTo(72, -155, 68, -230, 0, -235);
        p.setPen(Qt::NoPen);
        p.setBrush(tissue);
        p.drawPath(body);
        p.setPen(QPen(bone, 5));
        p.setBrush(QColor(208, 214, 215, 50));
        p.drawEllipse(QRectF(-57, -228, 114, 114));
        p.drawLine(QPointF(0, -112), QPointF(0, 205));
        for (int y = -104; y < 202; y += 21)
            p.drawRoundedRect(QRectF(-17, y, 34, 10), 3, 3);
        for (int i = 0; i < 8; ++i) {
            const qreal y = -88 + i * 25;
            p.drawArc(QRectF(-112, y, 112, 76), 20 * 16, 140 * 16);
            p.drawArc(QRectF(0, y, 112, 76), 20 * 16, 140 * 16);
        }
        p.drawEllipse(QRectF(-82, 150, 164, 82));
        p.drawLine(QPointF(-45, 220), QPointF(-58, 270));
        p.drawLine(QPointF(45, 220), QPointF(58, 270));
    } else {
        QPainterPath body;
        body.moveTo(12, -235);
        body.cubicTo(92, -210, 88, -140, 46, -110);
        body.cubicTo(98, -42, 94, 70, 58, 145);
        body.cubicTo(43, 180, 69, 222, 32, 258);
        body.lineTo(-55, 258);
        body.cubicTo(-63, 158, -46, 95, -70, 20);
        body.cubicTo(-95, -70, -51, -128, -28, -148);
        body.cubicTo(-44, -190, -25, -228, 12, -235);
        p.setPen(Qt::NoPen);
        p.setBrush(tissue);
        p.drawPath(body);
        p.setPen(QPen(bone, 5));
        p.setBrush(QColor(208, 214, 215, 48));
        p.drawEllipse(QRectF(-32, -226, 106, 112));
        QPainterPath spine;
        spine.moveTo(15, -112);
        spine.cubicTo(-18, -25, 39, 55, 2, 150);
        spine.cubicTo(-12, 185, 17, 222, 4, 250);
        p.drawPath(spine);
        for (int y = -100; y < 220; y += 22)
            p.drawRoundedRect(QRectF(-5 + 12 * std::sin(y / 53.0), y, 37, 9), 3, 3);
        p.drawEllipse(QRectF(-27, 152, 111, 72));
    }
    p.restore();
}

void ImagingViewport::drawVolume(QPainter &p, const QRectF &area)
{
    p.save();
    const qreal scale = qMin(area.width() / 520.0, area.height() / 520.0);
    p.translate(area.center());
    p.scale(scale, scale);

    QLinearGradient soft(-150, -180, 160, 220);
    soft.setColorAt(0, QColor(185, 119, 78, 65));
    soft.setColorAt(.5, QColor(121, 65, 43, 105));
    soft.setColorAt(1, QColor(52, 34, 27, 30));
    QPainterPath silhouette;
    silhouette.moveTo(0, -235);
    silhouette.cubicTo(-72, -225, -76, -156, -48, -118);
    silhouette.cubicTo(-138, -88, -152, 35, -106, 130);
    silhouette.cubicTo(-82, 184, -92, 222, -69, 253);
    silhouette.lineTo(69, 253);
    silhouette.cubicTo(92, 222, 82, 184, 106, 130);
    silhouette.cubicTo(152, 35, 138, -88, 48, -118);
    silhouette.cubicTo(76, -156, 72, -225, 0, -235);
    p.setPen(QPen(QColor(208, 145, 101, 95), 2));
    p.setBrush(soft);
    p.drawPath(silhouette);

    p.setPen(QPen(QColor(228, 218, 183, 150), 5));
    p.setBrush(QColor(206, 193, 158, 62));
    p.drawEllipse(QRectF(-52, -221, 104, 108));
    p.drawLine(QPointF(0, -110), QPointF(0, 205));
    for (int y = -103; y < 205; y += 20)
        p.drawRoundedRect(QRectF(-16, y, 32, 9), 3, 3);
    for (int i = 0; i < 9; ++i) {
        const qreal y = -90 + i * 23;
        const qreal w = 102 - i * 3;
        p.drawArc(QRectF(-w, y, w, 74), 20 * 16, 140 * 16);
        p.drawArc(QRectF(0, y, w, 74), 20 * 16, 140 * 16);
    }
    p.drawEllipse(QRectF(-76, 151, 152, 80));
    p.drawLine(QPointF(-42, 216), QPointF(-54, 274));
    p.drawLine(QPointF(42, 216), QPointF(54, 274));

    p.setPen(QPen(QColor(104, 143, 176, 105), 1));
    p.setBrush(Qt::NoBrush);
    const QRectF box(-184, -250, 368, 520);
    p.drawRect(box);
    p.drawLine(box.topLeft(), QPointF(-145, -220));
    p.drawLine(box.topRight(), QPointF(145, -220));
    p.drawLine(box.bottomLeft(), QPointF(-145, 235));
    p.drawLine(box.bottomRight(), QPointF(145, 235));
    p.drawRect(QRectF(-145, -220, 290, 455));
    p.restore();
}

void ImagingViewport::drawProjection(QPainter &p, const QRectF &area)
{
    p.save();
    const qreal scale = qMin(area.width() / 420.0, area.height() / 760.0);
    p.translate(area.center());
    p.scale(scale, scale);

    const bool lateral = m_type == ViewType::ProjectionLAT;
    const QColor soft(155, 168, 173, 42);
    const QColor bone(225, 231, 232, 145);
    QPainterPath body;
    if (lateral) {
        body.moveTo(18, -360);
        body.cubicTo(98, -340, 96, -270, 55, -235);
        body.cubicTo(116, -178, 118, -42, 76, 80);
        body.cubicTo(52, 155, 90, 245, 36, 360);
        body.lineTo(-74, 360);
        body.cubicTo(-98, 210, -78, 132, -101, 33);
        body.cubicTo(-134, -109, -91, -199, -45, -240);
        body.cubicTo(-72, -296, -58, -350, 18, -360);
    } else {
        body.moveTo(0, -365);
        body.cubicTo(-76, -360, -72, -285, -46, -245);
        body.cubicTo(-132, -211, -137, -72, -99, 42);
        body.cubicTo(-78, 110, -98, 207, -108, 360);
        body.lineTo(108, 360);
        body.cubicTo(98, 207, 78, 110, 99, 42);
        body.cubicTo(137, -72, 132, -211, 46, -245);
        body.cubicTo(72, -285, 76, -360, 0, -365);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(soft);
    p.drawPath(body);
    p.setPen(QPen(bone, 5));
    p.setBrush(QColor(218, 224, 225, 52));
    p.drawEllipse(lateral ? QRectF(-32, -355, 104, 118) : QRectF(-53, -354, 106, 118));
    if (lateral) {
        QPainterPath spine;
        spine.moveTo(16, -235);
        spine.cubicTo(-12, -125, 40, -15, 7, 112);
        spine.cubicTo(-10, 185, 20, 257, 3, 326);
        p.drawPath(spine);
        for (int y = -225; y < 315; y += 28)
            p.drawRoundedRect(QRectF(0 + 12 * std::sin(y / 58.0), y, 39, 11), 3, 3);
    } else {
        p.drawLine(QPointF(0, -235), QPointF(0, 315));
        for (int y = -225; y < 310; y += 27)
            p.drawRoundedRect(QRectF(-16, y, 32, 10), 3, 3);
        for (int i = 0; i < 10; ++i) {
            const qreal y = -202 + i * 29;
            const qreal w = 98 - i * 2;
            p.drawArc(QRectF(-w, y, w, 88), 20 * 16, 140 * 16);
            p.drawArc(QRectF(0, y, w, 88), 20 * 16, 140 * 16);
        }
        p.drawEllipse(QRectF(-76, 200, 152, 92));
        p.drawLine(QPointF(-43, 284), QPointF(-55, 372));
        p.drawLine(QPointF(43, 284), QPointF(55, 372));
    }
    p.restore();
}

void ImagingViewport::drawOverlay(QPainter &p, const QRectF &area)
{
    QFont font = p.font();
    font.setPixelSize(12);
    font.setBold(true);
    p.setFont(font);
    p.setPen(QColor("#d8dddf"));

    const QString top = m_type == ViewType::Axial ? "A" : "S";
    const QString bottom = m_type == ViewType::Axial ? "P" : "I";
    const QString left = m_type == ViewType::Sagittal || m_type == ViewType::ProjectionLAT ? "A" : "R";
    const QString right = m_type == ViewType::Sagittal || m_type == ViewType::ProjectionLAT ? "P" : "L";
    p.drawText(QRectF(area.left(), area.top() + 5, area.width(), 20), Qt::AlignTop | Qt::AlignHCenter, top);
    p.drawText(QRectF(area.left(), area.bottom() - 24, area.width(), 20), Qt::AlignBottom | Qt::AlignHCenter, bottom);
    p.drawText(QRectF(area.left() + 7, area.top(), 22, area.height()), Qt::AlignLeft | Qt::AlignVCenter, left);
    p.drawText(QRectF(area.right() - 28, area.top(), 22, area.height()), Qt::AlignRight | Qt::AlignVCenter, right);

    if (m_type != ViewType::Volume3D && m_type != ViewType::ProjectionAP && m_type != ViewType::ProjectionLAT) {
        const QPointF center = area.center();
        const QColor horizontal = m_type == ViewType::Axial ? accent(ViewType::Coronal) : accent(ViewType::Axial);
        const QColor vertical = m_type == ViewType::Sagittal ? accent(ViewType::Coronal) : accent(ViewType::Sagittal);
        p.setPen(QPen(horizontal, 1));
        p.drawLine(QPointF(area.left() + 22, center.y()), QPointF(area.right() - 22, center.y()));
        p.setPen(QPen(vertical, 1));
        p.drawLine(QPointF(center.x(), area.top() + 22), QPointF(center.x(), area.bottom() - 22));
        p.setBrush(QColor("#e3e6e7"));
        p.setPen(Qt::NoPen);
        p.drawEllipse(center, 3, 3);
    }

    font.setBold(false);
    font.setPixelSize(10);
    p.setFont(font);
    p.setPen(QColor("#889399"));
    p.drawText(area.adjusted(8, 7, -8, -7), Qt::AlignLeft | Qt::AlignBottom,
               m_type == ViewType::Volume3D ? "GPU  ·  42 fps" :
               m_type == ViewType::ProjectionAP || m_type == ViewType::ProjectionLAT ? "Projection · W/L Auto" :
               "W 1800  /  L 450");

    p.setPen(QColor("#8b969c"));
    p.drawText(area.adjusted(8, 7, -8, -7), Qt::AlignRight | Qt::AlignBottom, "Tool: " + m_toolMode);
}
