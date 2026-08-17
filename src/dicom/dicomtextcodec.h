#pragma once

#include <QString>

#include <string>

namespace DicomTextCodec {

// Decode a DICOM text value using (0008,0005) Specific Character Set.
QString decode(const std::string &value, const QString &specificCharacterSet);

// Reject text that has already lost characters before import (for example a
// vendor-written label containing a high proportion of literal '?' bytes).
bool isUsableDisplayText(const QString &text);

} // namespace DicomTextCodec
