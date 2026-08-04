#include "medicaldatacontroller.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QVariantMap>
#include <QFutureWatcher>

#include <QtConcurrent/QtConcurrentRun>

#include <itkBinaryThresholdImageFilter.h>
#include <itkConnectedThresholdImageFilter.h>
#include <itkGDCMImageIO.h>
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageSeriesReader.h>
#include <itkMetaDataObject.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

struct DicomSeriesCandidate
{
    struct Instance
    {
        QString path;
        int instanceNumber = 0;
        double imagePosition = std::numeric_limits<double>::quiet_NaN();
    };

    QString patientName;
    QString patientId;
    QString patientSex;
    QString patientBirthDate;
    QString studyDescription;
    QString studyDate;
    QString studyUid;
    QString seriesDescription;
    QString seriesUid;
    QString modality;
    std::vector<Instance> instances;
    double windowWidth = 0.0;
    double windowLevel = 0.0;
    int columns = 0;
    int rows = 0;
    int frames = 1;
    bool projection = false;
    bool unsignedPixels = false;
    bool volume = false;
};

namespace {

using Image3D = itk::Image<short, 3>;
using Image2D = itk::Image<short, 2>;
using UnsignedImage2D = itk::Image<unsigned short, 2>;
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

int dicomInteger(const itk::MetaDataDictionary &dictionary, const char *tag, int fallback)
{
    bool ok = false;
    const int value = dicomText(dictionary, tag).toInt(&ok);
    return ok ? value : fallback;
}

bool isProjectionModality(const QString &modality)
{
    static const QSet<QString> projectionModalities {
        QStringLiteral("CR"), QStringLiteral("DX"), QStringLiteral("MG"),
        QStringLiteral("RF"), QStringLiteral("XA"), QStringLiteral("XC")
    };
    return projectionModalities.contains(modality.toUpper());
}

struct DicomScanResult
{
    std::vector<std::shared_ptr<DicomSeriesCandidate>> candidates;
    QString error;
};

DicomScanResult scanDicomDirectory(const QString &path)
{
    DicomScanResult result;
    const QFileInfo sourceInfo(path);
    if (!sourceInfo.exists()) {
        result.error = QStringLiteral("所选 DICOM 路径不存在。");
        return result;
    }

    try {
        QStringList candidateFiles;
        if (sourceInfo.isDir()) {
            QDirIterator iterator(path, QDir::Files | QDir::Readable,
                                  QDirIterator::Subdirectories);
            while (iterator.hasNext())
                candidateFiles.append(iterator.next());
        } else {
            candidateFiles.append(path);
        }

        QHash<QString, std::shared_ptr<DicomSeriesCandidate>> groupedSeries;
        for (const QString &filePath : candidateFiles) {
            const QFileInfo fileInfo(filePath);
            const QString suffix = fileInfo.suffix().toLower();
            const QString fileName = fileInfo.fileName().toUpper();
            if (fileInfo.size() < 132 || suffix == QStringLiteral("zip")
                || suffix == QStringLiteral("xml") || fileName == QStringLiteral("DICOMDIR")
                || fileName == QStringLiteral("LOCKFILE") || fileName == QStringLiteral("VERSION")) {
                continue;
            }

            try {
                auto probe = itk::GDCMImageIO::New();
                const std::string nativeFile = QDir::toNativeSeparators(filePath).toStdString();
                if (!probe->CanReadFile(nativeFile.c_str()))
                    continue;
                probe->SetFileName(nativeFile);
                probe->ReadImageInformation();
                if (probe->GetNumberOfDimensions() < 2 || probe->GetDimensions(0) == 0
                    || probe->GetDimensions(1) == 0) {
                    continue;
                }

                const auto &dictionary = probe->GetMetaDataDictionary();
                const QString modality = dicomText(dictionary, "0008|0060").toUpper();
                if (modality.isEmpty())
                    continue;

                const QString patientId = dicomText(dictionary, "0010|0020");
                const QString studyUid = dicomText(dictionary, "0020|000d");
                QString seriesUid = dicomText(dictionary, "0020|000e");
                if (seriesUid.isEmpty())
                    seriesUid = fileInfo.absolutePath();
                const bool projection = isProjectionModality(modality);
                QString groupKey = patientId + QChar(u'|') + studyUid + QChar(u'|') + seriesUid;
                if (projection) {
                    QString sopUid = dicomText(dictionary, "0008|0018");
                    if (sopUid.isEmpty())
                        sopUid = filePath;
                    groupKey += QChar(u'|') + sopUid;
                }

                auto candidate = groupedSeries.value(groupKey);
                if (!candidate) {
                    candidate = std::make_shared<DicomSeriesCandidate>();
                    candidate->patientName = dicomText(dictionary, "0010|0010")
                                                 .replace(QChar(u'^'), QChar(u' '));
                    candidate->patientId = patientId;
                    candidate->patientSex = dicomText(dictionary, "0010|0040");
                    candidate->patientBirthDate = dicomText(dictionary, "0010|0030");
                    candidate->studyDescription = dicomText(dictionary, "0008|1030");
                    candidate->studyDate = dicomText(dictionary, "0008|0020");
                    candidate->studyUid = studyUid;
                    candidate->seriesDescription = dicomText(dictionary, "0008|103e");
                    candidate->seriesUid = seriesUid;
                    candidate->modality = modality;
                    candidate->windowWidth = dicomNumber(dictionary, "0028|1051", 0.0);
                    candidate->windowLevel = dicomNumber(dictionary, "0028|1050", 0.0);
                    candidate->columns = static_cast<int>(probe->GetDimensions(0));
                    candidate->rows = static_cast<int>(probe->GetDimensions(1));
                    candidate->frames = probe->GetNumberOfDimensions() >= 3
                        ? static_cast<int>(probe->GetDimensions(2)) : 1;
                    candidate->projection = projection;
                    candidate->unsignedPixels = dicomInteger(dictionary, "0028|0103", 0) == 0;
                    groupedSeries.insert(groupKey, candidate);
                }
                candidate->instances.push_back({filePath,
                    dicomInteger(dictionary, "0020|0013", 0),
                    dicomNumber(dictionary, "0020|0032",
                                std::numeric_limits<double>::quiet_NaN())});
            } catch (const itk::ExceptionObject &) {
                // Vendor folders often contain non-image DICOM metadata files.
            } catch (const std::exception &) {
            }
        }

        if (groupedSeries.isEmpty())
            throw std::runtime_error("No readable DICOM image instances found recursively");

        for (auto iterator = groupedSeries.cbegin(); iterator != groupedSeries.cend(); ++iterator) {
            auto candidate = iterator.value();
            std::sort(candidate->instances.begin(), candidate->instances.end(),
                      [](const DicomSeriesCandidate::Instance &left,
                         const DicomSeriesCandidate::Instance &right) {
                const bool leftHasPosition = std::isfinite(left.imagePosition);
                const bool rightHasPosition = std::isfinite(right.imagePosition);
                if (leftHasPosition && rightHasPosition
                    && !qFuzzyCompare(left.imagePosition + 1.0, right.imagePosition + 1.0))
                    return left.imagePosition < right.imagePosition;
                if (left.instanceNumber != right.instanceNumber)
                    return left.instanceNumber < right.instanceNumber;
                return left.path < right.path;
            });
            candidate->volume = !candidate->projection
                && (candidate->instances.size() > 1 || candidate->frames > 1);
            result.candidates.push_back(std::move(candidate));
        }
        std::sort(result.candidates.begin(), result.candidates.end(),
                  [](const auto &left, const auto &right) {
            if (left->patientId != right->patientId)
                return left->patientId < right->patientId;
            if (left->modality != right->modality)
                return left->modality < right->modality;
            if (left->seriesDescription != right->seriesDescription)
                return left->seriesDescription < right->seriesDescription;
            return left->instances.size() > right->instances.size();
        });
    } catch (const itk::ExceptionObject &error) {
        result.error = QStringLiteral("DICOM 读取失败：%1")
                           .arg(QString::fromUtf8(error.GetDescription()));
    } catch (const std::exception &error) {
        result.error = QStringLiteral("DICOM 读取失败：%1")
                           .arg(QString::fromUtf8(error.what()));
    }
    return result;
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

std::shared_ptr<VolumeSnapshot> snapshotFromUnsigned2D(const UnsignedImage2D *image)
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
    snapshot->pixels.resize(count);
    const auto *source = image->GetBufferPointer();
    std::transform(source, source + count, snapshot->pixels.begin(),
                   [](unsigned short value) {
        return static_cast<short>(static_cast<int>(value) - 32768);
    });
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
    const QString path = QDir::cleanPath(localPath(source));
    setBusy(true);
    m_errorMessage.clear();
    m_statusMessage = QStringLiteral("正在递归扫描 DICOM 文件和序列…");
    emit statusChanged();

    const auto result = scanDicomDirectory(path);
    if (!result.error.isEmpty()) {
        setBusy(false);
        setError(result.error);
        return false;
    }
    publishSeriesCandidates(result.candidates);
    if (m_seriesCandidates.size() == 1)
        return loadSeriesCandidate(0);
    m_statusMessage = QStringLiteral("递归扫描完成：发现 %1 个可加载序列或投影，请选择影像。")
                          .arg(m_seriesCandidates.size());
    emit statusChanged();
    setBusy(false);
    return true;
}

void MedicalDataController::importDicomAsync(const QUrl &source)
{
    if (m_busy)
        return;
    const QString path = QDir::cleanPath(localPath(source));
    setBusy(true);
    m_statusMessage = QStringLiteral("正在准备扫描医学数据目录…");
    m_errorMessage.clear();
    emit statusChanged();

    auto *watcher = new QFutureWatcher<DicomScanResult>(this);
    connect(watcher, &QFutureWatcher<DicomScanResult>::finished, this,
            [this, watcher] {
        const auto result = watcher->result();
        watcher->deleteLater();
        if (!result.error.isEmpty()) {
            setBusy(false);
            setError(result.error);
            return;
        }
        publishSeriesCandidates(result.candidates);
        if (m_seriesCandidates.size() == 1) {
            loadSeriesCandidate(0);
            return;
        }
        m_statusMessage = QStringLiteral("递归扫描完成：发现 %1 个可加载序列或投影，请选择影像。")
                              .arg(m_seriesCandidates.size());
        emit statusChanged();
        setBusy(false);
    });
    watcher->setFuture(QtConcurrent::run([path] { return scanDicomDirectory(path); }));
}

void MedicalDataController::publishSeriesCandidates(
    std::vector<std::shared_ptr<DicomSeriesCandidate>> candidates)
{
    m_seriesCandidates = std::move(candidates);
    m_seriesChoices.clear();
    for (qsizetype index = 0;
         index < static_cast<qsizetype>(m_seriesCandidates.size()); ++index) {
        const auto &candidate = m_seriesCandidates[static_cast<std::size_t>(index)];
        const qsizetype instanceCount = static_cast<qsizetype>(candidate->instances.size());
        QString description = candidate->seriesDescription;
        if (description.isEmpty())
            description = candidate->projection ? QStringLiteral("X 线投影")
                                                 : QStringLiteral("未命名序列");
        if (candidate->projection && candidate->instances.front().instanceNumber > 0)
            description += QStringLiteral(" · 图像 %1")
                               .arg(candidate->instances.front().instanceNumber);

        QVariantMap choice;
        choice.insert(QStringLiteral("index"), index);
        choice.insert(QStringLiteral("patientName"), candidate->patientName);
        choice.insert(QStringLiteral("patientId"), candidate->patientId);
        choice.insert(QStringLiteral("modality"), candidate->modality);
        choice.insert(QStringLiteral("description"), description);
        choice.insert(QStringLiteral("instanceCount"), instanceCount);
        choice.insert(QStringLiteral("dimensions"), candidate->volume
            ? QStringLiteral("%1 × %2 × %3").arg(candidate->columns)
                  .arg(candidate->rows).arg(instanceCount)
            : QStringLiteral("%1 × %2").arg(candidate->columns).arg(candidate->rows));
        choice.insert(QStringLiteral("sourceDirectory"),
                      QFileInfo(candidate->instances.front().path).absolutePath());
        m_seriesChoices.append(choice);
    }
    m_selectedSeriesIndex = -1;
    emit seriesChoicesChanged();
    emit selectedSeriesIndexChanged();
}

bool MedicalDataController::selectSeries(int index)
{
    return loadSeriesCandidate(index);
}

bool MedicalDataController::loadSeriesCandidate(int index)
{
    if (index < 0 || index >= static_cast<int>(m_seriesCandidates.size())) {
        setError(QStringLiteral("所选 DICOM 序列不存在，请重新扫描目录。"));
        setBusy(false);
        return false;
    }

    setBusy(true);
    m_errorMessage.clear();
    emit statusChanged();
    const auto candidate = m_seriesCandidates[static_cast<std::size_t>(index)];

    try {
        auto imageIO = itk::GDCMImageIO::New();
        const QString firstPath = candidate->instances.front().path;
        imageIO->SetFileName(QDir::toNativeSeparators(firstPath).toStdString());
        imageIO->ReadImageInformation();

        std::shared_ptr<VolumeSnapshot> snapshot;
        QStringList sourceFiles;
        for (const auto &instance : candidate->instances)
            sourceFiles.append(QDir::toNativeSeparators(instance.path));

        if (candidate->volume && candidate->instances.size() > 1) {
            std::vector<std::string> files;
            files.reserve(candidate->instances.size());
            for (const auto &instance : candidate->instances)
                files.push_back(QDir::toNativeSeparators(instance.path).toStdString());
            auto reader = itk::ImageSeriesReader<Image3D>::New();
            reader->SetImageIO(imageIO);
            reader->SetFileNames(files);
            reader->ForceOrthogonalDirectionOff();
            reader->Update();
            snapshot = snapshotFrom3D(reader->GetOutput());
        } else if (imageIO->GetNumberOfDimensions() >= 3
                   && imageIO->GetDimensions(2) > 1) {
            auto reader = itk::ImageFileReader<Image3D>::New();
            reader->SetImageIO(imageIO);
            reader->SetFileName(QDir::toNativeSeparators(firstPath).toStdString());
            reader->Update();
            snapshot = snapshotFrom3D(reader->GetOutput());
        } else if (candidate->projection && candidate->unsignedPixels) {
            auto reader = itk::ImageFileReader<UnsignedImage2D>::New();
            reader->SetImageIO(imageIO);
            reader->SetFileName(QDir::toNativeSeparators(firstPath).toStdString());
            reader->Update();
            snapshot = snapshotFromUnsigned2D(reader->GetOutput());
        } else {
            auto reader = itk::ImageFileReader<Image2D>::New();
            reader->SetImageIO(imageIO);
            reader->SetFileName(QDir::toNativeSeparators(firstPath).toStdString());
            reader->Update();
            snapshot = snapshotFrom2D(reader->GetOutput());
        }

        resetMetadata();
        m_patientName = candidate->patientName.isEmpty() ? QStringLiteral("未提供")
                                                         : candidate->patientName;
        m_patientId = candidate->patientId.isEmpty() ? QStringLiteral("未提供")
                                                     : candidate->patientId;
        m_patientSex = candidate->patientSex.isEmpty() ? QStringLiteral("--")
                                                       : candidate->patientSex;
        m_patientBirthDate = candidate->patientBirthDate.isEmpty() ? QStringLiteral("--")
                                                                   : candidate->patientBirthDate;
        m_modality = candidate->modality;
        m_studyDescription = candidate->studyDescription.isEmpty()
            ? QStringLiteral("未命名检查") : candidate->studyDescription;
        m_studyDate = candidate->studyDate.isEmpty() ? QStringLiteral("--")
                                                     : candidate->studyDate;
        m_seriesDescription = candidate->seriesDescription.isEmpty()
            ? (candidate->projection ? QStringLiteral("X 线投影")
                                     : QStringLiteral("未命名序列"))
            : candidate->seriesDescription;
        m_windowWidth = candidate->windowWidth;
        m_windowLevel = candidate->windowLevel;
        if (candidate->projection && candidate->unsignedPixels)
            m_windowLevel -= 32768.0;

        if (m_windowWidth <= 0.0 && snapshot && !snapshot->pixels.empty()) {
            const auto range = std::minmax_element(snapshot->pixels.begin(), snapshot->pixels.end());
            m_windowWidth = qMax(1.0, static_cast<double>(*range.second - *range.first));
            m_windowLevel = (static_cast<double>(*range.second) + *range.first) * 0.5;
        }

        m_sourcePath = QDir::toNativeSeparators(firstPath);
        installVolume(std::move(snapshot), sourceFiles);
        m_selectedSeriesIndex = index;
        emit selectedSeriesIndexChanged();
        m_statusMessage = candidate->projection
            ? QStringLiteral("DICOM %1 投影已载入并完成像素与标签校验").arg(candidate->modality)
            : QStringLiteral("DICOM %1 序列已载入：%2 个实例")
                  .arg(candidate->modality).arg(candidate->instances.size());
        emit windowingChanged();
        emit statusChanged();
        setBusy(false);
        return true;
    } catch (const itk::ExceptionObject &error) {
        setBusy(false);
        setError(QStringLiteral("DICOM 像素读取失败：%1")
                     .arg(QString::fromUtf8(error.GetDescription())));
    } catch (const std::exception &error) {
        setBusy(false);
        setError(QStringLiteral("DICOM 像素读取失败：%1")
                     .arg(QString::fromUtf8(error.what())));
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
    m_errorMessage.clear();
    m_statusMessage = QStringLiteral("种子点已选择：IJK (%1, %2, %3)，%4 HU")
                          .arg(seedX).arg(seedY).arg(seedZ).arg(m_regionGrowingSeedValue);
    emit regionGrowingSeedChanged();
    emit statusChanged();
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
        setError(QStringLiteral("请先在轴状位、冠状位或矢状位切片中选择种子点。"));
        return false;
    }
    return applyRegionGrowing(m_regionGrowingSeed[0], m_regionGrowingSeed[1],
                              m_regionGrowingSeed[2], lower, upper);
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

    const auto seedOffset = static_cast<std::size_t>(
        (seedZ * snapshot->dimensions[1] + seedY) * snapshot->dimensions[0] + seedX);
    const short seedValue = snapshot->pixels[seedOffset];
    if (seedValue < lower || seedValue > upper) {
        setError(QStringLiteral("种子点为 %1 HU，不在 %2 至 %3 HU 的生长范围内。")
                     .arg(seedValue).arg(lower).arg(upper));
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

        auto mask = maskSnapshotFromItk(filter->GetOutput(), *snapshot);
        const auto selectedCount = static_cast<qsizetype>(std::count_if(
            mask->pixels.cbegin(), mask->pixels.cend(),
            [](unsigned char value) { return value != 0; }));
        if (selectedCount <= 0) {
            setError(QStringLiteral("种子生长没有产生有效区域，请重新选择种子点或调整 HU 范围。"));
            return false;
        }
        {
            std::lock_guard<std::mutex> guard(m_snapshotMutex);
            m_mask = std::move(mask);
        }
        ++m_segmentationRevision;
        m_statusMessage = QStringLiteral("种子生长完成：IJK (%1, %2, %3)，%4 个体素")
                              .arg(seedX).arg(seedY).arg(seedZ).arg(selectedCount);
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
