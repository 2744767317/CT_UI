#pragma once

#include <QWidget>

#include <functional>

class ImagingViewport;
class QGridLayout;

class ViewportGrid final : public QWidget
{
public:
    explicit ViewportGrid(QWidget *parent = nullptr);

    void setLayoutMode(int mode);
    void setToolMode(const QString &toolMode);
    void setActiveViewChangedCallback(std::function<void(const QString &)> callback);

private:
    void selectViewport(ImagingViewport *viewport);
    void toggleMaximize(ImagingViewport *viewport);
    void rebuildLayout();
    void clearLayout();

    QGridLayout *m_layout = nullptr;
    ImagingViewport *m_axial = nullptr;
    ImagingViewport *m_coronal = nullptr;
    ImagingViewport *m_sagittal = nullptr;
    ImagingViewport *m_volume = nullptr;
    ImagingViewport *m_projectionAp = nullptr;
    ImagingViewport *m_projectionLat = nullptr;
    ImagingViewport *m_maximized = nullptr;
    int m_layoutMode = 0;
    std::function<void(const QString &)> m_activeViewChangedCallback;
};
