#pragma once

#include <QByteArray>
#include <QString>

// 将 RGB 画面写成 PNG 或 Secondary Capture DICOM。不依赖 VTK，供控制器和核心测试共用。
struct SecondaryCaptureMeta
{
    QString patientName;
    QString patientId;
    QString patientSex;
    QString patientBirthDate;
    QString studyDescription;
    QString studyDate;
    QString studyInstanceUid;
    QString seriesDescription;
};

QString generateDicomUid();

bool writeRgbPng(const QString &destinationPath,
                 const QByteArray &rgbPackedTopLeft,
                 int width,
                 int height,
                 QString *errorMessage);

bool writeSecondaryCaptureDicom(const QString &destinationPath,
                                const QByteArray &rgbPackedTopLeft,
                                int width,
                                int height,
                                const SecondaryCaptureMeta &meta,
                                QString *errorMessage);
