#include "secondarycaptureexport.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QRandomGenerator>

#include <cstring>

#include <itkGDCMImageIO.h>
#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkMetaDataObject.h>
#include <itkRGBPixel.h>

#include <exception>
#include <string>

namespace {

QString dicomDate(const QString &value)
{
    QString digits;
    digits.reserve(8);
    for (const QChar character : value) {
        if (character.isDigit())
            digits.append(character);
    }
    return digits.size() == 8 ? digits : QString();
}

QString dicomPersonName(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty() || trimmed == QStringLiteral("未提供")
        || trimmed == QStringLiteral("--"))
        return {};
    return trimmed;
}

QString dicomSex(const QString &value)
{
    const QString trimmed = value.trimmed().toUpper();
    if (trimmed == QStringLiteral("M") || trimmed == QStringLiteral("F")
        || trimmed == QStringLiteral("O"))
        return trimmed;
    return {};
}

void putTag(itk::MetaDataDictionary &dictionary, const char *tag, const QString &value)
{
    if (value.isEmpty())
        return;
    itk::EncapsulateMetaData<std::string>(dictionary, tag,
                                          std::string(value.toUtf8().constData()));
}

} // namespace

QString generateDicomUid()
{
    // Each arc stays below 2^31-1. Some DICOM readers parse UID components as
    // 32/64-bit integers and reject values from generate64() such as
    // 17111187354348879290 (> 2^63-1).
    auto *rng = QRandomGenerator::global();
    const auto arc = [rng]() { return rng->bounded(1, 2147483647); };
    return QStringLiteral("2.25.%1.%2.%3.%4").arg(arc()).arg(arc()).arg(arc()).arg(arc());
}

bool writeRgbPng(const QString &destinationPath,
                 const QByteArray &rgbPackedTopLeft,
                 int width,
                 int height,
                 QString *errorMessage)
{
    const auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };

    if (destinationPath.isEmpty())
        return fail(QStringLiteral("导出路径无效。"));
    if (width <= 0 || height <= 0
        || rgbPackedTopLeft.size() != static_cast<qsizetype>(width) * height * 3)
        return fail(QStringLiteral("3D 渲染捕获失败：图像数据无效。"));

    QImage image(width, height, QImage::Format_RGB888);
    if (image.isNull())
        return fail(QStringLiteral("无法创建 PNG 图像。"));

    const auto *source = reinterpret_cast<const uchar *>(rgbPackedTopLeft.constData());
    const qsizetype rowBytes = static_cast<qsizetype>(width) * 3;
    for (int y = 0; y < height; ++y)
        memcpy(image.scanLine(y), source + y * rowBytes, static_cast<size_t>(rowBytes));

    if (!image.save(destinationPath, "PNG"))
        return fail(QStringLiteral("写出 PNG 失败。"));

    if (errorMessage)
        errorMessage->clear();
    return true;
}

