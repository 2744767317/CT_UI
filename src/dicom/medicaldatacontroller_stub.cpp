#include "medicaldatacontroller.h"

#include <QDir>
#include <QVariantMap>

#include <algorithm>
#include <cmath>

struct LoadedVolumeNode
{
    QString id;
    QString name;
    QString patientName;
    QString patientId;
    QString modality;
    QString sourcePath;
    QStringList sourceFiles;
    std::shared_ptr<VolumeSnapshot> volume;
    std::shared_ptr<VolumeSnapshot> projectionPair;
    std::shared_ptr<MaskSnapshot> mask;
    double windowWidth = 400.0;
    double windowLevel = 40.0;
    QString segmentationMethod;
    qint64 segmentationVoxelCount = 0;
    double segmentationVolumeMl = 0.0;
    bool projectionUnsigned = false;
    bool projectionInverted = false;
    bool projectionPairInverted = false;
    bool visible = true;
};

// 兼容构建不执行真实异步医学任务，但保留相同的接口契约，方便 QML/UI 验证。
struct SeriesLoadResult {};
struct SegmentationResult {};

MedicalDataController::MedicalDataController(QObject *parent)
    : QObject(parent)
{
    resetMetadata();
    m_statusMessage = QStringLiteral("MinGW UI 兼容模式：医学影像后端未启用");
}

bool MedicalDataController::loaded() const
{
    std::lock_guard<std::mutex> guard(m_snapshotMutex);
    return m_volume && !m_volume->pixels.empty();
}

bool MedicalDataController::volumeData() const
{
    std::lock_guard<std::mutex> guard(m_snapshotMutex);
    return m_volume && m_volume->dimensions[2] > 1;
}

bool MedicalDataController::projectionData() const
{
    std::lock_guard<std::mutex> guard(m_snapshotMutex);
    return m_volume && !m_volume->pixels.empty() && m_volume->dimensions[2] == 1
        && m_modality != QStringLiteral("CT");
}

bool MedicalDataController::pairedProjectionAvailable() const
{
    std::lock_guard<std::mutex> guard(m_snapshotMutex);
    return m_projectionPair && !m_projectionPair->pixels.empty();
}

bool MedicalDataController::segmentationAvailable() const
{
    std::lock_guard<std::mutex> guard(m_snapshotMutex);
    return m_mask && !m_mask->pixels.empty();
}

QString MedicalDataController::dimensionsText() const
{
    const auto snapshot = volumeSnapshot();
    return snapshot
        ? QStringLiteral("%1 × %2 × %3").arg(snapshot->dimensions[0])
              .arg(snapshot->dimensions[1]).arg(snapshot->dimensions[2])
        : QStringLiteral("--");
}

QString MedicalDataController::spacingText() const
{
    const auto snapshot = volumeSnapshot();
    return snapshot
        ? QStringLiteral("%1 × %2 × %3 mm").arg(snapshot->spacing[0], 0, 'f', 2)
              .arg(snapshot->spacing[1], 0, 'f', 2).arg(snapshot->spacing[2], 0, 'f', 2)
        : QStringLiteral("--");
}

std::shared_ptr<const VolumeSnapshot> MedicalDataController::volumeSnapshot() const
{
    std::lock_guard<std::mutex> guard(m_snapshotMutex);
    return m_volume;
}

std::shared_ptr<const VolumeSnapshot> MedicalDataController::projectionPairSnapshot() const
{
    std::lock_guard<std::mutex> guard(m_snapshotMutex);
    return m_projectionPair;
}

double MedicalDataController::displayWindowLevel() const
{
    return m_projectionUnsigned ? m_windowLevel + 32768.0 : m_windowLevel;
}

std::shared_ptr<const MaskSnapshot> MedicalDataController::maskSnapshot() const
{
    std::lock_guard<std::mutex> guard(m_snapshotMutex);
    return m_mask;
}

