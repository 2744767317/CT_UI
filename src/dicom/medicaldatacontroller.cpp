#include "medicaldatacontroller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <itkBinaryThresholdImageFilter.h>
#include <itkConnectedThresholdImageFilter.h>
#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageSeriesReader.h>
#include <itkMetaDataObject.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace {

using Image3D = itk::Image<short, 3>;
using Image2D = itk::Image<short, 2>;
using MaskImage = itk::Image<unsigned char, 3>;

QString localPath(const QUrl &url)
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

QString dicomText(const itk::MetaDataDictionary &dictionary, const char *tag)
{
    std::string value;
    if (!itk::ExposeMetaData<std::string>(dictionary, tag, value))
        return {};
    return QString::fromStdString(value).trimmed();
}

double dicomNumber(const itk::MetaDataDictionary &dictionary, const char *tag, double fallback)
{
    QString value = dicomText(dictionary, tag);
    value = value.section(QChar(u'\\'), 0, 0).trimmed();
    bool ok = false;
    const double result = value.toDouble(&ok);
    return ok ? result : fallback;
}

std::shared_ptr<VolumeSnapshot> snapshotFrom3D(const Image3D *image)
{
    auto snapshot = std::make_shared<VolumeSnapshot>();
    const auto region = image->GetLargestPossibleRegion();
    const auto size = region.GetSize();
    const auto spacing = image->GetSpacing();
    const auto origin = image->GetOrigin();
    const auto direction = image->GetDirection();

    for (unsigned int axis = 0; axis < 3; ++axis) {
        snapshot->dimensions[axis] = static_cast<int>(size[axis]);
        snapshot->spacing[axis] = spacing[axis];
        snapshot->origin[axis] = origin[axis];
        for (unsigned int column = 0; column < 3; ++column)
            snapshot->direction[axis * 3 + column] = direction(axis, column);
    }

    const auto count = static_cast<std::size_t>(size[0] * size[1] * size[2]);
    snapshot->pixels.assign(image->GetBufferPointer(), image->GetBufferPointer() + count);
    return snapshot;
}

std::shared_ptr<VolumeSnapshot> snapshotFrom2D(const Image2D *image)
{
    auto snapshot = std::make_shared<VolumeSnapshot>();
    const auto size = image->GetLargestPossibleRegion().GetSize();
    const auto spacing = image->GetSpacing();
    const auto origin = image->GetOrigin();
    const auto direction = image->GetDirection();

    snapshot->dimensions = {static_cast<int>(size[0]), static_cast<int>(size[1]), 1};
    snapshot->spacing = {spacing[0], spacing[1], 1.0};
    snapshot->origin = {origin[0], origin[1], 0.0};
    snapshot->direction = {
        direction(0, 0), direction(0, 1), 0.0,
        direction(1, 0), direction(1, 1), 0.0,
        0.0, 0.0, 1.0
    };
    const auto count = static_cast<std::size_t>(size[0] * size[1]);
    snapshot->pixels.assign(image->GetBufferPointer(), image->GetBufferPointer() + count);
    return snapshot;
}

Image3D::Pointer itkImageFromSnapshot(const VolumeSnapshot &snapshot)
{
    auto image = Image3D::New();
    Image3D::RegionType region;
    Image3D::IndexType start;
    start.Fill(0);
    Image3D::SizeType size;
    Image3D::SpacingType spacing;
    Image3D::PointType origin;
    Image3D::DirectionType direction;

    for (unsigned int axis = 0; axis < 3; ++axis) {
        size[axis] = static_cast<Image3D::SizeType::SizeValueType>(snapshot.dimensions[axis]);
        spacing[axis] = snapshot.spacing[axis];
        origin[axis] = snapshot.origin[axis];
        for (unsigned int column = 0; column < 3; ++column)
            direction(axis, column) = snapshot.direction[axis * 3 + column];
    }

    region.SetIndex(start);
    region.SetSize(size);
    image->SetRegions(region);
    image->SetSpacing(spacing);
    image->SetOrigin(origin);
    image->SetDirection(direction);
    image->Allocate();
    std::memcpy(image->GetBufferPointer(), snapshot.pixels.data(),
                snapshot.pixels.size() * sizeof(short));
    return image;
}

std::shared_ptr<MaskSnapshot> maskSnapshotFromItk(const MaskImage *image,
                                                 const VolumeSnapshot &source)
{
    auto mask = std::make_shared<MaskSnapshot>();
    mask->dimensions = source.dimensions;
    mask->spacing = source.spacing;
    mask->origin = source.origin;
    mask->direction = source.direction;
    mask->pixels.assign(image->GetBufferPointer(),
                        image->GetBufferPointer() + source.pixels.size());
    return mask;
}

} // namespace

