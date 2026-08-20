#pragma once

#include "src/annotation/annotationcontroller.h"
#include "src/dicom/medicaldatacontroller.h"

#include <QVariantMap>
#include <QColor>

#include <atomic>
#include <cstdint>

#if CT_ENABLE_MEDICAL_BACKEND
#include <QQuickVTKItem.h>
using MedicalViewportBase = QQuickVTKItem;
#else
#include <QQuickPaintedItem>
using MedicalViewportBase = QQuickPaintedItem;
#endif

// QML 与 VTK 的边界控件。GUI 线程只更新属性，所有 VTK 对象都在渲染线程中访问。
class MedicalViewportItem : public MedicalViewportBase
{
    Q_OBJECT
    Q_PROPERTY(ViewType viewType READ viewType WRITE setViewType NOTIFY viewTypeChanged)
    Q_PROPERTY(MedicalDataController *controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(AnnotationController *annotations READ annotations WRITE setAnnotations NOTIFY annotationsChanged)
    Q_PROPERTY(double slicePosition READ slicePosition WRITE setSlicePosition NOTIFY slicePositionChanged)
    Q_PROPERTY(bool mip READ mip WRITE setMip NOTIFY mipChanged)
    Q_PROPERTY(VolumePreset volumePreset READ volumePreset WRITE setVolumePreset NOTIFY volumePresetChanged)
    Q_PROPERTY(bool showSegmentation READ showSegmentation WRITE setShowSegmentation NOTIFY showSegmentationChanged)
    Q_PROPERTY(bool pairedProjection READ pairedProjection WRITE setPairedProjection NOTIFY pairedProjectionChanged)
    Q_PROPERTY(bool showImage READ showImage WRITE setShowImage NOTIFY showImageChanged)
    Q_PROPERTY(double segmentationOpacity READ segmentationOpacity WRITE setSegmentationOpacity NOTIFY segmentationOpacityChanged)
    Q_PROPERTY(QColor segmentationColor READ segmentationColor WRITE setSegmentationColor NOTIFY segmentationColorChanged)
    Q_PROPERTY(int rotationQuarterTurns READ rotationQuarterTurns WRITE setRotationQuarterTurns NOTIFY orientationChanged)
    Q_PROPERTY(bool flipHorizontal READ flipHorizontal WRITE setFlipHorizontal NOTIFY orientationChanged)
    Q_PROPERTY(bool flipVertical READ flipVertical WRITE setFlipVertical NOTIFY orientationChanged)
    Q_PROPERTY(double cropMinimum READ cropMinimum WRITE setCropMinimum NOTIFY cropChanged)
    Q_PROPERTY(double cropMaximum READ cropMaximum WRITE setCropMaximum NOTIFY cropChanged)
    Q_PROPERTY(bool showAnnotations READ showAnnotations WRITE setShowAnnotations NOTIFY showAnnotationsChanged)
    Q_PROPERTY(bool renderEnabled READ renderEnabled WRITE setRenderEnabled NOTIFY renderEnabledChanged)
    Q_PROPERTY(int sliceCount READ sliceCount NOTIFY sliceCountChanged)

public:
    enum class ViewType { Axial = 0, Coronal = 1, Sagittal = 2, Volume3D = 3 };
    Q_ENUM(ViewType)
    enum class VolumePreset {
        ChestContrastPreset = 0,
        BonePreset = 1,
        LungPreset = 2,
        SoftTissuePreset = 3,
        VascularPreset = 4
    };
    Q_ENUM(VolumePreset)

    explicit MedicalViewportItem(QQuickItem *parent = nullptr);

    ViewType viewType() const { return m_viewType; }
    MedicalDataController *controller() const { return m_controller; }
    AnnotationController *annotations() const { return m_annotations; }
    double slicePosition() const { return m_slicePosition; }
    bool mip() const { return m_mip; }
    VolumePreset volumePreset() const { return m_volumePreset; }
    bool showSegmentation() const { return m_showSegmentation; }
    bool pairedProjection() const { return m_pairedProjection; }
    bool showImage() const { return m_showImage; }
    double segmentationOpacity() const { return m_segmentationOpacity; }
    QColor segmentationColor() const { return m_segmentationColor; }
    int rotationQuarterTurns() const { return m_rotationQuarterTurns; }
    bool flipHorizontal() const { return m_flipHorizontal; }
    bool flipVertical() const { return m_flipVertical; }
    double cropMinimum() const { return m_cropMinimum; }
    double cropMaximum() const { return m_cropMaximum; }
    bool showAnnotations() const { return m_showAnnotations; }
    bool renderEnabled() const { return m_renderEnabled; }
    int sliceCount() const;

    void setViewType(ViewType type);
    void setController(MedicalDataController *controller);
    void setAnnotations(AnnotationController *annotations);
    void setSlicePosition(double position);
    void setMip(bool mip);
    void setVolumePreset(VolumePreset preset);
    void setShowSegmentation(bool visible);
    void setPairedProjection(bool paired);
    void setShowImage(bool visible);
    void setSegmentationOpacity(double opacity);
    void setSegmentationColor(const QColor &color);
    void setRotationQuarterTurns(int turns);
    void setFlipHorizontal(bool flipped);
    void setFlipVertical(bool flipped);
    void setCropMinimum(double value);
    void setCropMaximum(double value);
    void setShowAnnotations(bool visible);
    void setRenderEnabled(bool enabled);
    Q_INVOKABLE void pickVoxel(double itemX, double itemY, bool updateSeed = true);
    /// 按当前切片几何关系同步映射点击到体素（不依赖 VTK PropPicker）。
    Q_INVOKABLE bool mapClickToVoxel(double itemX, double itemY, bool updateSeed = false);
    /// 无副作用地查询屏幕点对应的 IJK，供 Shift 联动切片浏览使用。
    Q_INVOKABLE QVariantMap mapClickToVoxelInfo(double itemX, double itemY) const;
    /// 将 IJK 映射到当前视口显示坐标，供切片交叉线叠加层使用。
    Q_INVOKABLE QVariantMap mapVoxelToDisplay(int voxelX, int voxelY, int voxelZ) const;
    Q_INVOKABLE bool beginAnnotationInteraction(double itemX, double itemY,
                                                double tolerancePx = 14.0);
    Q_INVOKABLE bool updateAnnotationControlPoint(int nodeId, int pointIndex,
                                                  double itemX, double itemY);
    /// 命中已提交标记的控制点；未命中返回空 map。
    Q_INVOKABLE QVariantMap hitTestControlPoint(double itemX, double itemY,
                                                double tolerancePx = 14.0);
    Q_INVOKABLE void panBy(double deltaX, double deltaY);
    Q_INVOKABLE void zoomBy(double factor, double anchorX, double anchorY);
    Q_INVOKABLE void resetView();
    Q_INVOKABLE QVariantMap captureRgbPacked(int magnification = 1,
                                             bool fitEntireVolume = false,
                                             bool includeAnnotations = true);

signals:
    void viewTypeChanged();
    void controllerChanged();
    void annotationsChanged();
    void slicePositionChanged();
    void mipChanged();
    void volumePresetChanged();
    void showSegmentationChanged();
    void pairedProjectionChanged();
    void showImageChanged();
    void segmentationOpacityChanged();
    void segmentationColorChanged();
    void orientationChanged();
    void cropChanged();
    void showAnnotationsChanged();
    void renderEnabledChanged();
    void sliceCountChanged();
    void voxelPicked(int voxelX, int voxelY, int voxelZ, int hu,
                     double normalizedX, double normalizedY);
    void voxelPickFailed(const QString &message);
    void annotationControlPointPressed(int nodeId, int pointIndex);

private slots:
    void reloadData();
    void updateRenderState();
    void updateCropState();
    void updateSliceState();
    void updateSliceCameraState();
    void syncAnnotationActors();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

#if CT_ENABLE_MEDICAL_BACKEND
    vtkUserData initializeVTK(vtkRenderWindow *renderWindow) override;
#else
public:
    void paint(QPainter *painter) override;
#endif

private:
    bool mapItemPositionToWorld(double itemX, double itemY,
                                QVector3D *worldOut, int *voxelOut = nullptr) const;

    ViewType m_viewType = ViewType::Axial;
    MedicalDataController *m_controller = nullptr;
    AnnotationController *m_annotations = nullptr;
    std::shared_ptr<const VolumeSnapshot> m_volume;
    std::shared_ptr<const MaskSnapshot> m_mask;
    double m_slicePosition = 0.5;
    double m_cropMinimum = 0.0;
    double m_cropMaximum = 1.0;
    bool m_mip = false;
    VolumePreset m_volumePreset = VolumePreset::BonePreset;
    bool m_showSegmentation = true;
    bool m_pairedProjection = false;
    bool m_showImage = true;
    bool m_showAnnotations = true;
    bool m_renderEnabled = true;
    double m_segmentationOpacity = 0.72;
    QColor m_segmentationColor = QColor(QStringLiteral("#F0783C"));
    int m_rotationQuarterTurns = 0;
    bool m_flipHorizontal = false;
    bool m_flipVertical = false;
    double m_viewZoom = 1.0;
    double m_viewPanX = 0.0;
    double m_viewPanY = 0.0;
    // Each high-frequency state family has its own serial. Older VTK tasks can
    // exit before touching the pipeline without suppressing an unrelated
    // change (for example, a crop update must not discard a windowing update).
    std::shared_ptr<std::atomic<std::uint64_t>> m_renderStateSerial =
        std::make_shared<std::atomic<std::uint64_t>>(0);
    std::shared_ptr<std::atomic<std::uint64_t>> m_cropStateSerial =
        std::make_shared<std::atomic<std::uint64_t>>(0);
    std::shared_ptr<std::atomic<std::uint64_t>> m_sliceStateSerial =
        std::make_shared<std::atomic<std::uint64_t>>(0);
    std::shared_ptr<std::atomic<std::uint64_t>> m_cameraStateSerial =
        std::make_shared<std::atomic<std::uint64_t>>(0);
    std::shared_ptr<std::atomic<std::uint64_t>> m_annotationStateSerial =
        std::make_shared<std::atomic<std::uint64_t>>(0);
    std::shared_ptr<std::atomic<std::uint64_t>> m_pipelineReloadSerial =
        std::make_shared<std::atomic<std::uint64_t>>(0);
};