QVariantList MedicalDataController::volumeNodes() const
{
    QVariantList result;
    for (qsizetype index = 0; index < static_cast<qsizetype>(m_volumeNodes.size()); ++index) {
        const auto &node = m_volumeNodes[static_cast<std::size_t>(index)];
        QVariantMap item;
        item.insert(QStringLiteral("index"), index);
        item.insert(QStringLiteral("id"), node->id);
        item.insert(QStringLiteral("name"), node->name);
        item.insert(QStringLiteral("patientName"), node->patientName);
        item.insert(QStringLiteral("patientId"), node->patientId);
        item.insert(QStringLiteral("modality"), node->modality);
        item.insert(QStringLiteral("visible"), node->visible);
        item.insert(QStringLiteral("active"), index == m_selectedVolumeIndex);
        item.insert(QStringLiteral("projection"), false);
        item.insert(QStringLiteral("pairedProjection"), false);
        item.insert(QStringLiteral("segmentation"), node->mask != nullptr);
        item.insert(QStringLiteral("segmentationMethod"), node->segmentationMethod);
        item.insert(QStringLiteral("segmentationVoxelCount"), node->segmentationVoxelCount);
        item.insert(QStringLiteral("dimensions"), node->volume
            ? QStringLiteral("%1 x %2 x %3").arg(node->volume->dimensions[0])
                  .arg(node->volume->dimensions[1]).arg(node->volume->dimensions[2])
            : QStringLiteral("--"));
        result.append(item);
    }
    return result;
}

bool MedicalDataController::activeVolumeVisible() const
{
    return m_selectedVolumeIndex >= 0
        && m_selectedVolumeIndex < static_cast<int>(m_volumeNodes.size())
        && m_volumeNodes[static_cast<std::size_t>(m_selectedVolumeIndex)]->visible;
}

QString MedicalDataController::activeVolumeId() const
{
    if (m_selectedVolumeIndex < 0
        || m_selectedVolumeIndex >= static_cast<int>(m_volumeNodes.size()))
        return {};
    return m_volumeNodes[static_cast<std::size_t>(m_selectedVolumeIndex)]->id;
}

bool MedicalDataController::selectVolume(int index)
{
    if (index < 0 || index >= static_cast<int>(m_volumeNodes.size()))
        return false;
    updateActiveVolumeNode();
    activateVolumeNode(index);
    return true;
}

bool MedicalDataController::renameVolume(int index, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (index < 0 || index >= static_cast<int>(m_volumeNodes.size()) || trimmed.isEmpty())
        return false;
    m_volumeNodes[static_cast<std::size_t>(index)]->name = trimmed;
    if (index == m_selectedVolumeIndex) {
        m_seriesDescription = trimmed;
        emit dataChanged();
    }
    emit volumeNodesChanged();
    return true;
}

bool MedicalDataController::removeVolume(int index)
{
    if (index < 0 || index >= static_cast<int>(m_volumeNodes.size()))
        return false;
    const bool active = index == m_selectedVolumeIndex;
    m_volumeNodes.erase(m_volumeNodes.begin() + index);
    if (m_volumeNodes.empty())
        clearActiveVolume();
    else if (active)
        activateVolumeNode(std::min(index, static_cast<int>(m_volumeNodes.size()) - 1));
    else {
        if (index < m_selectedVolumeIndex)
            --m_selectedVolumeIndex;
        emit volumeNodesChanged();
    }
    return true;
}

bool MedicalDataController::setVolumeVisibility(int index, bool visible)
{
    if (index < 0 || index >= static_cast<int>(m_volumeNodes.size()))
        return false;
    m_volumeNodes[static_cast<std::size_t>(index)]->visible = visible;
    emit volumeNodesChanged();
    if (index == m_selectedVolumeIndex)
        emit dataChanged();
    return true;
}

bool MedicalDataController::importDicom(const QUrl &)
{
    setError(QStringLiteral("当前 MinGW 构建未链接 MinGW 版 ITK/GDCM，不能读取 DICOM。"));
    return false;
}

void MedicalDataController::importDicomAsync(const QUrl &source)
{
    importDicom(source);
}

bool MedicalDataController::selectSeries(int)
{
    setError(QStringLiteral("当前 MinGW 构建未启用 DICOM 序列加载。"));
    return false;
}