bool writeSecondaryCaptureDicom(const QString &destinationPath,
                                const QByteArray &rgbPackedTopLeft,
                                int width,
                                int height,
                                const SecondaryCaptureMeta &meta,
                                QString *errorMessage)
{
    const auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };

    if (destinationPath.isEmpty())
        return fail(QStringLiteral("导出路径无效。"));
    if (width <= 0 || height <= 0
        || rgbPackedTopLeft.size() != static_cast<qsizetype>(width) * height * 3)
        return fail(QStringLiteral("3D 渲染捕获失败：图像数据无效。"));

    const QFileInfo fileInfo(destinationPath);
    if (fileInfo.absolutePath().isEmpty())
        return fail(QStringLiteral("导出路径无效。"));

    using RgbPixel = itk::RGBPixel<unsigned char>;
    using RgbImage = itk::Image<RgbPixel, 2>;
    auto image = RgbImage::New();
    RgbImage::IndexType start {};
    start.Fill(0);
    RgbImage::SizeType size;
    size[0] = static_cast<RgbImage::SizeValueType>(width);
    size[1] = static_cast<RgbImage::SizeValueType>(height);
    image->SetRegions(RgbImage::RegionType(start, size));
    RgbImage::SpacingType spacing;
    spacing.Fill(1.0);
    image->SetSpacing(spacing);
    image->Allocate();

    RgbPixel *pixels = image->GetBufferPointer();
    const auto *source = reinterpret_cast<const unsigned char *>(rgbPackedTopLeft.constData());
    const int pixelCount = width * height;
    for (int index = 0; index < pixelCount; ++index) {
        pixels[index].Set(source[index * 3], source[index * 3 + 1], source[index * 3 + 2]);
    }

    const QDateTime now = QDateTime::currentDateTime();
    const QString studyUid = meta.studyInstanceUid.isEmpty() ? generateDicomUid()
                                                             : meta.studyInstanceUid;
    const QString seriesUid = generateDicomUid();
    const QString sopInstanceUid = generateDicomUid();
    const QString seriesDescription = meta.seriesDescription.isEmpty()
        ? QStringLiteral("3D Volume Rendering")
        : meta.seriesDescription;

    auto &dictionary = image->GetMetaDataDictionary();
    putTag(dictionary, "0008|0005", QStringLiteral("ISO_IR 192"));
    putTag(dictionary, "0008|0008", QStringLiteral("DERIVED\\SECONDARY"));
    putTag(dictionary, "0008|0016", QStringLiteral("1.2.840.10008.5.1.4.1.1.7"));
    putTag(dictionary, "0008|0018", sopInstanceUid);
    putTag(dictionary, "0008|0020", dicomDate(meta.studyDate));
    putTag(dictionary, "0008|0023", now.toString(QStringLiteral("yyyyMMdd")));
    putTag(dictionary, "0008|0030", now.toString(QStringLiteral("HHmmss")));
    putTag(dictionary, "0008|0033", now.toString(QStringLiteral("HHmmss")));
    putTag(dictionary, "0008|0060", QStringLiteral("SC"));
    putTag(dictionary, "0008|0064", QStringLiteral("WSD"));
    putTag(dictionary, "0008|0070", QStringLiteral("GuangSuo"));
    putTag(dictionary, "0008|1030", dicomPersonName(meta.studyDescription));
    putTag(dictionary, "0008|103e", seriesDescription);
    putTag(dictionary, "0008|1090", QStringLiteral("CT_UI"));
    putTag(dictionary, "0010|0010", dicomPersonName(meta.patientName));
    putTag(dictionary, "0010|0020", dicomPersonName(meta.patientId));
    putTag(dictionary, "0010|0030", dicomDate(meta.patientBirthDate));
    putTag(dictionary, "0010|0040", dicomSex(meta.patientSex));
    putTag(dictionary, "0018|1012", now.toString(QStringLiteral("yyyyMMdd")));
    putTag(dictionary, "0018|1014", now.toString(QStringLiteral("HHmmss")));
    putTag(dictionary, "0018|1016", QStringLiteral("GuangSuo"));
    putTag(dictionary, "0018|1018", QStringLiteral("CT_UI"));
    putTag(dictionary, "0018|1019", QStringLiteral("CT_UI"));
    putTag(dictionary, "0020|000d", studyUid);
    putTag(dictionary, "0020|000e", seriesUid);
    putTag(dictionary, "0020|0011", QStringLiteral("9001"));
    putTag(dictionary, "0020|0013", QStringLiteral("1"));
    putTag(dictionary, "0028|0002", QStringLiteral("3"));
    putTag(dictionary, "0028|0004", QStringLiteral("RGB"));
    putTag(dictionary, "0028|0006", QStringLiteral("0"));

    try {
        auto imageIo = itk::GDCMImageIO::New();
        imageIo->SetKeepOriginalUID(true);
        auto writer = itk::ImageFileWriter<RgbImage>::New();
        writer->SetImageIO(imageIo);
        // ITK/GDCM 在 Windows 上按本地窄字符打开路径，中文目录会失败。
        // 先写到临时 ASCII 路径，再用 Qt 宽字符接口搬到目标位置。
        const QString finalPath = fileInfo.absoluteFilePath();
        const QString tempPath =
                QDir::temp().filePath(QStringLiteral("ct_ui_sc_%1.dcm").arg(sopInstanceUid));
        QFile::remove(tempPath);
        writer->SetFileName(QFile::encodeName(tempPath).constData());
        writer->SetInput(image);
        writer->Update();
        QFile::remove(finalPath);
        if (!QFile::rename(tempPath, finalPath) && !QFile::copy(tempPath, finalPath)) {
            QFile::remove(tempPath);
            return fail(QStringLiteral("无法将 Secondary Capture 移动到导出目录。"));
        }
        QFile::remove(tempPath);
    } catch (const std::exception &exception) {
        return fail(QStringLiteral("写出 Secondary Capture 失败：%1")
                        .arg(QString::fromLocal8Bit(exception.what())));
    }

    if (errorMessage)
        errorMessage->clear();
    return true;
}
