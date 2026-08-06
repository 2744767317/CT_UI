#include "annotationcontroller.h"

#include "src/markups/markupspicker.h"

AnnotationController::AnnotationController(QObject *parent)
    : QObject(parent)
{
}

int AnnotationController::toolType() const
{
    return static_cast<int>(m_scene.tool());
}

bool AnnotationController::visible() const
{
    return m_scene.visible();
}

bool AnnotationController::hasActive() const
{
    return m_scene.hasActive();
}

int AnnotationController::revision() const
{
    return m_scene.revision();
}

QVariantList AnnotationController::items() const
{
    return m_scene.itemsVariant();
}

QVariantList AnnotationController::activePoints() const
{
    return m_scene.activePointsVariant();
}

QString AnnotationController::activeLabelPreview() const
{
    return m_scene.activeLabelPreview();
}

QVariantList AnnotationController::renderItems() const
{
    return m_scene.renderItemsVariant();
}

void AnnotationController::setMedicalData(MedicalDataController *data)
{
    if (m_medicalData == data)
        return;
    if (m_medicalData)
        disconnect(m_medicalData, nullptr, this, nullptr);
    m_medicalData = data;
    if (m_medicalData) {
        connect(m_medicalData, &MedicalDataController::dataChanged,
                this, &AnnotationController::onMedicalDataChanged);
    }
    clearAll();
}

void AnnotationController::emitSceneChanged()
{
    const int rev = m_scene.revision();
    if (rev == m_lastRevision)
        return;
    m_lastRevision = rev;
    emit annotationsChanged();
}

void AnnotationController::setToolType(int type)
{
    const auto tool = static_cast<MarkupsTool>(type);
    const int before = static_cast<int>(m_scene.tool());
    m_scene.setTool(tool);
    if (before != static_cast<int>(m_scene.tool()))
        emit toolChanged();
    emitSceneChanged();
}

void AnnotationController::setVisible(bool visible)
{
    const bool before = m_scene.visible();
    m_scene.setVisible(visible);
    if (before != m_scene.visible())
        emit visibleChanged();
    emitSceneChanged();
}

bool AnnotationController::addWorldPoint(double x, double y, double z)
{
    if (!m_scene.addWorldPoint(QVector3D(static_cast<float>(x),
                                         static_cast<float>(y),
                                         static_cast<float>(z))))
        return false;
    emitSceneChanged();
    return true;
}

bool AnnotationController::addControlPoint(int voxelX, int voxelY, int voxelZ)
{
    if (!m_medicalData || !m_medicalData->volumeData())
        return false;
    const auto snapshot = m_medicalData->volumeSnapshot();
    if (!snapshot)
        return false;
    const QVector3D world = MarkupsPicker::voxelToWorld(*snapshot, voxelX, voxelY, voxelZ);
    return addWorldPoint(world.x(), world.y(), world.z());
}

bool AnnotationController::finishActive()
{
    if (!m_scene.finishActive())
        return false;
    emitSceneChanged();
    return true;
}

void AnnotationController::cancelActive()
{
    m_scene.cancelActive();
    emitSceneChanged();
}

void AnnotationController::clearAll()
{
    m_scene.clearAll();
    emitSceneChanged();
}

bool AnnotationController::updateControlPoint(int nodeId, int pointIndex,
                                              double x, double y, double z)
{
    if (!m_scene.updateControlPoint(nodeId, pointIndex,
                                    QVector3D(static_cast<float>(x),
                                              static_cast<float>(y),
                                              static_cast<float>(z))))
        return false;
    emitSceneChanged();
    return true;
}

bool AnnotationController::updateControlPointFromVoxel(int nodeId, int pointIndex,
                                                       int voxelX, int voxelY, int voxelZ)
{
    if (!m_medicalData || !m_medicalData->volumeData())
        return false;
    const auto snapshot = m_medicalData->volumeSnapshot();
    if (!snapshot)
        return false;
    const QVector3D world = MarkupsPicker::voxelToWorld(*snapshot, voxelX, voxelY, voxelZ);
    return updateControlPoint(nodeId, pointIndex, world.x(), world.y(), world.z());
}

void AnnotationController::onMedicalDataChanged()
{
    clearAll();
}