void MedicalDataController::selectSeriesAsync(int index)
{
    selectSeries(index);
}

bool MedicalDataController::exportDicomCopy(const QUrl &)
{
    setError(QStringLiteral("当前 MinGW 构建未启用 DICOM 导出后端。"));
    return false;
}

void MedicalDataController::loadDemoVolume()
{
    constexpr int sx = 192;
    constexpr int sy = 192;
    constexpr int sz = 160;
    auto snapshot = std::make_shared<VolumeSnapshot>();
    snapshot->dimensions = {sx, sy, sz};
    snapshot->spacing = {1.2, 1.2, 1.5};
    snapshot->pixels.resize(static_cast<std::size_t>(sx * sy * sz), -1000);
    for (int z = 0; z < sz; ++z)
        for (int y = 0; y < sy; ++y)
            for (int x = 0; x < sx; ++x) {
                const double dx = (x - sx * 0.5) / 36.0;
                const double dy = (y - sy * 0.5) / 28.0;
                const double dz = (z - sz * 0.5) / 35.0;
                if (dx * dx + dy * dy + dz * dz < 1.0)
                    snapshot->pixels[static_cast<std::size_t>((z * sy + y) * sx + x)] = 45;
            }
    resetMetadata();
    m_patientName = QStringLiteral("MinGW 演示患者");
    m_patientId = QStringLiteral("DEMO-CT-001");
    m_modality = QStringLiteral("CT");
    m_studyDescription = QStringLiteral("UI 兼容性演示");
    m_seriesDescription = QStringLiteral("无医学后端");
    m_sourcePath = QStringLiteral("内置演示数据");
    installVolume(std::move(snapshot), {});
    m_statusMessage = QStringLiteral("演示数据已载入；当前构建不提供医学渲染");
    emit statusChanged();
}

bool MedicalDataController::applyThreshold(double lower, double upper)
{
    const auto source = volumeSnapshot();
    if (!source || source->dimensions[2] <= 1 || lower > upper) {
        setError(QStringLiteral("阈值参数无效。"));
        return false;
    }
    auto mask = std::make_shared<MaskSnapshot>();
    mask->dimensions = source->dimensions;
    mask->spacing = source->spacing;
    mask->origin = source->origin;
    mask->direction = source->direction;
    mask->pixels.resize(source->pixels.size());
    for (std::size_t i = 0; i < source->pixels.size(); ++i)
        mask->pixels[i] = source->pixels[i] >= lower && source->pixels[i] <= upper ? 1 : 0;
    {
        std::lock_guard<std::mutex> guard(m_snapshotMutex);
        m_mask = std::move(mask);
    }
    updateActiveVolumeNode();
    ++m_segmentationRevision;
    m_segmentationMethod = QStringLiteral("阈值分割");
    m_segmentationVoxelCount = 0;
    for (const unsigned char value : m_mask->pixels)
        m_segmentationVoxelCount += value != 0;
    m_segmentationVolumeMl = static_cast<double>(m_segmentationVoxelCount)
        * source->spacing[0] * source->spacing[1] * source->spacing[2] / 1000.0;
    m_statusMessage = QStringLiteral("兼容模式阈值分割完成");
    m_errorMessage.clear();
    emit segmentationChanged();
    emit statusChanged();
    return true;
}

void MedicalDataController::applyThresholdAsync(double lower, double upper)
{
    applyThreshold(lower, upper);
}

bool MedicalDataController::setRegionGrowingSeed(int seedX, int seedY, int seedZ)
{
    const auto snapshot = volumeSnapshot();
    if (!snapshot || snapshot->dimensions[2] <= 1
        || seedX < 0 || seedY < 0 || seedZ < 0
        || seedX >= snapshot->dimensions[0]
        || seedY >= snapshot->dimensions[1]
        || seedZ >= snapshot->dimensions[2]) {
        setError(QStringLiteral("种子点不在当前 CT 体数据范围内。"));
        return false;
    }
    const auto offset = static_cast<std::size_t>(
        (seedZ * snapshot->dimensions[1] + seedY) * snapshot->dimensions[0] + seedX);
    m_regionGrowingSeed = {seedX, seedY, seedZ};
    m_regionGrowingSeedValue = snapshot->pixels[offset];
    m_regionGrowingSeedValid = true;
    emit regionGrowingSeedChanged();
    return true;
}

