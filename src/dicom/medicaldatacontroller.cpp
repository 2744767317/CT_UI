#include "medicaldatacontroller.h"

#include "dicompresentation.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QThread>
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
#include <iostream>
#include <limits>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <streambuf>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

// 目录扫描阶段只读取轻量标签并建立候选对象，直到用户选择后才解码完整像素。
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
    QString sopClassUid;
    QString sopClassName;
    QString imageType;
    QString patientOrientation;
    QString projectionViewKey;
    QString projectionViewLabel;
    QString projectionVariant;
    std::vector<Instance> instances;
    double windowWidth = 0.0;
    double windowLevel = 0.0;
    int columns = 0;
    int rows = 0;
    int frames = 1;
    bool projection = false;
    bool unsignedPixels = false;
    bool inverted = false;
    bool volume = false;
};

// 工作区节点保存像素快照、显示参数和元数据。切换节点时不重新读取 DICOM，
// 分割掩膜与窗宽窗位也随节点一起恢复。
struct LoadedVolumeNode
{
    QString id;
    QString name;
    QString patientName;
    QString patientId;
    QString patientSex;
    QString patientBirthDate;
    QString modality;
    QString studyDescription;
    QString studyDate;
    QString projectionViewLabel;
    QString projectionPairViewLabel;
    QString patientOrientation;
    QString projectionPairOrientation;
    QString imageType;
    QString sopClassName;
    QString projectionPairImageType;
    QString projectionPairSopClassName;
    QString sourcePath;
    QStringList sourceFiles;
    QStringList pairSourceFiles;
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

struct SeriesLoadResult
{
    int index = -1;
    std::shared_ptr<DicomSeriesCandidate> candidate;
    std::shared_ptr<DicomSeriesCandidate> pairCandidate;
    std::shared_ptr<VolumeSnapshot> snapshot;
    std::shared_ptr<VolumeSnapshot> pairSnapshot;
    QStringList sourceFiles;
    QStringList pairSourceFiles;
    QString error;
};

struct SegmentationResult
{
    std::shared_ptr<MaskSnapshot> mask;
    qint64 selectedVoxelCount = 0;
    double selectedVolumeMl = 0.0;
    QString method;
    QString error;
};

namespace {

class NullStreamBuffer final : public std::streambuf
{
protected:
    int_type overflow(int_type character) override { return character; }
};

class ScopedGdcmOutputSilencer final
{
public:
    ScopedGdcmOutputSilencer()
        : lock(globalMutex())
        , previous(std::cerr.rdbuf(&buffer()))
    {
    }

    ~ScopedGdcmOutputSilencer() { std::cerr.rdbuf(previous); }

private:
    static std::mutex &globalMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    static NullStreamBuffer &buffer()
    {
        static NullStreamBuffer buffer;
        return buffer;
    }

