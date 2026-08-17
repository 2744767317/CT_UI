#include "dicomtextcodec.h"

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace {

QString trimDicomPadding(QString text)
{
    while (!text.isEmpty() && (text.back() == QChar(u'\0') || text.back() == QChar(u' ')))
        text.chop(1);
    return text.trimmed();
}

#ifdef Q_OS_WIN
QString decodeWindowsCodePage(const std::string &value, unsigned int codePage)
{
    if (value.empty())
        return {};

    const int sourceLength = static_cast<int>(value.size());
    const int required = MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, value.data(),
                                              sourceLength, nullptr, 0);
    if (required <= 0)
        return {};

    QString decoded;
    decoded.resize(required);
    const int written = MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, value.data(),
                                             sourceLength,
                                             reinterpret_cast<LPWSTR>(decoded.data()), required);
    return written == required ? trimDicomPadding(decoded) : QString();
}
#endif

} // namespace

namespace DicomTextCodec {

QString decode(const std::string &value, const QString &specificCharacterSet)
{
    const QString characterSet = specificCharacterSet.trimmed().toUpper();

#ifdef Q_OS_WIN
    unsigned int codePage = 0;
    if (characterSet.contains(QStringLiteral("GB18030")))
        codePage = 54936;
    else if (characterSet.contains(QStringLiteral("ISO_IR 58"))
             || characterSet.contains(QStringLiteral("GB2312"))
             || characterSet.contains(QStringLiteral("GBK")))
        codePage = 936;
    else if (characterSet.contains(QStringLiteral("ISO_IR 192"))
             || characterSet.contains(QStringLiteral("UTF-8")))
        codePage = CP_UTF8;
    else if (characterSet.contains(QStringLiteral("ISO_IR 100")))
        codePage = 28591;
    else if (characterSet.contains(QStringLiteral("ISO_IR 101")))
        codePage = 28592;
    else if (characterSet.contains(QStringLiteral("ISO_IR 109")))
        codePage = 28593;
    else if (characterSet.contains(QStringLiteral("ISO_IR 110")))
        codePage = 28594;
    else if (characterSet.contains(QStringLiteral("ISO_IR 144")))
        codePage = 28595;
    else if (characterSet.contains(QStringLiteral("ISO_IR 127")))
        codePage = 28596;
    else if (characterSet.contains(QStringLiteral("ISO_IR 126")))
        codePage = 28597;
    else if (characterSet.contains(QStringLiteral("ISO_IR 138")))
        codePage = 28598;
    else if (characterSet.contains(QStringLiteral("ISO_IR 148")))
        codePage = 28599;

    if (codePage != 0) {
        const QString decoded = decodeWindowsCodePage(value, codePage);
        if (!decoded.isNull())
            return decoded;
    }
#endif

    // DICOM's default repertoire is ASCII. UTF-8 is also a useful fallback for
    // non-conformant files that omitted (0008,0005); invalid input is made
    // visible to isUsableDisplayText() through the replacement character.
    return trimDicomPadding(
        QString::fromUtf8(value.data(), static_cast<int>(value.size())));
}

bool isUsableDisplayText(const QString &text)
{
    const QString value = text.trimmed();
    if (value.isEmpty())
        return false;

    int questionMarks = 0;
    for (const QChar character : value) {
        if (character == QChar::ReplacementCharacter || character == QChar(u'\0'))
            return false;
        if (character.category() == QChar::Other_Control)
            return false;
        if (character == QChar(u'?'))
            ++questionMarks;
    }

    // A question mark can be legitimate. Several question marks occupying at
    // least a quarter of a label indicate characters that the exporting system
    // already replaced with 0x3F, which no decoder can reconstruct.
    return questionMarks < 3 || questionMarks * 4 < value.size();
}

} // namespace DicomTextCodec
