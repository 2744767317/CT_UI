#pragma once

#include <QWidget>

#include <functional>

class QMouseEvent;
class QPainter;

class ImagingViewport final : public QWidget
{
public:
    enum class ViewType { Axial, Coronal, Sagittal, Volume3D, ProjectionAP, ProjectionLAT };

    explicit ImagingViewport(ViewType type, QWidget *parent = nullptr);

    void setSelected(bool selected);
    void setToolMode(const QString &toolMode);
    QString displayName() const;
    ViewType viewType() const { return m_type; }
    void setActivatedCallback(std::function<void(ImagingViewport *)> callback);
    void setMaximizeCallback(std::function<void(ImagingViewport *)> callback);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void drawSlice(QPainter &p, const QRectF &area);
    void drawVolume(QPainter &p, const QRectF &area);
    void drawProjection(QPainter &p, const QRectF &area);
    void drawOverlay(QPainter &p, const QRectF &area);

    ViewType m_type;
    bool m_selected = false;
    QString m_toolMode = "选择";
    std::function<void(ImagingViewport *)> m_activatedCallback;
    std::function<void(ImagingViewport *)> m_maximizeCallback;
};