    std::unique_lock<std::mutex> lock;
    std::streambuf *previous = nullptr;
};

using Image3D = itk::Image<short, 3>;
using Image2D = itk::Image<short, 2>;
using UnsignedImage2D = itk::Image<unsigned short, 2>;
using MaskImage = itk::Image<unsigned char, 3>;

QString localPath(const QUrl &url)
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

QString decodeDicomBytes(const std::string &value, const QString &characterSet)
{
#ifdef Q_OS_WIN
    if (characterSet.contains(QStringLiteral("GB18030"), Qt::CaseInsensitive)) {
        const int sourceLength = static_cast<int>(value.size());
        const int required = MultiByteToWideChar(54936, 0, value.data(), sourceLength,
                                                  nullptr, 0);
        if (required > 0) {
            QString decoded;
            decoded.resize(required);
            MultiByteToWideChar(54936, 0, value.data(), sourceLength,
                                reinterpret_cast<LPWSTR>(decoded.data()), required);
            return decoded.trimmed();
        }
    }
#else
    Q_UNUSED(characterSet)
#endif
    return QString::fromUtf8(value.data(), static_cast<int>(value.size())).trimmed();
}

QString dicomText(const itk::MetaDataDictionary &dictionary, const char *tag)
{
    std::string value;
    if (!itk::ExposeMetaData<std::string>(dictionary, tag, value))
        return {};
    std::string characterSetValue;
    itk::ExposeMetaData<std::string>(dictionary, "0008|0005", characterSetValue);
    const QString characterSet = QString::fromLatin1(characterSetValue.data(),
                                                       static_cast<int>(characterSetValue.size()));
    return decodeDicomBytes(value, characterSet);
}

QString sopClassLabel(const QString &uid)
{
    if (uid == QStringLiteral("1.2.840.10008.5.1.4.1.1.1"))
        return QStringLiteral("Digital X-Ray Image Storage");
    if (uid == QStringLiteral("1.2.840.10008.5.1.4.1.1.1.1"))
        return QStringLiteral("Digital X-Ray Image Storage - For Presentation");
    if (uid == QStringLiteral("1.2.840.10008.5.1.4.1.1.7"))
        return QStringLiteral("Secondary Capture Image Storage");
    if (uid == QStringLiteral("1.2.840.10008.5.1.4.1.1.2"))
        return QStringLiteral("CT Image Storage");
    return uid.isEmpty() ? QStringLiteral("未知 SOP Class") : uid;
}

QString projectionViewKeyFor(const QString &imageType, const QString &orientation)
{
    const QString type = imageType.toUpper();
    if (type.contains(QStringLiteral("BIPLANE A")))
        return QStringLiteral("frontal");
    if (type.contains(QStringLiteral("BIPLANE B")))
        return QStringLiteral("lateral");

    const QStringList values = orientation.toUpper().split(QChar(u'\\'));
    if (!values.isEmpty() && (values.front() == QStringLiteral("L")
                              || values.front() == QStringLiteral("R")))
        return QStringLiteral("frontal");
    if (!values.isEmpty() && (values.front() == QStringLiteral("A")
                              || values.front() == QStringLiteral("P")))
        return QStringLiteral("lateral");
    return QStringLiteral("unknown");
}

QString projectionViewLabelFor(const QString &key)
{
    if (key == QStringLiteral("frontal"))
        return QStringLiteral("正位 / Frontal");
    if (key == QStringLiteral("lateral"))
        return QStringLiteral("侧位 / Lateral");
    return QStringLiteral("投影 / Projection");
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
    // LIDC 等公开数据的合法私有标签会让 ITKIOGDCM DLL 输出大量重复 Warning。
    // 只在后台 DICOM 操作期间屏蔽 stderr，真正的异常仍转换为 errorMessage。
    ScopedGdcmOutputSilencer silenceGdcmOutput;
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

        // CT 按 Series Instance UID 聚合为体数据；DX/CR 等投影按 SOP Instance
        // 分开，防止把正位和侧位错误堆叠成三维 Volume。
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
                    candidate->sopClassUid = dicomText(dictionary, "0008|0016");
                    candidate->sopClassName = sopClassLabel(candidate->sopClassUid);
                    candidate->imageType = dicomText(dictionary, "0008|0008");
                    candidate->patientOrientation = dicomText(dictionary, "0020|0020");
                    candidate->projectionViewKey = projection
                        ? projectionViewKeyFor(candidate->imageType,
                                               candidate->patientOrientation)
                        : QStringLiteral("volume");
                    candidate->projectionViewLabel = projectionViewLabelFor(
                        candidate->projectionViewKey);
                    // Keep projection scalars unchanged; the renderer applies this
                    // display polarity after the stored-value/window-level pipeline.
                    candidate->inverted = DicomPresentation::grayscaleInverted(
                        dicomText(dictionary, "0028|0004"),
                        dicomText(dictionary, "2050|0020"));
                    candidate->projectionVariant = candidate->imageType.toUpper().contains(
                        QStringLiteral("DERIVED")) ? QStringLiteral("derived")
                                                     : QStringLiteral("original");
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

std::shared_ptr<VolumeSnapshot> readCandidateSnapshot(
    const DicomSeriesCandidate &candidate, QStringList *sourceFiles)
{
    auto imageIO = itk::GDCMImageIO::New();
    const QString firstPath = candidate.instances.front().path;
    imageIO->SetFileName(QDir::toNativeSeparators(firstPath).toStdString());
    imageIO->ReadImageInformation();

    if (sourceFiles) {
        sourceFiles->clear();
        for (const auto &instance : candidate.instances)
            sourceFiles->append(QDir::toNativeSeparators(instance.path));
    }

    if (candidate.volume && candidate.instances.size() > 1) {
        std::vector<std::string> files;
        files.reserve(candidate.instances.size());
        for (const auto &instance : candidate.instances)
            files.push_back(QDir::toNativeSeparators(instance.path).toStdString());
        auto reader = itk::ImageSeriesReader<Image3D>::New();
        reader->SetImageIO(imageIO);
        reader->SetFileNames(files);
        reader->ForceOrthogonalDirectionOff();
        reader->Update();
        return snapshotFrom3D(reader->GetOutput());
    }
    if (imageIO->GetNumberOfDimensions() >= 3 && imageIO->GetDimensions(2) > 1) {
        auto reader = itk::ImageFileReader<Image3D>::New();
        reader->SetImageIO(imageIO);
        reader->SetFileName(QDir::toNativeSeparators(firstPath).toStdString());
        reader->Update();
        return snapshotFrom3D(reader->GetOutput());
    }
    if (candidate.projection && candidate.unsignedPixels) {
        auto reader = itk::ImageFileReader<UnsignedImage2D>::New();
        reader->SetImageIO(imageIO);
        reader->SetFileName(QDir::toNativeSeparators(firstPath).toStdString());
        reader->Update();
        return snapshotFromUnsigned2D(reader->GetOutput());
    }

    auto reader = itk::ImageFileReader<Image2D>::New();
    reader->SetImageIO(imageIO);
    reader->SetFileName(QDir::toNativeSeparators(firstPath).toStdString());
    reader->Update();
    return snapshotFrom2D(reader->GetOutput());
}

int projectionPairIndex(const std::vector<std::shared_ptr<DicomSeriesCandidate>> &candidates,
                        int selectedIndex)
{
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(candidates.size()))
        return -1;
    const auto &selected = candidates[static_cast<std::size_t>(selectedIndex)];
    if (!selected->projection || selected->projectionViewKey == QStringLiteral("unknown"))
        return -1;

    int bestIndex = -1;
    int bestScore = -1;
    for (int index = 0; index < static_cast<int>(candidates.size()); ++index) {
        if (index == selectedIndex)
            continue;
        const auto &candidate = candidates[static_cast<std::size_t>(index)];
        if (!candidate->projection || candidate->projectionViewKey == selected->projectionViewKey
            || candidate->projectionViewKey == QStringLiteral("unknown"))
            continue;
        if (candidate->patientId != selected->patientId
            || candidate->studyUid != selected->studyUid
            || candidate->projectionVariant != selected->projectionVariant)
            continue;
        const bool sameSeries = candidate->seriesUid == selected->seriesUid;
        const bool sameDescription = !candidate->seriesDescription.isEmpty()
            && candidate->seriesDescription == selected->seriesDescription;
        const int score = (sameSeries ? 4 : 0) + (sameDescription ? 2 : 0)
            + (candidate->sopClassUid == selected->sopClassUid ? 1 : 0);
        if (score > bestScore) {
            bestScore = score;
            bestIndex = index;
        }
    }
    return bestIndex;
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
    // 分割任务持有不可变 VolumeSnapshot，ITK 可以直接只读其像素缓冲区。
    // 不再为每次算法执行复制整套 CT，降低峰值内存和启动延迟。
    image->GetPixelContainer()->SetImportPointer(
        const_cast<short *>(snapshot.pixels.data()), snapshot.pixels.size(), false);
    return image;
}

std::shared_ptr<MaskSnapshot> maskSnapshotFromItk(const MaskImage *image,
                                                 const VolumeSnapshot &source,
                                                 qint64 *selectedVoxelCount = nullptr)
{
    auto mask = std::make_shared<MaskSnapshot>();
    mask->dimensions = source.dimensions;
    mask->spacing = source.spacing;
    mask->origin = source.origin;
    mask->direction = source.direction;
    const auto *input = image->GetBufferPointer();
    mask->pixels.resize(source.pixels.size());
    qint64 selected = 0;
    for (std::size_t index = 0; index < source.pixels.size(); ++index) {
        const unsigned char value = input[index] != 0 ? 1 : 0;
        mask->pixels[index] = value;
        selected += value;
    }
    if (selectedVoxelCount)
        *selectedVoxelCount = selected;
    return mask;
}

unsigned int segmentationWorkUnits()
{
    // 给 GUI/VTK 渲染线程保留至少一个逻辑核心，并限制小型工作站上的线程争用。
    return static_cast<unsigned int>(std::clamp(QThread::idealThreadCount() - 1, 1, 8));
}

SeriesLoadResult decodeSeries(
    const std::vector<std::shared_ptr<DicomSeriesCandidate>> &candidates, int index)
{
    ScopedGdcmOutputSilencer silenceGdcmOutput;
    SeriesLoadResult result;
    result.index = index;
    if (index < 0 || index >= static_cast<int>(candidates.size())) {
        result.error = QStringLiteral("所选 DICOM 序列不存在，请重新扫描目录。");
        return result;
    }

    result.candidate = candidates[static_cast<std::size_t>(index)];
    try {
        result.snapshot = readCandidateSnapshot(*result.candidate, &result.sourceFiles);
        const int pairIndex = projectionPairIndex(candidates, index);
        if (pairIndex >= 0) {
            result.pairCandidate = candidates[static_cast<std::size_t>(pairIndex)];
            result.pairSnapshot = readCandidateSnapshot(*result.pairCandidate,
                                                        &result.pairSourceFiles);
        }
        if (!result.snapshot || result.snapshot->pixels.empty())
            result.error = QStringLiteral("DICOM 序列未产生可显示的像素数据。");
    } catch (const itk::ExceptionObject &error) {
        result.error = QStringLiteral("DICOM 像素读取失败：%1")
                           .arg(QString::fromUtf8(error.GetDescription()));
    } catch (const std::exception &error) {
        result.error = QStringLiteral("DICOM 像素读取失败：%1")
                           .arg(QString::fromUtf8(error.what()));
    }
    return result;
}

SegmentationResult thresholdSegmentation(
    const std::shared_ptr<const VolumeSnapshot> &snapshot, double lower, double upper)
{
    SegmentationResult result;
    result.method = QStringLiteral("阈值分割");
    try {
        using Filter = itk::BinaryThresholdImageFilter<Image3D, MaskImage>;
        auto filter = Filter::New();
        filter->SetNumberOfWorkUnits(segmentationWorkUnits());
        filter->SetInput(itkImageFromSnapshot(*snapshot));
        filter->SetLowerThreshold(static_cast<short>(std::clamp(lower, -32768.0, 32767.0)));
        filter->SetUpperThreshold(static_cast<short>(std::clamp(upper, -32768.0, 32767.0)));
        filter->SetInsideValue(1);
        filter->SetOutsideValue(0);
        filter->Update();
        result.mask = maskSnapshotFromItk(filter->GetOutput(), *snapshot,
                                         &result.selectedVoxelCount);
    } catch (const itk::ExceptionObject &error) {
        result.error = QStringLiteral("阈值分割失败：%1")
                           .arg(QString::fromUtf8(error.GetDescription()));
    } catch (const std::exception &error) {
        result.error = QStringLiteral("阈值分割失败：%1")
                           .arg(QString::fromUtf8(error.what()));
    }
    if (result.error.isEmpty() && result.selectedVoxelCount <= 0)
        result.error = QStringLiteral("当前 HU 范围没有选中任何体素。");
    if (result.error.isEmpty()) {
        result.selectedVolumeMl = static_cast<double>(result.selectedVoxelCount)
            * snapshot->spacing[0] * snapshot->spacing[1] * snapshot->spacing[2] / 1000.0;
    }
    return result;
}

SegmentationResult regionGrowingSegmentation(
    const std::shared_ptr<const VolumeSnapshot> &snapshot,
    int seedX, int seedY, int seedZ, double lower, double upper, bool fullyConnected)
{
    SegmentationResult result;
    result.method = fullyConnected ? QStringLiteral("种子生长（26 邻域）")
                                   : QStringLiteral("种子生长（6 邻域）");
    try {
        using Filter = itk::ConnectedThresholdImageFilter<Image3D, MaskImage>;
        auto filter = Filter::New();
        filter->SetNumberOfWorkUnits(segmentationWorkUnits());
        filter->SetInput(itkImageFromSnapshot(*snapshot));
        filter->SetLower(static_cast<short>(std::clamp(lower, -32768.0, 32767.0)));
        filter->SetUpper(static_cast<short>(std::clamp(upper, -32768.0, 32767.0)));
        filter->SetReplaceValue(1);
        filter->SetConnectivity(fullyConnected ? Filter::FullConnectivity
                                               : Filter::FaceConnectivity);
        Image3D::IndexType seed;
        seed[0] = seedX;
        seed[1] = seedY;
        seed[2] = seedZ;
        filter->SetSeed(seed);
        filter->Update();
        result.mask = maskSnapshotFromItk(filter->GetOutput(), *snapshot,
                                         &result.selectedVoxelCount);
    } catch (const itk::ExceptionObject &error) {
        result.error = QStringLiteral("种子生长失败：%1")
                           .arg(QString::fromUtf8(error.GetDescription()));
    } catch (const std::exception &error) {
        result.error = QStringLiteral("种子生长失败：%1")
                           .arg(QString::fromUtf8(error.what()));
    }
    if (result.error.isEmpty() && result.selectedVoxelCount <= 0)
        result.error = QStringLiteral("种子生长没有产生有效区域，请调整种子点或 HU 范围。");
    if (result.error.isEmpty()) {
        result.selectedVolumeMl = static_cast<double>(result.selectedVoxelCount)
            * snapshot->spacing[0] * snapshot->spacing[1] * snapshot->spacing[2] / 1000.0;
    }
    return result;
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
    result.reserve(static_cast<qsizetype>(m_volumeNodes.size()));
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
        item.insert(QStringLiteral("projection"), node->volume
            && node->volume->dimensions[2] == 1 && node->modality != QStringLiteral("CT"));
        item.insert(QStringLiteral("pairedProjection"), node->projectionPair != nullptr);
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
    if (index < 0 || index >= static_cast<int>(m_volumeNodes.size())) {
        setError(QStringLiteral("所选 Volume 不存在。"));
        return false;
    }
    if (index == m_selectedVolumeIndex)
        return true;
    updateActiveVolumeNode();
    activateVolumeNode(index);
    return true;
}

bool MedicalDataController::renameVolume(int index, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (index < 0 || index >= static_cast<int>(m_volumeNodes.size()) || trimmed.isEmpty()) {
        setError(QStringLiteral("Volume 名称不能为空。"));
        return false;
    }
    auto &node = m_volumeNodes[static_cast<std::size_t>(index)];
    if (node->name == trimmed)
        return true;
    node->name = trimmed;
    if (index == m_selectedVolumeIndex) {
        m_seriesDescription = trimmed;
        emit dataChanged();
    }
    emit volumeNodesChanged();
    return true;
}

bool MedicalDataController::removeVolume(int index)
{
    if (index < 0 || index >= static_cast<int>(m_volumeNodes.size())) {
        setError(QStringLiteral("所选 Volume 不存在。"));
        return false;
    }
    const bool removingActive = index == m_selectedVolumeIndex;
    m_volumeNodes.erase(m_volumeNodes.begin() + index);
    if (m_volumeNodes.empty()) {
        clearActiveVolume();
    } else if (removingActive) {
        activateVolumeNode(std::min(index, static_cast<int>(m_volumeNodes.size()) - 1));
    } else {
        if (index < m_selectedVolumeIndex)
            --m_selectedVolumeIndex;
        emit volumeNodesChanged();
    }
    return true;
}

bool MedicalDataController::setVolumeVisibility(int index, bool visible)
{
    if (index < 0 || index >= static_cast<int>(m_volumeNodes.size())) {
        setError(QStringLiteral("所选 Volume 不存在。"));
        return false;
    }
    auto &node = m_volumeNodes[static_cast<std::size_t>(index)];
    if (node->visible == visible)
        return true;
    node->visible = visible;
    emit volumeNodesChanged();
    if (index == m_selectedVolumeIndex)
        emit dataChanged();
    return true;
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

    // 递归探测可能遍历数千文件，放到线程池执行；候选结果只在 GUI 线程发布。
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
            // 扫描和像素解码是两个独立后台阶段，避免单序列目录在扫描结束时
            // 又回到 GUI 线程同步解码完整体数据。
            setBusy(false);
            selectSeriesAsync(0);
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
        choice.insert(QStringLiteral("viewLabel"), candidate->projectionViewLabel);
        choice.insert(QStringLiteral("orientation"), candidate->patientOrientation);
        choice.insert(QStringLiteral("sopClass"), candidate->sopClassName);
        choice.insert(QStringLiteral("imageType"), candidate->imageType);
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

void MedicalDataController::selectSeriesAsync(int index)
{
    if (m_busy)
        return;
    if (index < 0 || index >= static_cast<int>(m_seriesCandidates.size())) {
        setError(QStringLiteral("所选 DICOM 序列不存在，请重新扫描目录。"));
        return;
    }

    setBusy(true);
    m_errorMessage.clear();
    m_statusMessage = QStringLiteral("正在后台解码 DICOM 像素…");
    emit statusChanged();

    const auto candidates = m_seriesCandidates;
    auto *watcher = new QFutureWatcher<SeriesLoadResult>(this);
    connect(watcher, &QFutureWatcher<SeriesLoadResult>::finished, this,
            [this, watcher] {
        auto result = watcher->result();
        watcher->deleteLater();
        commitSeriesLoad(std::move(result));
    });
    watcher->setFuture(QtConcurrent::run(
        [candidates, index] { return decodeSeries(candidates, index); }));
}

bool MedicalDataController::loadSeriesCandidate(int index)
{
    setBusy(true);
    m_errorMessage.clear();
    m_statusMessage = QStringLiteral("正在解码 DICOM 像素…");
    emit statusChanged();
    return commitSeriesLoad(decodeSeries(m_seriesCandidates, index));
}

bool MedicalDataController::commitSeriesLoad(SeriesLoadResult result)
{
    if (!result.error.isEmpty()) {
        setBusy(false);
        setError(result.error);
        return false;
    }

    const auto &candidate = result.candidate;
    const auto &pairCandidate = result.pairCandidate;
    const QString firstPath = candidate->instances.front().path;
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
    m_projectionViewLabel = candidate->projection ? candidate->projectionViewLabel : QString();
    m_patientOrientation = candidate->projection ? candidate->patientOrientation : QString();
    m_imageType = candidate->imageType;
    m_sopClassName = candidate->sopClassName;
    m_projectionUnsigned = candidate->projection && candidate->unsignedPixels;
    m_projectionInverted = candidate->projection && candidate->inverted;
    m_projectionPairInverted = pairCandidate && pairCandidate->inverted;
    m_projectionPairViewLabel = pairCandidate ? pairCandidate->projectionViewLabel : QString();
    m_projectionPairOrientation = pairCandidate ? pairCandidate->patientOrientation : QString();
    m_projectionPairImageType = pairCandidate ? pairCandidate->imageType : QString();
    m_projectionPairSopClassName = pairCandidate ? pairCandidate->sopClassName : QString();
    m_windowWidth = candidate->windowWidth;
    m_windowLevel = candidate->windowLevel;
    if (candidate->projection && candidate->unsignedPixels)
        m_windowLevel -= 32768.0;

    if (m_windowWidth <= 0.0 && !result.snapshot->pixels.empty()) {
        const auto range = std::minmax_element(result.snapshot->pixels.begin(),
                                               result.snapshot->pixels.end());
        m_windowWidth = qMax(1.0, static_cast<double>(*range.second - *range.first));
        m_windowLevel = (static_cast<double>(*range.second) + *range.first) * 0.5;
    }

    m_sourcePath = QDir::toNativeSeparators(firstPath);
    installVolume(std::move(result.snapshot), result.sourceFiles,
                  std::move(result.pairSnapshot), result.pairSourceFiles);
    m_selectedSeriesIndex = result.index;
    emit selectedSeriesIndexChanged();
    m_statusMessage = candidate->projection
        ? QStringLiteral("DICOM %1 投影已载入并完成像素与标签校验").arg(candidate->modality)
        : QStringLiteral("DICOM %1 序列已载入：%2 个实例")
              .arg(candidate->modality).arg(candidate->instances.size());
    m_errorMessage.clear();
    emit windowingChanged();
    emit statusChanged();
    setBusy(false);
    return true;
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
    if (!snapshot || snapshot->dimensions[2] <= 1 || lower > upper) {
        setError(QStringLiteral("阈值分割参数无效或尚未载入影像。"));
        return false;
    }
    const int revision = m_datasetRevision;
    setBusy(true);
    m_statusMessage = QStringLiteral("正在执行阈值分割…");
    m_errorMessage.clear();
    emit statusChanged();
    return commitSegmentation(
        thresholdSegmentation(snapshot, lower, upper), revision,
        QStringLiteral("阈值分割完成：%1 至 %2 HU").arg(lower).arg(upper));
}

void MedicalDataController::applyThresholdAsync(double lower, double upper)
{
    if (m_busy)
        return;
    const auto snapshot = volumeSnapshot();
    if (!snapshot || snapshot->dimensions[2] <= 1 || lower > upper) {
        setError(QStringLiteral("阈值分割参数无效或尚未载入 CT 体数据。"));
        return;
    }

    const int revision = m_datasetRevision;
    const QString success = QStringLiteral("阈值分割完成：%1 至 %2 HU")
                                .arg(lower).arg(upper);
    setBusy(true);
    m_statusMessage = QStringLiteral("正在后台执行阈值分割…");
    m_errorMessage.clear();
    emit statusChanged();

    auto *watcher = new QFutureWatcher<SegmentationResult>(this);
    connect(watcher, &QFutureWatcher<SegmentationResult>::finished, this,
            [this, watcher, revision, success] {
        auto result = watcher->result();
        watcher->deleteLater();
        commitSegmentation(std::move(result), revision, success);
    });
    watcher->setFuture(QtConcurrent::run(
        [snapshot, lower, upper] { return thresholdSegmentation(snapshot, lower, upper); }));
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
    m_errorMessage.clear();
    m_statusMessage = QStringLiteral("种子点已清除");
    emit regionGrowingSeedChanged();
    emit statusChanged();
}

bool MedicalDataController::applyRegionGrowingFromSeed(double lower, double upper)
{
    if (!m_regionGrowingSeedValid) {
        setError(QStringLiteral("请先在轴状位、冠状位或矢状位切片中选择种子点。"));
        return false;
    }
    if (!std::isfinite(lower) || !std::isfinite(upper) || lower > upper) {
        setError(QStringLiteral("种子生长的 HU 范围无效。"));
        return false;
    }
    return applyRegionGrowing(m_regionGrowingSeed[0], m_regionGrowingSeed[1],
                              m_regionGrowingSeed[2], lower, upper);
}

void MedicalDataController::applyRegionGrowingFromSeedAsync(
    double lower, double upper, bool fullyConnected)
{
    if (m_busy)
        return;
    if (!m_regionGrowingSeedValid) {
        setError(QStringLiteral("请先在轴状位、冠状位或矢状位切片中选择种子点。"));
        return;
    }

    const auto snapshot = volumeSnapshot();
    const int seedX = m_regionGrowingSeed[0];
    const int seedY = m_regionGrowingSeed[1];
    const int seedZ = m_regionGrowingSeed[2];
    if (!snapshot || !std::isfinite(lower) || !std::isfinite(upper) || lower > upper) {
        setError(QStringLiteral("种子生长的 HU 范围无效。"));
        return;
    }
    const auto seedOffset = static_cast<std::size_t>(
        (seedZ * snapshot->dimensions[1] + seedY) * snapshot->dimensions[0] + seedX);
    const short seedValue = snapshot->pixels[seedOffset];
    if (seedValue < lower || seedValue > upper) {
        setError(QStringLiteral("种子点为 %1 HU，不在 %2 至 %3 HU 的生长范围内。")
                     .arg(seedValue).arg(lower).arg(upper));
        return;
    }

    const int revision = m_datasetRevision;
    const QString success = QStringLiteral("种子生长完成：IJK (%1, %2, %3)")
                                .arg(seedX).arg(seedY).arg(seedZ);
    setBusy(true);
    m_statusMessage = QStringLiteral("正在后台执行种子生长…");
    m_errorMessage.clear();
    emit statusChanged();

    auto *watcher = new QFutureWatcher<SegmentationResult>(this);
    connect(watcher, &QFutureWatcher<SegmentationResult>::finished, this,
            [this, watcher, revision, success] {
        auto result = watcher->result();
        watcher->deleteLater();
        commitSegmentation(std::move(result), revision, success);
    });
    watcher->setFuture(QtConcurrent::run(
        [snapshot, seedX, seedY, seedZ, lower, upper, fullyConnected] {
            return regionGrowingSegmentation(snapshot, seedX, seedY, seedZ,
                                             lower, upper, fullyConnected);
        }));
}

bool MedicalDataController::applyRegionGrowing(int seedX, int seedY, int seedZ,
                                               double lower, double upper,
                                               bool fullyConnected)
{
    const auto snapshot = volumeSnapshot();
    if (!snapshot || !std::isfinite(lower) || !std::isfinite(upper) || lower > upper
        || seedX < 0 || seedY < 0 || seedZ < 0
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

    const int revision = m_datasetRevision;
    setBusy(true);
    m_statusMessage = QStringLiteral("正在执行种子生长…");
    m_errorMessage.clear();
    emit statusChanged();
    return commitSegmentation(
        regionGrowingSegmentation(snapshot, seedX, seedY, seedZ,
                                  lower, upper, fullyConnected),
        revision, QStringLiteral("种子生长完成：IJK (%1, %2, %3)")
                      .arg(seedX).arg(seedY).arg(seedZ));
}

bool MedicalDataController::commitSegmentation(
    SegmentationResult result, int expectedDatasetRevision, const QString &successMessage)
{
    if (expectedDatasetRevision != m_datasetRevision) {
        setBusy(false);
        setError(QStringLiteral("数据集已切换，本次分割结果已丢弃。"));
        return false;
    }
    if (!result.error.isEmpty()) {
        setBusy(false);
        setError(result.error);
        return false;
    }

    {
        std::lock_guard<std::mutex> guard(m_snapshotMutex);
        m_mask = std::move(result.mask);
    }
    m_segmentationMethod = std::move(result.method);
    m_segmentationVoxelCount = result.selectedVoxelCount;
    m_segmentationVolumeMl = result.selectedVolumeMl;
    updateActiveVolumeNode();
    ++m_segmentationRevision;
    m_statusMessage = QStringLiteral("%1，%2 个体素，约 %3 mL")
                          .arg(successMessage)
                          .arg(m_segmentationVoxelCount)
                          .arg(m_segmentationVolumeMl, 0, 'f', 2);
    m_errorMessage.clear();
    emit segmentationChanged();
    emit statusChanged();
    setBusy(false);
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
    m_statusMessage = QStringLiteral("分割结果已清除");
    emit segmentationChanged();
    emit statusChanged();
}

double MedicalDataController::estimateDistanceMm(int viewType, double pixelDx, double pixelDy,
                                                 double viewportWidth, double viewportHeight,
                                                 bool pairedProjection) const
{
    const auto snapshot = pairedProjection ? projectionPairSnapshot() : volumeSnapshot();
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
    updateActiveVolumeNode();
    emit windowingChanged();
}

void MedicalDataController::setWindowLevel(double value)
{
    if (qFuzzyCompare(m_windowLevel, value))
        return;
    m_windowLevel = value;
    updateActiveVolumeNode();
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
                                          const QStringList &sourceFiles,
                                          std::shared_ptr<VolumeSnapshot> pairSnapshot,
                                          const QStringList &pairSourceFiles)
{
    // 相同源文件再次载入时更新原节点，保留用户重命名和显隐状态；不同源文件
    // 则追加为新 Volume，实现同一工作区内的多数据集切换。
    auto node = std::make_shared<LoadedVolumeNode>();
    node->id = !m_sourcePath.isEmpty()
        ? m_sourcePath
        : QStringLiteral("volume-%1").arg(m_datasetRevision + 1);
    node->name = m_seriesDescription;
    node->patientName = m_patientName;
    node->patientId = m_patientId;
    node->patientSex = m_patientSex;
    node->patientBirthDate = m_patientBirthDate;
    node->modality = m_modality;
    node->studyDescription = m_studyDescription;
    node->studyDate = m_studyDate;
    node->projectionViewLabel = m_projectionViewLabel;
    node->projectionPairViewLabel = m_projectionPairViewLabel;
    node->patientOrientation = m_patientOrientation;
    node->projectionPairOrientation = m_projectionPairOrientation;
    node->imageType = m_imageType;
    node->sopClassName = m_sopClassName;
    node->projectionPairImageType = m_projectionPairImageType;
    node->projectionPairSopClassName = m_projectionPairSopClassName;
    node->projectionUnsigned = m_projectionUnsigned;
    node->projectionInverted = m_projectionInverted;
    node->projectionPairInverted = m_projectionPairInverted;
    node->sourcePath = m_sourcePath;
    node->sourceFiles = sourceFiles;
    node->pairSourceFiles = pairSourceFiles;
    node->volume = std::move(snapshot);
    node->projectionPair = std::move(pairSnapshot);
    node->windowWidth = m_windowWidth;
    node->windowLevel = m_windowLevel;

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
    {
        std::lock_guard<std::mutex> guard(m_snapshotMutex);
        node->volume = m_volume;
        node->projectionPair = m_projectionPair;
        node->mask = m_mask;
    }
    node->windowWidth = m_windowWidth;
    node->windowLevel = m_windowLevel;
    node->projectionInverted = m_projectionInverted;
    node->projectionPairInverted = m_projectionPairInverted;
    node->segmentationMethod = m_segmentationMethod;
    node->segmentationVoxelCount = m_segmentationVoxelCount;
    node->segmentationVolumeMl = m_segmentationVolumeMl;
}

void MedicalDataController::activateVolumeNode(int index)
{
    if (index < 0 || index >= static_cast<int>(m_volumeNodes.size()))
        return;
    const auto &node = m_volumeNodes[static_cast<std::size_t>(index)];
    // shared_ptr 交换是轻量操作；dataChanged 后各视口异步重建自己的 VTK 管线。
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
    m_patientSex = node->patientSex;
    m_patientBirthDate = node->patientBirthDate;
    m_modality = node->modality;
    m_studyDescription = node->studyDescription;
    m_studyDate = node->studyDate;
    m_seriesDescription = node->name;
    m_projectionViewLabel = node->projectionViewLabel;
    m_projectionPairViewLabel = node->projectionPairViewLabel;
    m_patientOrientation = node->patientOrientation;
    m_projectionPairOrientation = node->projectionPairOrientation;
    m_imageType = node->imageType;
    m_sopClassName = node->sopClassName;
    m_projectionPairImageType = node->projectionPairImageType;
    m_projectionPairSopClassName = node->projectionPairSopClassName;
    m_projectionUnsigned = node->projectionUnsigned;
    m_projectionInverted = node->projectionInverted;
    m_projectionPairInverted = node->projectionPairInverted;
    m_sourcePath = node->sourcePath;
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
    m_regionGrowingSeed = {-1, -1, -1};
    m_regionGrowingSeedValue = 0;
    m_regionGrowingSeedValid = false;
    m_segmentationMethod.clear();
    m_segmentationVoxelCount = 0;
    m_segmentationVolumeMl = 0.0;
    emit dataChanged();
    emit segmentationChanged();
    emit regionGrowingSeedChanged();
    emit windowingChanged();
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
    m_segmentationMethod.clear();
    m_segmentationVoxelCount = 0;
    m_segmentationVolumeMl = 0.0;
}
