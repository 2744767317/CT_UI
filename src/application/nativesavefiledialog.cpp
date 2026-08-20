#include "src/application/nativesavefiledialog.h"

#include <QGuiApplication>
#include <QString>
#include <QUrl>
#include <QWindow>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <string>
#endif

NativeSaveFileDialog::NativeSaveFileDialog(QObject *parent)
    : QObject(parent)
{
}

QUrl NativeSaveFileDialog::getSaveFileUrl(const QString &title,
                                          const QString &defaultBaseName,
                                          const QString &defaultSuffix)
{
#ifdef Q_OS_WIN
    IFileSaveDialog *dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dialog));
    if (FAILED(hr) || dialog == nullptr)
        return {};

    const std::wstring dialogTitle = (title.isEmpty() ? QStringLiteral("保存") : title).toStdWString();
    dialog->SetTitle(dialogTitle.c_str());

    COMDLG_FILTERSPEC filters[] = {
        {L"PNG 图像", L"*.png"},
        {L"DICOM 文件", L"*.dcm"},
    };
    dialog->SetFileTypes(ARRAYSIZE(filters), filters);
    dialog->SetFileTypeIndex(1);

    const std::wstring suffix =
            (defaultSuffix.isEmpty() ? QStringLiteral("png") : defaultSuffix).toStdWString();
    dialog->SetDefaultExtension(suffix.c_str());

    if (!defaultBaseName.isEmpty()) {
        const std::wstring fileName = defaultBaseName.toStdWString();
        dialog->SetFileName(fileName.c_str());
    }

    DWORD options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options))) {
        dialog->SetOptions(options | FOS_OVERWRITEPROMPT | FOS_NOCHANGEDIR | FOS_FORCEFILESYSTEM);
    }

    IShellItem *desktopItem = nullptr;
    if (SUCCEEDED(SHCreateItemInKnownFolder(FOLDERID_Desktop, 0, nullptr,
                                            IID_PPV_ARGS(&desktopItem)))
            && desktopItem != nullptr) {
        dialog->SetDefaultFolder(desktopItem);
        desktopItem->Release();
    }

    HWND hwnd = nullptr;
    if (QWindow *window = QGuiApplication::focusWindow())
        hwnd = reinterpret_cast<HWND>(window->winId());

    QUrl result;
    hr = dialog->Show(hwnd);
    if (SUCCEEDED(hr)) {
        IShellItem *item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr) {
                result = QUrl::fromLocalFile(QString::fromWCharArray(path));
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dialog->Release();
    return result;
#else
    Q_UNUSED(title);
    Q_UNUSED(defaultBaseName);
    Q_UNUSED(defaultSuffix);
    return {};
#endif
}