MedicalDataController::MedicalDataController(QObject *parent)
    : QObject(parent)
{
    resetMetadata();
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
    if (!snapshot)
        return QStringLiteral("--");
    return QStringLiteral("%1 × %2 × %3")
        .arg(snapshot->dimensions[0])
        .arg(snapshot->dimensions[1])
        .arg(snapshot->dimensions[2]);
}

QString MedicalDataController::spacingText() const
{
    const auto snapshot = volumeSnapshot();
    if (!snapshot)
        return QStringLiteral("--");
    return QStringLiteral("%1 × %2 × %3 mm")
        .arg(snapshot->spacing[0], 0, 'f', 2)
        .arg(snapshot->spacing[1], 0, 'f', 2)
        .arg(snapshot->spacing[2], 0, 'f', 2);
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

bool MedicalDataController::importDicom(const QUrl &source)
{
    const QString path = QDir::toNativeSeparators(localPath(source));
    const QFileInfo sourceInfo(path);
    if (!sourceInfo.exists()) {
        setError(QStringLiteral("所选 DICOM 路径不存在。"));
        return false;
    }

    setBusy(true);
    m_errorMessage.clear();
    emit statusChanged();

    try {
        auto imageIO = itk::GDCMImageIO::New();
        std::shared_ptr<VolumeSnapshot> snapshot;
        QStringList sourceFiles;

        if (sourceInfo.isDir()) {
            auto names = itk::GDCMSeriesFileNames::New();
            names->SetUseSeriesDetails(true);
            names->SetDirectory(path.toStdString());
            const auto &seriesUids = names->GetSeriesUIDs();
            if (seriesUids.empty())
                throw std::runtime_error("No DICOM series found");

            std::vector<std::string> bestSeries;
            for (const auto &uid : seriesUids) {
                auto files = names->GetFileNames(uid);
                if (files.size() > bestSeries.size())
                    bestSeries = std::move(files);
            }

            auto reader = itk::ImageSeriesReader<Image3D>::New();
            reader->SetImageIO(imageIO);
            reader->SetFileNames(bestSeries);
            reader->ForceOrthogonalDirectionOff();
            reader->Update();
            snapshot = snapshotFrom3D(reader->GetOutput());
            for (const auto &file : bestSeries)
                sourceFiles.append(QString::fromStdString(file));
        } else {
            imageIO->SetFileName(path.toStdString());
            imageIO->ReadImageInformation();
            if (imageIO->GetNumberOfDimensions() >= 3 && imageIO->GetDimensions(2) > 1) {
                auto reader = itk::ImageFileReader<Image3D>::New();
                reader->SetImageIO(imageIO);
                reader->SetFileName(path.toStdString());
                reader->Update();
                snapshot = snapshotFrom3D(reader->GetOutput());
            } else {
                auto reader = itk::ImageFileReader<Image2D>::New();
                reader->SetImageIO(imageIO);
                reader->SetFileName(path.toStdString());
                reader->Update();
                snapshot = snapshotFrom2D(reader->GetOutput());
            }
            sourceFiles.append(path);
        }

        const auto &dictionary = imageIO->GetMetaDataDictionary();
        resetMetadata();
        m_patientName = dicomText(dictionary, "0010|0010").replace(QChar(u'^'), QChar(u' '));
        m_patientId = dicomText(dictionary, "0010|0020");
        m_patientSex = dicomText(dictionary, "0010|0040");
        m_patientBirthDate = dicomText(dictionary, "0010|0030");
        m_modality = dicomText(dictionary, "0008|0060");
        m_studyDescription = dicomText(dictionary, "0008|1030");
        m_seriesDescription = dicomText(dictionary, "0008|103e");
        m_studyDate = dicomText(dictionary, "0008|0020");
        m_windowWidth = dicomNumber(dictionary, "0028|1051", 0.0);
        m_windowLevel = dicomNumber(dictionary, "0028|1050", 0.0);

        if (m_windowWidth <= 0.0 && snapshot && !snapshot->pixels.empty()) {
            const auto range = std::minmax_element(snapshot->pixels.begin(), snapshot->pixels.end());
            m_windowWidth = qMax(1.0, static_cast<double>(*range.second - *range.first));
            m_windowLevel = (static_cast<double>(*range.second) + *range.first) * 0.5;
        }

        m_sourcePath = QDir::toNativeSeparators(sourceInfo.absoluteFilePath());
        installVolume(std::move(snapshot), sourceFiles);
        m_statusMessage = QStringLiteral("DICOM 已载入并完成像素与标签校验");
        emit windowingChanged();
        emit statusChanged();
        setBusy(false);
        return true;
    } catch (const itk::ExceptionObject &error) {
        setBusy(false);
        setError(QStringLiteral("DICOM 读取失败：%1").arg(QString::fromUtf8(error.GetDescription())));
    } catch (const std::exception &error) {
        setBusy(false);
        setError(QStringLiteral("DICOM 读取失败：%1").arg(QString::fromUtf8(error.what())));
    }
    return false;
}

bool MedicalDataController::exportDicomCopy(const QUrl &destination)
{
    const QString path = localPath(destination);
    if (m_sourceFiles.isEmpty()) {
        setError(QStringLiteral("当前数据不是来自 DICOM，无法导出原始 DICOM 副本。"));
        return false;
    }

    QDir directory(path);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        setError(QStringLiteral("无法创建导出目录。"));
        return false;
    }

    int copied = 0;
    for (int index = 0; index < m_sourceFiles.size(); ++index) {
        const QFileInfo source(m_sourceFiles.at(index));
        const QString targetName = QStringLiteral("%1_%2")
            .arg(index + 1, 5, 10, QChar(u'0'))
            .arg(source.fileName());
        const QString target = directory.filePath(targetName);
        if (QFile::exists(target))
            QFile::remove(target);
        if (QFile::copy(source.absoluteFilePath(), target))
            ++copied;
    }

    if (copied != m_sourceFiles.size()) {
        setError(QStringLiteral("DICOM 导出不完整：已复制 %1 / %2 个实例。")
                     .arg(copied)
                     .arg(m_sourceFiles.size()));
        return false;
    }

    m_statusMessage = QStringLiteral("已导出 %1 个原始 DICOM 实例").arg(copied);
    m_errorMessage.clear();
    emit statusChanged();
    return true;
}

