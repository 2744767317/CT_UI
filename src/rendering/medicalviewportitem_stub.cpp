#include "medicalviewportitem.h"

#include <QPainter>

#include <algorithm>

MedicalViewportItem::MedicalViewportItem(QQuickItem *parent)
    : MedicalViewportBase(parent)
{
    setAntialiasing(true);
}

void MedicalViewportItem::setViewType(ViewType type)
{
    if (m_viewType == type)
        return;
    m_viewType = type;
    emit viewTypeChanged();
    update();
}

void MedicalViewportItem::setController(MedicalDataController *controller)
{
    if (m_controller == controller)
        return;
    if (m_controller)
        disconnect(m_controller, nullptr, this, nullptr);
    m_controller = controller;
    if (m_controller) {
        connect(m_controller, &MedicalDataController::dataChanged,
                this, &MedicalViewportItem::reloadData);
        connect(m_controller, &MedicalDataController::segmentationChanged,
                this, &MedicalViewportItem::reloadData);
        connect(m_controller, &MedicalDataController::windowingChanged,
                this, &MedicalViewportItem::updateRenderState);
    }
    emit controllerChanged();
    reloadData();
}

void MedicalViewportItem::setSlicePosition(double position)
{
    position = std::clamp(position, 0.0, 1.0);
    if (qFuzzyCompare(m_slicePosition, position))
        return;
    m_slicePosition = position;
    emit slicePositionChanged();
    update();
}

void MedicalViewportItem::setMip(bool mip)
{
    if (m_mip == mip)
        return;
    m_mip = mip;
    emit mipChanged();
    update();
}

void MedicalViewportItem::setVolumePreset(VolumePreset preset)
{
    if (m_volumePreset == preset)
        return;
    m_volumePreset = preset;
    emit volumePresetChanged();
    update();
}

void MedicalViewportItem::setShowSegmentation(bool visible)
{
    if (m_showSegmentation == visible)
        return;
    m_showSegmentation = visible;
    emit showSegmentationChanged();
    update();
}

void MedicalViewportItem::setPairedProjection(bool paired)
{
    if (m_pairedProjection == paired)
        return;
    m_pairedProjection = paired;
    emit pairedProjectionChanged();
    reloadData();
}

void MedicalViewportItem::setShowImage(bool visible)
{
    if (m_showImage == visible)
        return;
    m_showImage = visible;
    emit showImageChanged();
    update();
}

void MedicalViewportItem::setSegmentationOpacity(double opacity)
{
    opacity = std::clamp(opacity, 0.0, 1.0);
    if (qFuzzyCompare(m_segmentationOpacity, opacity))
        return;
    m_segmentationOpacity = opacity;
    emit segmentationOpacityChanged();
    update();
}

void MedicalViewportItem::setRotationQuarterTurns(int turns)
{
    turns = ((turns % 4) + 4) % 4;
    if (m_rotationQuarterTurns == turns)
        return;
    m_rotationQuarterTurns = turns;
    emit orientationChanged();
    reloadData();
}

void MedicalViewportItem::setFlipHorizontal(bool flipped)
{
    if (m_flipHorizontal == flipped)
        return;
    m_flipHorizontal = flipped;
    emit orientationChanged();
    reloadData();
}

void MedicalViewportItem::setFlipVertical(bool flipped)
{
    if (m_flipVertical == flipped)
        return;
    m_flipVertical = flipped;
    emit orientationChanged();
    reloadData();
}

void MedicalViewportItem::setCropMinimum(double value)
{
    m_cropMinimum = std::clamp(value, 0.0, m_cropMaximum - 0.01);
    emit cropChanged();
    update();
}

void MedicalViewportItem::setCropMaximum(double value)
{
    m_cropMaximum = std::clamp(value, m_cropMinimum + 0.01, 1.0);
    emit cropChanged();
    update();
}

void MedicalViewportItem::pickVoxel(double, double)
{
    emit voxelPickFailed(QStringLiteral("MinGW UI 兼容模式不提供医学图像拾取。"));
}

void MedicalViewportItem::reloadData()
{
    m_volume = m_controller ? m_controller->volumeSnapshot() : nullptr;
    m_mask = m_controller ? m_controller->maskSnapshot() : nullptr;
    update();
}

void MedicalViewportItem::updateRenderState()
{
    update();
}

void MedicalViewportItem::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), QColor(QStringLiteral("#070A0C")));
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(QColor(QStringLiteral("#344047")), 1));
    painter->drawLine(QPointF(width() * 0.5, 20.0), QPointF(width() * 0.5, height() - 20.0));
    painter->drawLine(QPointF(20.0, height() * 0.5), QPointF(width() - 20.0, height() * 0.5));
    painter->setPen(QColor(QStringLiteral("#9AA5AA")));
    painter->drawText(boundingRect(), Qt::AlignCenter,
                      QStringLiteral("MinGW UI 兼容模式\n需要 MinGW 版 VTK / ITK 才能显示医学影像"));
}
