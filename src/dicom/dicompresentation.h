#pragma once

#include <QString>

namespace DicomPresentation {

inline bool grayscaleInverted(const QString &photometricInterpretation,
                              const QString &presentationLutShape)
{
    const bool monochrome1 = photometricInterpretation.trimmed().compare(
                                 QStringLiteral("MONOCHROME1"),
                                 Qt::CaseInsensitive) == 0;
    const bool inversePresentation = presentationLutShape.trimmed().compare(
                                         QStringLiteral("INVERSE"),
                                         Qt::CaseInsensitive) == 0;
    // The two tags describe the requested display polarity. If both are present,
    // they reinforce the same polarity instead of requesting two inversions.
    return monochrome1 || inversePresentation;
}

} // namespace DicomPresentation