void MedicalDataController::loadDemoVolume()
{
    constexpr int sizeX = 192;
    constexpr int sizeY = 192;
    constexpr int sizeZ = 160;
    auto snapshot = std::make_shared<VolumeSnapshot>();
    snapshot->dimensions = {sizeX, sizeY, sizeZ};
    snapshot->spacing = {0.8, 0.8, 1.0};
    snapshot->origin = {-76.8, -76.8, -80.0};
    snapshot->pixels.resize(static_cast<std::size_t>(sizeX * sizeY * sizeZ), -1000);

    for (int z = 0; z < sizeZ; ++z) {
        for (int y = 0; y < sizeY; ++y) {
            for (int x = 0; x < sizeX; ++x) {
                const double nx = (x - sizeX * 0.5) / 70.0;
                const double ny = (y - sizeY * 0.5) / 55.0;
                const double nz = (z - sizeZ * 0.5) / 74.0;
                const double body = nx * nx + ny * ny + nz * nz;
                short value = -1000;
                if (body < 1.0)
                    value = 45;
                const double spine = std::pow((x - sizeX * 0.5) / 12.0, 2.0)
                    + std::pow((y - sizeY * 0.60) / 10.0, 2.0);
                if (body < 0.88 && spine < 1.0)
                    value = 850;
                const double leftLung = std::pow((x - sizeX * 0.37) / 25.0, 2.0)
                    + std::pow((y - sizeY * 0.43) / 32.0, 2.0);
                const double rightLung = std::pow((x - sizeX * 0.63) / 25.0, 2.0)
                    + std::pow((y - sizeY * 0.43) / 32.0, 2.0);
                if (body < 0.9 && (leftLung < 1.0 || rightLung < 1.0) && std::abs(nz) < 0.55)
                    value = -720;
                snapshot->pixels[static_cast<std::size_t>((z * sizeY + y) * sizeX + x)] = value;
            }
        }
    }

    resetMetadata();
    m_patientName = QStringLiteral("演示患者");
    m_patientId = QStringLiteral("DEMO-CT-001");
    m_patientSex = QStringLiteral("O");
    m_modality = QStringLiteral("CT");
    m_studyDescription = QStringLiteral("CT 三维工作站演示数据");
    m_seriesDescription = QStringLiteral("Synthetic CT Volume");
    m_sourcePath = QStringLiteral("内置非临床演示体数据");
    m_windowWidth = 400.0;
    m_windowLevel = 40.0;
    installVolume(std::move(snapshot), {});
    m_statusMessage = QStringLiteral("演示体数据已载入，不得用于临床诊断");
    emit windowingChanged();
    emit statusChanged();
}

