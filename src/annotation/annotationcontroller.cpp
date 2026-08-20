#include "annotationcontroller.h"

#include "src/markups/markupspicker.h"

#include <algorithm>

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

int AnnotationController::markCount() const
{
    return m_scene.markCount();
}

int AnnotationController::measureCount() const
{
    return m_scene.measureCount();
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
        connect(m_medicalData, &MedicalDataController::volumeNodesChanged,
                this, [this]() { rebindActiveScene(); emitSceneChanged(); });
        connect(m_medicalData, &MedicalDataController::casePackageAnnotationsReady,
                this, &AnnotationController::restoreCaseAnnotations);
    }
    m_scenes.clear();
    m_activeVolumeId.clear();
    rebindActiveScene();
    emitSceneChanged();
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
    return addWorldPointForView(x, y, z, {});
}

bool AnnotationController::addWorldPointForView(double x, double y, double z,
                                                const QString &viewId)
{
    if (!m_scene.addWorldPoint(QVector3D(static_cast<float>(x),
                                         static_cast<float>(y),
                                         static_cast<float>(z)), viewId))
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

void AnnotationController::setNodeVisible(int nodeId, bool visible)
{
    m_scene.setNodeVisible(nodeId, visible);
    emitSceneChanged();
}

void AnnotationController::setNodeColor(int nodeId, const QString &color)
{
    m_scene.setNodeColor(nodeId, color);
    emitSceneChanged();
}

bool AnnotationController::removeNode(int nodeId)
{
    const bool ok = m_scene.removeNode(nodeId);
    if (ok)
        emitSceneChanged();
    return ok;
}

int AnnotationController::markCountFor(const QString &volumeId) const
{
    const MarkupsScene *s = sceneForId(volumeId);
    return s ? s->markCount() : 0;
}

int AnnotationController::measureCountFor(const QString &volumeId) const
{
    const MarkupsScene *s = sceneForId(volumeId);
    return s ? s->measureCount() : 0;
}

MarkupsScene *AnnotationController::sceneForId(const QString &volumeId) const
{
    if (volumeId.isEmpty())
        return nullptr;
    if (volumeId == m_activeVolumeId)
        return const_cast<MarkupsScene *>(&m_scene);
    auto it = const_cast<std::map<QString, MarkupsScene> &>(m_scenes).find(volumeId);
    if (it == m_scenes.end())
        return nullptr;
    return &it->second;
}

void AnnotationController::onMedicalDataChanged()
{
    rebindActiveScene();
    emitSceneChanged();
}

void AnnotationController::rebindActiveScene()
{
    if (!m_medicalData) {
        m_activeVolumeId.clear();
        return;
    }

    // 收集当前所有 volume id，清理已不存在的 scene。
    const QVariantList nodes = m_medicalData->volumeNodes();
    std::vector<QString> currentIds;
    currentIds.reserve(static_cast<std::size_t>(nodes.size()));
    for (const QVariant &entry : nodes) {
        const QVariantMap item = entry.toMap();
        currentIds.push_back(item.value(QStringLiteral("id")).toString());
    }
    for (auto it = m_scenes.begin(); it != m_scenes.end();) {
        if (std::find(currentIds.begin(), currentIds.end(), it->first) == currentIds.end())
            it = m_scenes.erase(it);
        else
            ++it;
    }

    // 取活动数据集 id（优先用 activeVolumeId()，回退到 volumeNodes）。
    QString activeId;
    if (m_medicalData->selectedVolumeIndex() >= 0
        && m_medicalData->selectedVolumeIndex() < static_cast<int>(nodes.size())) {
        activeId = m_medicalData->activeVolumeId();
        if (activeId.isEmpty()) {
            const QVariantMap item = nodes.at(m_medicalData->selectedVolumeIndex()).toMap();
            activeId = item.value(QStringLiteral("id")).toString();
        }
    }

    if (activeId.isEmpty()) {
        const MarkupsTool tool = m_scene.tool();
        const bool visible = m_scene.visible();
        m_scene = MarkupsScene {};
        m_scene.setTool(tool);
        m_scene.setVisible(visible);
        m_activeVolumeId.clear();
        m_lastRevision = -1;
        return;
    }

    if (activeId == m_activeVolumeId)
        return;  // 未切换，保持当前 scene 状态

    // 切换前：把当前工作 scene 回存到旧数据集（若旧数据集仍存在）。
    if (!m_activeVolumeId.isEmpty() && m_scenes.count(m_activeVolumeId))
        m_scenes[m_activeVolumeId] = m_scene;

    // 加载目标数据集的 scene（新数据集会默认构造为空 scene）。
    const MarkupsTool tool = m_scene.tool();
    const bool visible = m_scene.visible();
    m_scene = m_scenes[activeId];
    m_scene.setTool(tool);
    m_scene.setVisible(visible);
    m_activeVolumeId = activeId;
    m_lastRevision = -1;
}

bool AnnotationController::restoreCaseAnnotations(const QVariantList &items)
{
    if (!m_scene.restoreItems(items))
        return false;
    emitSceneChanged();
    return true;
}
