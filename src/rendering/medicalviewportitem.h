#pragma once

#include "src/dicom/medicaldatacontroller.h"

#if CT_ENABLE_MEDICAL_BACKEND
#include <QQuickVTKItem.h>
using MedicalViewportBase = QQuickVTKItem;
#else
#include <QQuickPaintedItem>
using MedicalViewportBase = QQuickPaintedItem;
#endif

class MedicalViewportItem : public MedicalViewportBase
{
    Q_OBJECT
    Q_PROPERTY(ViewType viewType READ viewType WRITE setViewType NOTIFY viewTypeChanged)
    Q_PROPERTY(MedicalDataController *controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(double slicePosition READ slicePosition WRITE setSlicePosition NOTIFY slicePositionChanged)
    Q_PROPERTY(bool mip READ mip WRITE setMip NOTIFY mipChanged)
    Q_PROPERTY(VolumePreset volumePreset READ volumePreset WRITE setVolumePreset NOTIFY volumePresetChanged)
    Q_PROPERTY(bool showSegmentation READ showSegmentation WRITE setShowSegmentation NOTIFY showSegmentationChanged)
    Q_PROPERTY(double cropMinimum READ cropMinimum WRITE setCropMinimum NOTIFY cropChanged)
    Q_PROPERTY(double cropMaximum READ cropMaximum WRITE setCropMaximum NOTIFY cropChanged)

public:
    enum class ViewType { Axial = 0, Coronal = 1, Sagittal = 2, Volume3D = 3 };
    Q_ENUM(ViewType)
    enum class VolumePreset {
        ChestContrastPreset = 0,
        BonePreset = 1,
        LungPreset = 2,
        SoftTissuePreset = 3
    };
    Q_ENUM(VolumePreset)

    explicit MedicalViewportItem(QQuickItem *parent = nullptr);

    ViewType viewType() const { return m_viewType; }
    MedicalDataController *controller() const { return m_controller; }
    double slicePosition() const { return m_slicePosition; }
    bool mip() const { return m_mip; }
    VolumePreset volumePreset() const { return m_volumePreset; }
    bool showSegmentation() const { return m_showSegmentation; }
    double cropMinimum() const { return m_cropMinimum; }
    double cropMaximum() const { return m_cropMaximum; }

    void setViewType(ViewType type);
    void setController(MedicalDataController *controller);
    void setSlicePosition(double position);
    void setMip(bool mip);
    void setVolumePreset(VolumePreset preset);
    void setShowSegmentation(bool visible);
    void setCropMinimum(double value);
    void setCropMaximum(double value);
    Q_INVOKABLE void pickVoxel(double itemX, double itemY);

signals:
    void viewTypeChanged();
    void controllerChanged();
    void slicePositionChanged();
    void mipChanged();
    void volumePresetChanged();
    void showSegmentationChanged();
    void cropChanged();
    void voxelPicked(int voxelX, int voxelY, int voxelZ, int hu,
                     double normalizedX, double normalizedY);
    void voxelPickFailed(const QString &message);

private slots:
    void reloadData();
    void updateRenderState();

#if CT_ENABLE_MEDICAL_BACKEND
protected:
    vtkUserData initializeVTK(vtkRenderWindow *renderWindow) override;
#else
public:
    void paint(QPainter *painter) override;
#endif

private:
    ViewType m_viewType = ViewType::Axial;
    MedicalDataController *m_controller = nullptr;
    std::shared_ptr<const VolumeSnapshot> m_volume;
    std::shared_ptr<const MaskSnapshot> m_mask;
    double m_slicePosition = 0.5;
    double m_cropMinimum = 0.0;
    double m_cropMaximum = 1.0;
    bool m_mip = false;
    VolumePreset m_volumePreset = VolumePreset::BonePreset;
    bool m_showSegmentation = true;
};