void MedicalDataController::clearRegionGrowingSeed()
{
    if (!m_regionGrowingSeedValid)
        return;
    m_regionGrowingSeed = {-1, -1, -1};
    m_regionGrowingSeedValue = 0;
    m_regionGrowingSeedValid = false;
    m_errorMessage.clear();
    m_statusMessage = QStringLiteral("种子点已清除");
    emit regionGrowingSeedChanged();
    emit statusChanged();
}

bool MedicalDataController::applyRegionGrowingFromSeed(double lower, double upper)
{
    if (!m_regionGrowingSeedValid) {
        setError(QStringLiteral("请先在切片中选择种子点。"));
        return false;
    }
    if (!std::isfinite(lower) || !std::isfinite(upper) || lower > upper) {
        setError(QStringLiteral("兼容模式种子生长的 HU 范围无效。"));
        return false;
    }
    return applyRegionGrowing(m_regionGrowingSeed[0], m_regionGrowingSeed[1],
                              m_regionGrowingSeed[2], lower, upper);
}

void MedicalDataController::applyRegionGrowingFromSeedAsync(
    double lower, double upper, bool fullyConnected)
{
    Q_UNUSED(fullyConnected)
    applyRegionGrowingFromSeed(lower, upper);
}

bool MedicalDataController::applyRegionGrowing(int seedX, int seedY, int seedZ,
                                               double lower, double upper,
                                               bool fullyConnected)
{
    Q_UNUSED(fullyConnected)
    const auto source = volumeSnapshot();
    if (!source || !std::isfinite(lower) || !std::isfinite(upper) || lower > upper
        || seedX < 0 || seedY < 0 || seedZ < 0
        || seedX >= source->dimensions[0] || seedY >= source->dimensions[1]
        || seedZ >= source->dimensions[2]) {
        setError(QStringLiteral("兼容模式种子点或 HU 范围无效。"));
        return false;
    }
    const auto seedOffset = static_cast<std::size_t>(
        (seedZ * source->dimensions[1] + seedY) * source->dimensions[0] + seedX);
    const short seedValue = source->pixels[seedOffset];
    if (seedValue < lower || seedValue > upper) {
        setError(QStringLiteral("种子点为 %1 HU，不在当前生长范围内。")
                     .arg(seedValue));
        return false;
    }
    auto mask = std::make_shared<MaskSnapshot>();
    mask->dimensions = source->dimensions;
    mask->spacing = source->spacing;
    mask->origin = source->origin;
    mask->direction = source->direction;
    mask->pixels.resize(source->pixels.size());
    for (std::size_t index = 0; index < source->pixels.size(); ++index)
        mask->pixels[index] = source->pixels[index] >= lower
            && source->pixels[index] <= upper ? 1 : 0;
    {
        std::lock_guard<std::mutex> guard(m_snapshotMutex);
        m_mask = std::move(mask);
    }
    updateActiveVolumeNode();
    ++m_segmentationRevision;
    m_segmentationMethod = QStringLiteral("种子生长（6 邻域）");
    m_segmentationVoxelCount = 0;
    for (const unsigned char value : m_mask->pixels)
        m_segmentationVoxelCount += value != 0;
    m_segmentationVolumeMl = static_cast<double>(m_segmentationVoxelCount)
        * source->spacing[0] * source->spacing[1] * source->spacing[2] / 1000.0;
    m_statusMessage = QStringLiteral("兼容模式已生成近似种子生长结果。");
    m_errorMessage.clear();
    emit segmentationChanged();
    emit statusChanged();
    return true;
}