bool MedicalDataController::applyThreshold(double lower, double upper)
{
    const auto snapshot = volumeSnapshot();
    if (!snapshot || lower > upper) {
        setError(QStringLiteral("阈值分割参数无效或尚未载入影像。"));
        return false;
    }

    try {
        using Filter = itk::BinaryThresholdImageFilter<Image3D, MaskImage>;
        auto filter = Filter::New();
        filter->SetInput(itkImageFromSnapshot(*snapshot));
        filter->SetLowerThreshold(static_cast<short>(std::clamp(lower, -32768.0, 32767.0)));
        filter->SetUpperThreshold(static_cast<short>(std::clamp(upper, -32768.0, 32767.0)));
        filter->SetInsideValue(1);
        filter->SetOutsideValue(0);
        filter->Update();

        {
            std::lock_guard<std::mutex> guard(m_snapshotMutex);
            m_mask = maskSnapshotFromItk(filter->GetOutput(), *snapshot);
        }
        ++m_segmentationRevision;
        m_statusMessage = QStringLiteral("阈值分割完成：%1 至 %2 HU").arg(lower).arg(upper);
        m_errorMessage.clear();
        emit segmentationChanged();
        emit statusChanged();
        return true;
    } catch (const itk::ExceptionObject &error) {
        setError(QStringLiteral("阈值分割失败：%1").arg(QString::fromUtf8(error.GetDescription())));
        return false;
    }
}

bool MedicalDataController::applyRegionGrowing(int seedX, int seedY, int seedZ,
                                               double lower, double upper)
{
    const auto snapshot = volumeSnapshot();
    if (!snapshot || lower > upper || seedX < 0 || seedY < 0 || seedZ < 0
        || seedX >= snapshot->dimensions[0] || seedY >= snapshot->dimensions[1]
        || seedZ >= snapshot->dimensions[2]) {
        setError(QStringLiteral("区域生长的种子点或阈值范围无效。"));
        return false;
    }

    try {
        using Filter = itk::ConnectedThresholdImageFilter<Image3D, MaskImage>;
        auto filter = Filter::New();
        filter->SetInput(itkImageFromSnapshot(*snapshot));
        filter->SetLower(static_cast<short>(std::clamp(lower, -32768.0, 32767.0)));
        filter->SetUpper(static_cast<short>(std::clamp(upper, -32768.0, 32767.0)));
        filter->SetReplaceValue(1);
        Image3D::IndexType seed;
        seed[0] = seedX;
        seed[1] = seedY;
        seed[2] = seedZ;
        filter->SetSeed(seed);
        filter->Update();

        {
            std::lock_guard<std::mutex> guard(m_snapshotMutex);
            m_mask = maskSnapshotFromItk(filter->GetOutput(), *snapshot);
        }
        ++m_segmentationRevision;
        m_statusMessage = QStringLiteral("种子生长完成：(%1, %2, %3)")
                              .arg(seedX).arg(seedY).arg(seedZ);
        m_errorMessage.clear();
        emit segmentationChanged();
        emit statusChanged();
        return true;
    } catch (const itk::ExceptionObject &error) {
        setError(QStringLiteral("种子生长失败：%1").arg(QString::fromUtf8(error.GetDescription())));
        return false;
    }
}

void MedicalDataController::clearSegmentation()
{
    {
        std::lock_guard<std::mutex> guard(m_snapshotMutex);
        m_mask.reset();
    }
    ++m_segmentationRevision;
    m_statusMessage = QStringLiteral("分割结果已清除");
    emit segmentationChanged();
    emit statusChanged();
}

double MedicalDataController::estimateDistanceMm(int viewType, double pixelDx, double pixelDy,
                                                 double viewportWidth, double viewportHeight) const
{
    const auto snapshot = volumeSnapshot();
    if (!snapshot || viewportWidth <= 0.0 || viewportHeight <= 0.0)
        return 0.0;

    int axisX = 0;
    int axisY = 1;
    if (viewType == 1) {
        axisX = 0;
        axisY = 2;
    } else if (viewType == 2) {
        axisX = 1;
        axisY = 2;
    }

    const double scale = std::min(viewportWidth / snapshot->dimensions[axisX],
                                  viewportHeight / snapshot->dimensions[axisY]);
    if (scale <= 0.0)
        return 0.0;
    const double dx = pixelDx / scale * snapshot->spacing[axisX];
    const double dy = pixelDy / scale * snapshot->spacing[axisY];
    return std::sqrt(dx * dx + dy * dy);
}

void MedicalDataController::setWindowWidth(double value)
{
    value = qMax(1.0, value);
    if (qFuzzyCompare(m_windowWidth, value))
        return;
    m_windowWidth = value;
    emit windowingChanged();
}

void MedicalDataController::setWindowLevel(double value)
{
    if (qFuzzyCompare(m_windowLevel, value))
        return;
    m_windowLevel = value;
    emit windowingChanged();
}

void MedicalDataController::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
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
    emit dataChanged();
    emit segmentationChanged();
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
