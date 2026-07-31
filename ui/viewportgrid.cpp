#include "viewportgrid.h"

#include "imagingviewport.h"

#include <QGridLayout>
#include <QLayoutItem>

#include <utility>

ViewportGrid::ViewportGrid(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("viewportGrid");
    setMinimumWidth(680);
    m_layout = new QGridLayout(this);
    m_layout->setContentsMargins(3, 3, 3, 3);
    m_layout->setSpacing(3);

    m_axial = new ImagingViewport(ImagingViewport::ViewType::Axial, this);
    m_coronal = new ImagingViewport(ImagingViewport::ViewType::Coronal, this);
    m_sagittal = new ImagingViewport(ImagingViewport::ViewType::Sagittal, this);
    m_volume = new ImagingViewport(ImagingViewport::ViewType::Volume3D, this);
    m_projectionAp = new ImagingViewport(ImagingViewport::ViewType::ProjectionAP, this);
    m_projectionLat = new ImagingViewport(ImagingViewport::ViewType::ProjectionLAT, this);

    const QList<ImagingViewport *> views = {
        m_axial, m_coronal, m_sagittal, m_volume, m_projectionAp, m_projectionLat
    };
    for (auto *view : views) {
        view->setActivatedCallback([this](ImagingViewport *selected) { selectViewport(selected); });
        view->setMaximizeCallback([this](ImagingViewport *selected) { toggleMaximize(selected); });
    }

    rebuildLayout();
    selectViewport(m_axial);
}

void ViewportGrid::setLayoutMode(int mode)
{
    if (mode < 0 || mode > 3)
        return;
    m_layoutMode = mode;
    m_maximized = nullptr;
    rebuildLayout();
    selectViewport(mode == 2 ? m_volume : mode == 3 ? m_projectionAp : m_axial);
}

void ViewportGrid::setToolMode(const QString &toolMode)
{
    const QList<ImagingViewport *> views = {
        m_axial, m_coronal, m_sagittal, m_volume, m_projectionAp, m_projectionLat
    };
    for (auto *view : views)
        view->setToolMode(toolMode);
}

void ViewportGrid::setActiveViewChangedCallback(std::function<void(const QString &)> callback)
{
    m_activeViewChangedCallback = std::move(callback);
}

void ViewportGrid::selectViewport(ImagingViewport *viewport)
{
    const QList<ImagingViewport *> views = {
        m_axial, m_coronal, m_sagittal, m_volume, m_projectionAp, m_projectionLat
    };
    for (auto *view : views)
        view->setSelected(view == viewport);
    if (m_activeViewChangedCallback)
        m_activeViewChangedCallback(viewport->displayName());
}

void ViewportGrid::toggleMaximize(ImagingViewport *viewport)
{
    m_maximized = m_maximized ? nullptr : viewport;
    rebuildLayout();
    selectViewport(viewport);
}

void ViewportGrid::rebuildLayout()
{
    clearLayout();
    const QList<ImagingViewport *> views = {
        m_axial, m_coronal, m_sagittal, m_volume, m_projectionAp, m_projectionLat
    };
    for (auto *view : views)
        view->hide();

    if (m_maximized) {
        m_layout->addWidget(m_maximized, 0, 0);
        m_maximized->show();
        return;
    }

    if (m_layoutMode == 0) {
        m_layout->addWidget(m_axial, 0, 0);
        m_layout->addWidget(m_volume, 0, 1);
        m_layout->addWidget(m_coronal, 1, 0);
        m_layout->addWidget(m_sagittal, 1, 1);
        m_axial->show();
        m_volume->show();
        m_coronal->show();
        m_sagittal->show();
        m_layout->setRowStretch(0, 1);
        m_layout->setRowStretch(1, 1);
        m_layout->setRowStretch(2, 0);
        m_layout->setColumnStretch(0, 1);
        m_layout->setColumnStretch(1, 1);
    } else if (m_layoutMode == 1) {
        m_layout->addWidget(m_volume, 0, 0, 3, 1);
        m_layout->addWidget(m_axial, 0, 1);
        m_layout->addWidget(m_coronal, 1, 1);
        m_layout->addWidget(m_sagittal, 2, 1);
        m_volume->show();
        m_axial->show();
        m_coronal->show();
        m_sagittal->show();
        m_layout->setRowStretch(0, 1);
        m_layout->setRowStretch(1, 1);
        m_layout->setRowStretch(2, 1);
        m_layout->setColumnStretch(0, 3);
        m_layout->setColumnStretch(1, 2);
    } else if (m_layoutMode == 2) {
        m_layout->addWidget(m_volume, 0, 0);
        m_volume->show();
        m_layout->setRowStretch(0, 1);
        m_layout->setColumnStretch(0, 1);
    } else {
        m_layout->addWidget(m_projectionAp, 0, 0);
        m_layout->addWidget(m_projectionLat, 0, 1);
        m_projectionAp->show();
        m_projectionLat->show();
        m_layout->setRowStretch(0, 1);
        m_layout->setColumnStretch(0, 1);
        m_layout->setColumnStretch(1, 1);
    }
}

void ViewportGrid::clearLayout()
{
    while (auto *item = m_layout->takeAt(0))
        delete item;
    for (int index = 0; index < 4; ++index) {
        m_layout->setRowStretch(index, 0);
        m_layout->setColumnStretch(index, 0);
    }
}