void MedicalDataController::clearSegmentation()
{
    {
        std::lock_guard<std::mutex> guard(m_snapshotMutex);
        m_mask.reset();
    }
    m_segmentationMethod.clear();
    m_segmentationVoxelCount = 0;
    m_segmentationVolumeMl = 0.0;
    updateActiveVolumeNode();
    ++m_segmentationRevision;
    emit segmentationChanged();
}

bool MedicalDataController::commitSeriesLoad(SeriesLoadResult result)
{
    Q_UNUSED(result)
    return false;
}

bool MedicalDataController::commitSegmentation(
    SegmentationResult result, int expectedDatasetRevision, const QString &successMessage)
{
    Q_UNUSED(result)
    Q_UNUSED(expectedDatasetRevision)
    Q_UNUSED(successMessage)
    return false;
}

double MedicalDataController::estimateDistanceMm(int viewType, double pixelDx, double pixelDy,
                                                 double viewportWidth, double viewportHeight,
                                                 bool pairedProjection) const
{
    const auto snapshot = pairedProjection ? projectionPairSnapshot() : volumeSnapshot();
    if (!snapshot || viewportWidth <= 0.0 || viewportHeight <= 0.0)
        return 0.0;
    int axisX = viewType == 2 ? 1 : 0;
    int axisY = viewType == 0 ? 1 : 2;
    const double scale = std::min(viewportWidth / snapshot->dimensions[axisX],
                                  viewportHeight / snapshot->dimensions[axisY]);
    const double dx = pixelDx / scale * snapshot->spacing[axisX];
    const double dy = pixelDy / scale * snapshot->spacing[axisY];
    return std::sqrt(dx * dx + dy * dy);
}

void MedicalDataController::setWindowWidth(double value)
{
    value = qMax(1.0, value);
    if (!qFuzzyCompare(value, m_windowWidth)) {
        m_windowWidth = value;
        updateActiveVolumeNode();
        emit windowingChanged();
    }
}

void MedicalDataController::setWindowLevel(double value)
{
    if (!qFuzzyCompare(value, m_windowLevel)) {
        m_windowLevel = value;
        updateActiveVolumeNode();
        emit windowingChanged();
    }
}

