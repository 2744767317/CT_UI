#pragma once

#include <QObject>
#include <QUrl>

// Windows 系统另存为对话框。由 QML 同步调用，取消时返回空 URL。
class NativeSaveFileDialog final : public QObject
{
    Q_OBJECT

public:
    explicit NativeSaveFileDialog(QObject *parent = nullptr);

    Q_INVOKABLE QUrl getSaveFileUrl(const QString &title,
                                    const QString &defaultBaseName,
                                    const QString &defaultSuffix);
};
