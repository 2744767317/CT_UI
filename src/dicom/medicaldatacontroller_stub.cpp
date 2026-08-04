#include "medicaldatacontroller.h"

#include <QDir>

#include <algorithm>
#include <cmath>

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

std::shared_ptr<const MaskSnapshot> MedicalDataController::maskSnapshot() const
{
    std::lock_guard<std::mutex> guard(m_snapshotMutex);
    return m_mask;
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

bool MedicalDataController::exportDicomCopy(const QUrl &)
{
    setError(QStringLiteral("当前 MinGW 构建未启用 DICOM 导出后端。"));
    return false;
}

void MedicalDataController::loadDemoVolume()
{
    constexpr int sx = 96;
    constexpr int sy = 96;
    constexpr int sz = 80;
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
                    snapshot->pixels[static_cast<std::size_t>((z * sy + y) * sx + x)] = 50;
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
    if (!source || lower > upper) {
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
    ++m_segmentationRevision;
    emit segmentationChanged();
    return true;
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
    emit regionGrowingSeedChanged();
}

bool MedicalDataController::applyRegionGrowingFromSeed(double lower, double upper)
{
    if (!m_regionGrowingSeedValid) {
        setError(QStringLiteral("请先在切片中选择种子点。"));
        return false;
    }
    return applyRegionGrowing(m_regionGrowingSeed[0], m_regionGrowingSeed[1],
                              m_regionGrowingSeed[2], lower, upper);
}

bool MedicalDataController::applyRegionGrowing(int, int, int, double, double)
{
    setError(QStringLiteral("MinGW UI 模式不提供 ITK 种子生长。"));
    return false;
}

void MedicalDataController::clearSegmentation()
{
    {
        std::lock_guard<std::mutex> guard(m_snapshotMutex);
        m_mask.reset();
    }
    ++m_segmentationRevision;
    emit segmentationChanged();
}

double MedicalDataController::estimateDistanceMm(int viewType, double pixelDx, double pixelDy,
                                                 double viewportWidth, double viewportHeight) const
{
    const auto snapshot = volumeSnapshot();
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
        emit windowingChanged();
    }
}

void MedicalDataController::setWindowLevel(double value)
{
    if (!qFuzzyCompare(value, m_windowLevel)) {
        m_windowLevel = value;
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
                                          const QStringList &sourceFiles)
{
    {
        std::lock_guard<std::mutex> guard(m_snapshotMutex);
        m_volume = std::move(snapshot);
        m_mask.reset();
        m_sourceFiles = sourceFiles;
    }
    ++m_datasetRevision;
    ++m_segmentationRevision;
    m_regionGrowingSeed = {-1, -1, -1};
    m_regionGrowingSeedValue = 0;
    m_regionGrowingSeedValid = false;
    emit dataChanged();
    emit segmentationChanged();
    emit regionGrowingSeedChanged();
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
    m_sourcePath.clear();
    m_sourceFiles.clear();
}