void MedicalDataController::setBusy(bool busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

void MedicalDataController::setError(const QString &message)
{
    m_errorMessage = message;
    m_statusMessage.clear();
    emit statusChanged();
}

void MedicalDataController::installVolume(std::shared_ptr<VolumeSnapshot> snapshot,
                                          const QStringList &sourceFiles,
                                          std::shared_ptr<VolumeSnapshot> pairSnapshot,
                                          const QStringList &pairSourceFiles)
{
    Q_UNUSED(pairSourceFiles)
    auto node = std::make_shared<LoadedVolumeNode>();
    node->id = !m_sourcePath.isEmpty() ? m_sourcePath
                                       : QStringLiteral("volume-%1").arg(m_datasetRevision + 1);
    node->name = m_seriesDescription;
    node->patientName = m_patientName;
    node->patientId = m_patientId;
    node->modality = m_modality;
    node->sourcePath = m_sourcePath;
    node->sourceFiles = sourceFiles;
    node->volume = std::move(snapshot);
    node->projectionPair = std::move(pairSnapshot);
    node->windowWidth = m_windowWidth;
    node->windowLevel = m_windowLevel;
    node->segmentationMethod = m_segmentationMethod;
    node->segmentationVoxelCount = m_segmentationVoxelCount;
    node->segmentationVolumeMl = m_segmentationVolumeMl;
    node->projectionUnsigned = m_projectionUnsigned;
    node->projectionInverted = m_projectionInverted;
    node->projectionPairInverted = m_projectionPairInverted;

    int targetIndex = -1;
    for (int index = 0; index < static_cast<int>(m_volumeNodes.size()); ++index) {
        if (m_volumeNodes[static_cast<std::size_t>(index)]->id == node->id) {
            targetIndex = index;
            node->name = m_volumeNodes[static_cast<std::size_t>(index)]->name;
            node->visible = m_volumeNodes[static_cast<std::size_t>(index)]->visible;
            break;
        }
    }
    if (targetIndex < 0) {
        m_volumeNodes.push_back(node);
        targetIndex = static_cast<int>(m_volumeNodes.size()) - 1;
    } else {
        m_volumeNodes[static_cast<std::size_t>(targetIndex)] = node;
    }
    activateVolumeNode(targetIndex);
}

void MedicalDataController::updateActiveVolumeNode()
{
    if (m_selectedVolumeIndex < 0
        || m_selectedVolumeIndex >= static_cast<int>(m_volumeNodes.size()))
        return;
    auto &node = m_volumeNodes[static_cast<std::size_t>(m_selectedVolumeIndex)];
    std::lock_guard<std::mutex> guard(m_snapshotMutex);
    node->volume = m_volume;
    node->projectionPair = m_projectionPair;
    node->mask = m_mask;
    node->windowWidth = m_windowWidth;
    node->windowLevel = m_windowLevel;
    node->projectionInverted = m_projectionInverted;
    node->projectionPairInverted = m_projectionPairInverted;
}

void MedicalDataController::activateVolumeNode(int index)
{
    if (index < 0 || index >= static_cast<int>(m_volumeNodes.size()))
        return;
    const auto &node = m_volumeNodes[static_cast<std::size_t>(index)];
    {
        std::lock_guard<std::mutex> guard(m_snapshotMutex);
        m_volume = node->volume;
        m_projectionPair = node->projectionPair;
        m_mask = node->mask;
        m_sourceFiles = node->sourceFiles;
    }
    m_selectedVolumeIndex = index;
    m_patientName = node->patientName;
    m_patientId = node->patientId;
    m_modality = node->modality;
    m_seriesDescription = node->name;
    m_sourcePath = node->sourcePath;
    m_projectionUnsigned = node->projectionUnsigned;
    m_projectionInverted = node->projectionInverted;
    m_projectionPairInverted = node->projectionPairInverted;
    m_windowWidth = node->windowWidth;
    m_windowLevel = node->windowLevel;
    m_segmentationMethod = node->segmentationMethod;
    m_segmentationVoxelCount = node->segmentationVoxelCount;
    m_segmentationVolumeMl = node->segmentationVolumeMl;
    ++m_datasetRevision;
    ++m_segmentationRevision;
    m_regionGrowingSeed = {-1, -1, -1};
    m_regionGrowingSeedValue = 0;
    m_regionGrowingSeedValid = false;
    emit dataChanged();
    emit segmentationChanged();
    emit regionGrowingSeedChanged();
    emit windowingChanged();
    emit volumeNodesChanged();
}

void MedicalDataController::clearActiveVolume()
{
    {
        std::lock_guard<std::mutex> guard(m_snapshotMutex);
        m_volume.reset();
        m_projectionPair.reset();
        m_mask.reset();
    }
    m_selectedVolumeIndex = -1;
    resetMetadata();
    ++m_datasetRevision;
    ++m_segmentationRevision;
    m_segmentationMethod.clear();
    m_segmentationVoxelCount = 0;
    m_segmentationVolumeMl = 0.0;
    emit dataChanged();
    emit segmentationChanged();
    emit volumeNodesChanged();
}

void MedicalDataController::resetMetadata()
{
    m_patientName = QStringLiteral("未提供");
    m_patientId = QStringLiteral("未提供");
    m_patientSex = QStringLiteral("--");
    m_patientBirthDate = QStringLiteral("--");
    m_modality = QStringLiteral("--");
    m_studyDescription = QStringLiteral("未命名检查");
    m_studyDate = QStringLiteral("--");
    m_seriesDescription = QStringLiteral("未命名序列");
    m_projectionViewLabel.clear();
    m_projectionPairViewLabel.clear();
    m_patientOrientation.clear();
    m_projectionPairOrientation.clear();
    m_imageType.clear();
    m_sopClassName.clear();
    m_projectionPairImageType.clear();
    m_projectionPairSopClassName.clear();
    m_projectionUnsigned = false;
    m_projectionInverted = false;
    m_projectionPairInverted = false;
    m_sourcePath.clear();
    m_sourceFiles.clear();
}
