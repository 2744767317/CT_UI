#include "medicalviewportitem.h"

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkColorTransferFunction.h>
#include <vtkFlyingEdges3D.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkImageData.h>
#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkInteractorStyleImage.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkLookupTable.h>
#include <vtkLight.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPropPicker.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include <QMetaObject>
#include <QPointer>

namespace {

class ViewportPipeline final : public vtkObject
{
public:
    static ViewportPipeline *New();
    vtkTypeMacro(ViewportPipeline, vtkObject);

    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkMatrix4x4> dataToWorld;
    vtkSmartPointer<vtkMatrix4x4> worldToData;
    vtkSmartPointer<vtkImageData> image;
    vtkSmartPointer<vtkImageData> mask;
    vtkSmartPointer<vtkImageSliceMapper> sliceMapper;
    vtkSmartPointer<vtkImageSlice> sliceActor;
    vtkSmartPointer<vtkImageSliceMapper> maskMapper;
    vtkSmartPointer<vtkImageSlice> maskActor;
    vtkSmartPointer<vtkGPUVolumeRayCastMapper> volumeMapper;
    vtkSmartPointer<vtkColorTransferFunction> color;
    vtkSmartPointer<vtkPiecewiseFunction> opacity;
    vtkSmartPointer<vtkVolumeProperty> volumeProperty;
    vtkSmartPointer<vtkVolume> volumeActor;
    vtkSmartPointer<vtkActor> segmentationActor;
};

vtkStandardNewMacro(ViewportPipeline);

vtkSmartPointer<vtkImageData> vtkImageFromVolume(const VolumeSnapshot &snapshot)
{
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(snapshot.dimensions.data());
    image->SetSpacing(snapshot.spacing.data());
    image->SetOrigin(0.0, 0.0, 0.0);
    image->AllocateScalars(VTK_SHORT, 1);
    std::memcpy(image->GetScalarPointer(), snapshot.pixels.data(),
                snapshot.pixels.size() * sizeof(short));
    return image;
}

vtkSmartPointer<vtkImageData> vtkImageFromMask(const MaskSnapshot &snapshot)
{
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(snapshot.dimensions.data());
    image->SetSpacing(snapshot.spacing.data());
    image->SetOrigin(0.0, 0.0, 0.0);
    image->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
    std::memcpy(image->GetScalarPointer(), snapshot.pixels.data(), snapshot.pixels.size());
    return image;
}

void updatePatientTransform(ViewportPipeline *pipeline, const VolumeSnapshot &snapshot)
{
    pipeline->dataToWorld = vtkSmartPointer<vtkMatrix4x4>::New();
    pipeline->dataToWorld->Identity();
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column)
            pipeline->dataToWorld->SetElement(row, column,
                                               snapshot.direction[row * 3 + column]);
        pipeline->dataToWorld->SetElement(row, 3, snapshot.origin[row]);
    }
    pipeline->worldToData = vtkSmartPointer<vtkMatrix4x4>::New();
    vtkMatrix4x4::Invert(pipeline->dataToWorld, pipeline->worldToData);
}

int orientationFor(MedicalViewportItem::ViewType type)
{
    if (type == MedicalViewportItem::ViewType::Sagittal)
        return 0;
    if (type == MedicalViewportItem::ViewType::Coronal)
        return 1;
    return 2;
}

void applyVolumePreset(ViewportPipeline *pipeline,
                       MedicalViewportItem::VolumePreset preset, bool mip)
{
    if (!pipeline->color || !pipeline->opacity || !pipeline->volumeProperty)
        return;

    pipeline->color->RemoveAllPoints();
    pipeline->opacity->RemoveAllPoints();

    vtkVolumeProperty *property = pipeline->volumeProperty;
    property->ShadeOn();
    property->SetAmbient(0.42);
    property->SetDiffuse(0.58);
    property->SetSpecular(0.12);
    property->SetSpecularPower(10.0);

    if (mip) {
        pipeline->color->AddRGBPoint(-3024.0, 0.0, 0.0, 0.0);
        pipeline->color->AddRGBPoint(-637.62, 1.0, 1.0, 1.0);
        pipeline->color->AddRGBPoint(700.0, 1.0, 1.0, 1.0);
        pipeline->color->AddRGBPoint(3071.0, 1.0, 1.0, 1.0);
        pipeline->opacity->AddPoint(-3024.0, 0.0);
        pipeline->opacity->AddPoint(-637.62, 0.0);
        pipeline->opacity->AddPoint(700.0, 1.0);
        pipeline->opacity->AddPoint(3071.0, 1.0);
        property->ShadeOff();
        property->SetAmbient(1.0);
        return;
    }

    switch (preset) {
    case MedicalViewportItem::VolumePreset::BonePreset:
        pipeline->color->AddRGBPoint(-1000.0, 0.0, 0.0, 0.0);
        pipeline->color->AddRGBPoint(152.19, 0.45, 0.20, 0.10);
        pipeline->color->AddRGBPoint(463.28, 0.76, 0.48, 0.25);
        pipeline->color->AddRGBPoint(659.15, 0.92, 0.76, 0.52);
        pipeline->color->AddRGBPoint(953.0, 1.0, 0.93, 0.80);
        pipeline->opacity->AddPoint(-1000.0, 0.0);
        pipeline->opacity->AddPoint(152.19, 0.0);
        pipeline->opacity->AddPoint(278.93, 0.190476);
        pipeline->opacity->AddPoint(952.0, 0.2);
        property->SetAmbient(0.2);
        property->SetDiffuse(1.0);
        property->SetSpecular(0.0);
        property->SetSpecularPower(1.0);
        break;
    case MedicalViewportItem::VolumePreset::LungPreset:
        pipeline->color->AddRGBPoint(-1000.0, 0.3, 0.3, 1.0);
        pipeline->color->AddRGBPoint(-600.0, 0.0, 0.0, 1.0);
        pipeline->color->AddRGBPoint(-530.0, 0.134704, 0.781726, 0.0724558);
        pipeline->color->AddRGBPoint(-460.0, 0.929244, 1.0, 0.109473);
        pipeline->color->AddRGBPoint(-400.0, 0.888889, 0.254949, 0.0240258);
        pipeline->color->AddRGBPoint(2952.0, 1.0, 0.3, 0.3);
        pipeline->opacity->AddPoint(-1000.0, 0.0);
        pipeline->opacity->AddPoint(-600.0, 0.0);
        pipeline->opacity->AddPoint(-599.0, 0.15);
        pipeline->opacity->AddPoint(-400.0, 0.15);
        pipeline->opacity->AddPoint(-399.0, 0.0);
        pipeline->opacity->AddPoint(2952.0, 0.0);
        property->SetAmbient(0.42);
        property->SetDiffuse(0.58);
        property->SetSpecular(0.0);
        property->SetSpecularPower(1.0);
        break;
    case MedicalViewportItem::VolumePreset::SoftTissuePreset:
        pipeline->color->AddRGBPoint(-2048.0, 0.0, 0.0, 0.0);
        pipeline->color->AddRGBPoint(-167.01, 0.0, 0.0, 0.0);
        pipeline->color->AddRGBPoint(-160.0, 0.0556356, 0.0556356, 0.0556356);
        pipeline->color->AddRGBPoint(240.0, 1.0, 1.0, 1.0);
        pipeline->color->AddRGBPoint(3661.0, 1.0, 1.0, 1.0);
        pipeline->opacity->AddPoint(-2048.0, 0.0);
        pipeline->opacity->AddPoint(-167.01, 0.0);
        pipeline->opacity->AddPoint(-160.0, 1.0);
        pipeline->opacity->AddPoint(240.0, 1.0);
        pipeline->opacity->AddPoint(3661.0, 1.0);
        property->ShadeOff();
        property->SetAmbient(0.2);
        property->SetDiffuse(1.0);
        property->SetSpecular(0.0);
        property->SetSpecularPower(1.0);
        break;
    case MedicalViewportItem::VolumePreset::ChestContrastPreset:
    default:
        pipeline->color->AddRGBPoint(-3024.0, 0.0, 0.0, 0.0);
        pipeline->color->AddRGBPoint(67.0106, 0.54902, 0.25098, 0.14902);
        pipeline->color->AddRGBPoint(251.105, 0.882353, 0.603922, 0.290196);
        pipeline->color->AddRGBPoint(439.291, 1.0, 0.937033, 0.954531);
        pipeline->color->AddRGBPoint(3071.0, 0.827451, 0.658824, 1.0);
        pipeline->opacity->AddPoint(-3024.0, 0.0);
        pipeline->opacity->AddPoint(67.0106, 0.0);
        pipeline->opacity->AddPoint(251.105, 0.446429);
        pipeline->opacity->AddPoint(439.291, 0.625);
        pipeline->opacity->AddPoint(3071.0, 0.616071);
        break;
    }
}

void configureSlice(ViewportPipeline *pipeline, MedicalViewportItem::ViewType type,
                    double slicePosition, double width, double level,
                    bool showSegmentation)
{
    const int orientation = orientationFor(type);
    const int sliceCount = pipeline->image->GetDimensions()[orientation];
    const int slice = std::clamp(static_cast<int>(slicePosition * (sliceCount - 1)),
                                 0, std::max(0, sliceCount - 1));

    pipeline->sliceMapper = vtkSmartPointer<vtkImageSliceMapper>::New();
    pipeline->sliceMapper->SetInputData(pipeline->image);
    pipeline->sliceMapper->SetOrientation(orientation);
    pipeline->sliceMapper->SetSliceNumber(slice);
    pipeline->sliceActor = vtkSmartPointer<vtkImageSlice>::New();
    pipeline->sliceActor->SetMapper(pipeline->sliceMapper);
    pipeline->sliceActor->SetUserMatrix(pipeline->dataToWorld);
    pipeline->sliceActor->GetProperty()->SetColorWindow(width);
    pipeline->sliceActor->GetProperty()->SetColorLevel(level);
    pipeline->sliceActor->GetProperty()->SetInterpolationTypeToLinear();
    pipeline->renderer->AddViewProp(pipeline->sliceActor);

    if (pipeline->mask) {
        vtkNew<vtkLookupTable> lookup;
        lookup->SetNumberOfTableValues(2);
        lookup->SetRange(0.0, 1.0);
        lookup->SetTableValue(0, 0.0, 0.0, 0.0, 0.0);
        lookup->SetTableValue(1, 0.95, 0.38, 0.08, 0.72);
        lookup->Build();
        pipeline->maskMapper = vtkSmartPointer<vtkImageSliceMapper>::New();
        pipeline->maskMapper->SetInputData(pipeline->mask);
        pipeline->maskMapper->SetOrientation(orientation);
        pipeline->maskMapper->SetSliceNumber(slice);
        pipeline->maskActor = vtkSmartPointer<vtkImageSlice>::New();
        pipeline->maskActor->SetMapper(pipeline->maskMapper);
        pipeline->maskActor->SetUserMatrix(pipeline->dataToWorld);
        pipeline->maskActor->GetProperty()->SetLookupTable(lookup);
        pipeline->maskActor->GetProperty()->UseLookupTableScalarRangeOn();
        pipeline->maskActor->SetVisibility(showSegmentation);
        pipeline->renderer->AddViewProp(pipeline->maskActor);
    }
}

void configureVolume(ViewportPipeline *pipeline, bool mip, double cropMinimum,
                     double cropMaximum, MedicalViewportItem::VolumePreset preset,
                     bool showSegmentation)
{
    pipeline->volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
    pipeline->volumeMapper->SetInputData(pipeline->image);
    pipeline->volumeMapper->SetAutoAdjustSampleDistances(true);
    pipeline->volumeMapper->SetLockSampleDistanceToInputSpacing(false);
    if (mip)
        pipeline->volumeMapper->SetBlendModeToMaximumIntensity();
    else
        pipeline->volumeMapper->SetBlendModeToComposite();

    double bounds[6];
    pipeline->image->GetBounds(bounds);
    const double zMinimum = bounds[4] + (bounds[5] - bounds[4]) * cropMinimum;
    const double zMaximum = bounds[4] + (bounds[5] - bounds[4]) * cropMaximum;
    pipeline->volumeMapper->SetCroppingRegionPlanes(
        bounds[0], bounds[1], bounds[2], bounds[3], zMinimum, zMaximum);
    pipeline->volumeMapper->SetCropping(cropMinimum > 0.0 || cropMaximum < 1.0);

    pipeline->color = vtkSmartPointer<vtkColorTransferFunction>::New();
    pipeline->opacity = vtkSmartPointer<vtkPiecewiseFunction>::New();
    pipeline->volumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();
    pipeline->volumeProperty->SetColor(pipeline->color);
    pipeline->volumeProperty->SetScalarOpacity(pipeline->opacity);
    pipeline->volumeProperty->SetInterpolationTypeToLinear();
    applyVolumePreset(pipeline, preset, mip);

    pipeline->volumeActor = vtkSmartPointer<vtkVolume>::New();
    pipeline->volumeActor->SetMapper(pipeline->volumeMapper);
    pipeline->volumeActor->SetProperty(pipeline->volumeProperty);
    pipeline->volumeActor->SetUserMatrix(pipeline->dataToWorld);
    pipeline->renderer->AddVolume(pipeline->volumeActor);

    if (pipeline->mask) {
        vtkNew<vtkFlyingEdges3D> surface;
        surface->SetInputData(pipeline->mask);
        surface->SetValue(0, 0.5);
        surface->ComputeNormalsOn();
        surface->Update();
        auto surfaceData = vtkSmartPointer<vtkPolyData>::New();
        surfaceData->ShallowCopy(surface->GetOutput());
        vtkNew<vtkPolyDataMapper> mapper;
        mapper->SetInputData(surfaceData);
        mapper->ScalarVisibilityOff();
        pipeline->segmentationActor = vtkSmartPointer<vtkActor>::New();
        pipeline->segmentationActor->SetMapper(mapper);
        pipeline->segmentationActor->SetUserMatrix(pipeline->dataToWorld);
        pipeline->segmentationActor->GetProperty()->SetColor(0.95, 0.38, 0.08);
        pipeline->segmentationActor->GetProperty()->SetOpacity(0.82);
        pipeline->segmentationActor->GetProperty()->SetAmbient(0.34);
        pipeline->segmentationActor->GetProperty()->SetDiffuse(0.66);
        pipeline->segmentationActor->GetProperty()->SetSpecular(0.12);
        pipeline->segmentationActor->SetVisibility(showSegmentation);
        pipeline->renderer->AddActor(pipeline->segmentationActor);
    }
}

void rebuildPipeline(ViewportPipeline *pipeline, vtkRenderWindow *renderWindow,
                     const std::shared_ptr<const VolumeSnapshot> &volume,
                     const std::shared_ptr<const MaskSnapshot> &mask,
                     MedicalViewportItem::ViewType type, double slicePosition,
                     bool mip, MedicalViewportItem::VolumePreset preset,
                     bool showSegmentation, double cropMinimum,
                     double cropMaximum, double width, double level)
{
    pipeline->renderer->RemoveAllViewProps();
    pipeline->image = nullptr;
    pipeline->mask = nullptr;
    pipeline->sliceMapper = nullptr;
    pipeline->sliceActor = nullptr;
    pipeline->maskMapper = nullptr;
    pipeline->maskActor = nullptr;
    pipeline->volumeMapper = nullptr;
    pipeline->volumeProperty = nullptr;
    pipeline->volumeActor = nullptr;
    pipeline->segmentationActor = nullptr;

    if (!volume || volume->pixels.empty())
        return;

    pipeline->image = vtkImageFromVolume(*volume);
    updatePatientTransform(pipeline, *volume);
    if (mask && mask->dimensions == volume->dimensions)
        pipeline->mask = vtkImageFromMask(*mask);

    if (type == MedicalViewportItem::ViewType::Volume3D)
        configureVolume(pipeline, mip, cropMinimum, cropMaximum, preset,
                        showSegmentation);
    else
        configureSlice(pipeline, type, slicePosition, width, level, showSegmentation);

    if (renderWindow->GetInteractor()) {
        renderWindow->GetInteractor()->SetDesiredUpdateRate(30.0);
        renderWindow->GetInteractor()->SetStillUpdateRate(0.5);
        if (type == MedicalViewportItem::ViewType::Volume3D) {
            vtkNew<vtkInteractorStyleTrackballCamera> style;
            renderWindow->GetInteractor()->SetInteractorStyle(style);
        } else {
            vtkNew<vtkInteractorStyleImage> style;
            renderWindow->GetInteractor()->SetInteractorStyle(style);
        }
    }
    auto *camera = pipeline->renderer->GetActiveCamera();
    if (type == MedicalViewportItem::ViewType::Sagittal) {
        camera->SetPosition(1.0, 0.0, 0.0);
        camera->SetFocalPoint(0.0, 0.0, 0.0);
        camera->SetViewUp(0.0, 0.0, 1.0);
    } else if (type == MedicalViewportItem::ViewType::Coronal) {
        camera->SetPosition(0.0, -1.0, 0.0);
        camera->SetFocalPoint(0.0, 0.0, 0.0);
        camera->SetViewUp(0.0, 0.0, 1.0);
    } else if (type == MedicalViewportItem::ViewType::Axial) {
        camera->SetPosition(0.0, 0.0, 1.0);
        camera->SetFocalPoint(0.0, 0.0, 0.0);
        camera->SetViewUp(0.0, 1.0, 0.0);
    } else if (type == MedicalViewportItem::ViewType::Volume3D) {
        // DICOM LPS: view from anterior toward posterior, with superior up.
        camera->SetPosition(0.0, -1.0, 0.0);
        camera->SetFocalPoint(0.0, 0.0, 0.0);
        camera->SetViewUp(0.0, 0.0, 1.0);
    }
    pipeline->renderer->ResetCamera();
    if (type == MedicalViewportItem::ViewType::Volume3D) {
        camera->Azimuth(-18.0);
        camera->Elevation(8.0);
        camera->OrthogonalizeViewUp();
        camera->Dolly(1.2);
    }
    if (type != MedicalViewportItem::ViewType::Volume3D)
        pipeline->renderer->GetActiveCamera()->ParallelProjectionOn();
    pipeline->renderer->ResetCameraClippingRange();
}

} // namespace

MedicalViewportItem::MedicalViewportItem(QQuickItem *parent)
    : MedicalViewportBase(parent)
{
}

QQuickVTKItem::vtkUserData MedicalViewportItem::initializeVTK(vtkRenderWindow *renderWindow)
{
    auto pipeline = vtkSmartPointer<ViewportPipeline>::New();
    pipeline->renderer = vtkSmartPointer<vtkRenderer>::New();
    pipeline->renderer->SetBackground(0.018, 0.025, 0.029);
    pipeline->renderer->SetBackground2(0.055, 0.064, 0.069);
    pipeline->renderer->GradientBackgroundOn();
    pipeline->renderer->SetLayer(0);
    vtkNew<vtkLight> headlight;
    headlight->SetLightTypeToHeadlight();
    headlight->SetIntensity(1.1);
    pipeline->renderer->RemoveAllLights();
    pipeline->renderer->AddLight(headlight);
    renderWindow->SetNumberOfLayers(1);
    renderWindow->AddRenderer(pipeline->renderer);

    const double width = m_controller ? m_controller->windowWidth() : 400.0;
    const double level = m_controller ? m_controller->windowLevel() : 40.0;
    rebuildPipeline(pipeline, renderWindow, m_volume, m_mask, m_viewType,
                    m_slicePosition, m_mip, m_volumePreset, m_showSegmentation,
                    m_cropMinimum, m_cropMaximum, width, level);
    return pipeline;
}

void MedicalViewportItem::setViewType(ViewType type)
{
    if (m_viewType == type)
        return;
    m_viewType = type;
    emit viewTypeChanged();
    reloadData();
}

void MedicalViewportItem::setController(MedicalDataController *controller)
{
    if (m_controller == controller)
        return;
    if (m_controller)
        disconnect(m_controller, nullptr, this, nullptr);
    m_controller = controller;
    if (m_controller) {
        connect(m_controller, &MedicalDataController::dataChanged,
                this, &MedicalViewportItem::reloadData);
        connect(m_controller, &MedicalDataController::segmentationChanged,
                this, &MedicalViewportItem::reloadData);
        connect(m_controller, &MedicalDataController::windowingChanged,
                this, &MedicalViewportItem::updateRenderState);
    }
    emit controllerChanged();
    reloadData();
}

void MedicalViewportItem::setSlicePosition(double position)
{
    position = std::clamp(position, 0.0, 1.0);
    if (qFuzzyCompare(m_slicePosition, position))
        return;
    m_slicePosition = position;
    emit slicePositionChanged();
    updateRenderState();
}

void MedicalViewportItem::setMip(bool mip)
{
    if (m_mip == mip)
        return;
    m_mip = mip;
    emit mipChanged();
    reloadData();
}

void MedicalViewportItem::setVolumePreset(VolumePreset preset)
{
    if (m_volumePreset == preset)
        return;
    m_volumePreset = preset;
    emit volumePresetChanged();
    updateRenderState();
}

void MedicalViewportItem::setShowSegmentation(bool visible)
{
    if (m_showSegmentation == visible)
        return;
    m_showSegmentation = visible;
    emit showSegmentationChanged();
    updateRenderState();
}

void MedicalViewportItem::setCropMinimum(double value)
{
    value = std::clamp(value, 0.0, m_cropMaximum - 0.01);
    if (qFuzzyCompare(m_cropMinimum, value))
        return;
    m_cropMinimum = value;
    emit cropChanged();
    reloadData();
}

void MedicalViewportItem::setCropMaximum(double value)
{
    value = std::clamp(value, m_cropMinimum + 0.01, 1.0);
    if (qFuzzyCompare(m_cropMaximum, value))
        return;
    m_cropMaximum = value;
    emit cropChanged();
    reloadData();
}

void MedicalViewportItem::pickVoxel(double itemX, double itemY)
{
    if (m_viewType == ViewType::Volume3D || !m_controller || !m_volume
        || width() <= 0.0 || height() <= 0.0)
        return;

    const double itemWidth = width();
    const double itemHeight = height();
    const double normalizedX = std::clamp(itemX / itemWidth, 0.0, 1.0);
    const double normalizedY = std::clamp(itemY / itemHeight, 0.0, 1.0);
    const auto view = QPointer<MedicalViewportItem>(this);
    const auto controller = QPointer<MedicalDataController>(m_controller);

    dispatch_async([view, controller, normalizedX, normalizedY]
                   (vtkRenderWindow *window, vtkUserData userData) {
        auto *pipeline = ViewportPipeline::SafeDownCast(userData);
        if (!pipeline || !pipeline->renderer || !pipeline->sliceActor
            || !pipeline->image || !pipeline->worldToData || !view || !controller)
            return;

        const int *renderSize = window->GetSize();
        const double displayX = normalizedX * std::max(0, renderSize[0] - 1);
        const double displayY = (1.0 - normalizedY) * std::max(0, renderSize[1] - 1);
        vtkNew<vtkPropPicker> picker;
        picker->PickFromListOn();
        picker->AddPickList(pipeline->sliceActor);
        if (!picker->Pick(displayX, displayY, 0.0, pipeline->renderer)) {
            QMetaObject::invokeMethod(view.data(), [view]() {
                if (view)
                    emit view->voxelPickFailed(QStringLiteral("点击位置不在当前切片图像内。"));
            }, Qt::QueuedConnection);
            return;
        }

        double world[3];
        double worldPoint[4];
        double dataPoint[4];
        double continuousIndex[3];
        picker->GetPickPosition(world);
        worldPoint[0] = world[0];
        worldPoint[1] = world[1];
        worldPoint[2] = world[2];
        worldPoint[3] = 1.0;
        pipeline->worldToData->MultiplyPoint(worldPoint, dataPoint);
        pipeline->image->TransformPhysicalPointToContinuousIndex(dataPoint, continuousIndex);
        const int *dimensions = pipeline->image->GetDimensions();
        std::array<int, 3> index {
            static_cast<int>(std::lround(continuousIndex[0])),
            static_cast<int>(std::lround(continuousIndex[1])),
            static_cast<int>(std::lround(continuousIndex[2]))
        };
        for (int axis = 0; axis < 3; ++axis)
            index[axis] = std::clamp(index[axis], 0, dimensions[axis] - 1);

        QMetaObject::invokeMethod(view.data(),
            [view, controller, index, normalizedX, normalizedY]() {
                if (!view || !controller
                    || !controller->setRegionGrowingSeed(index[0], index[1], index[2]))
                    return;
                emit view->voxelPicked(index[0], index[1], index[2],
                                       controller->regionGrowingSeedValue(),
                                       normalizedX, normalizedY);
            }, Qt::QueuedConnection);
    });
}

void MedicalViewportItem::reloadData()
{
    m_volume = m_controller ? m_controller->volumeSnapshot() : nullptr;
    m_mask = m_controller ? m_controller->maskSnapshot() : nullptr;
    const auto volume = m_volume;
    const auto mask = m_mask;
    const auto type = m_viewType;
    const double slice = m_slicePosition;
    const bool mipMode = m_mip;
    const auto preset = m_volumePreset;
    const bool segmentation = m_showSegmentation;
    const double cropMin = m_cropMinimum;
    const double cropMax = m_cropMaximum;
    const double width = m_controller ? m_controller->windowWidth() : 400.0;
    const double level = m_controller ? m_controller->windowLevel() : 40.0;
    dispatch_async([volume, mask, type, slice, mipMode, preset, segmentation,
                    cropMin, cropMax, width, level](vtkRenderWindow *window,
                                                   vtkUserData userData) {
        auto *pipeline = ViewportPipeline::SafeDownCast(userData);
        if (pipeline)
            rebuildPipeline(pipeline, window, volume, mask, type, slice, mipMode, preset,
                            segmentation, cropMin, cropMax, width, level);
    });
    scheduleRender();
}

void MedicalViewportItem::updateRenderState()
{
    const int orientation = orientationFor(m_viewType);
    const double slicePosition = m_slicePosition;
    const bool segmentation = m_showSegmentation;
    const bool mipMode = m_mip;
    const auto preset = m_volumePreset;
    const double cropMin = m_cropMinimum;
    const double cropMax = m_cropMaximum;
    const double width = m_controller ? m_controller->windowWidth() : 400.0;
    const double level = m_controller ? m_controller->windowLevel() : 40.0;
    dispatch_async([orientation, slicePosition, segmentation, mipMode, preset,
                    cropMin, cropMax, width, level](vtkRenderWindow *, vtkUserData userData) {
        auto *pipeline = ViewportPipeline::SafeDownCast(userData);
        if (!pipeline)
            return;
        if (pipeline->sliceMapper && pipeline->image) {
            const int count = pipeline->image->GetDimensions()[orientation];
            const int slice = std::clamp(static_cast<int>(slicePosition * (count - 1)),
                                         0, std::max(0, count - 1));
            pipeline->sliceMapper->SetSliceNumber(slice);
            pipeline->sliceActor->GetProperty()->SetColorWindow(width);
            pipeline->sliceActor->GetProperty()->SetColorLevel(level);
            if (pipeline->maskMapper)
                pipeline->maskMapper->SetSliceNumber(slice);
        }
        if (pipeline->maskActor)
            pipeline->maskActor->SetVisibility(segmentation);
        if (pipeline->segmentationActor)
            pipeline->segmentationActor->SetVisibility(segmentation);
        if (pipeline->volumeMapper && pipeline->image) {
            if (mipMode)
                pipeline->volumeMapper->SetBlendModeToMaximumIntensity();
            else
                pipeline->volumeMapper->SetBlendModeToComposite();
            double bounds[6];
            pipeline->image->GetBounds(bounds);
            const double zMin = bounds[4] + (bounds[5] - bounds[4]) * cropMin;
            const double zMax = bounds[4] + (bounds[5] - bounds[4]) * cropMax;
            pipeline->volumeMapper->SetCroppingRegionPlanes(
                bounds[0], bounds[1], bounds[2], bounds[3], zMin, zMax);
            pipeline->volumeMapper->SetCropping(cropMin > 0.0 || cropMax < 1.0);
            applyVolumePreset(pipeline, preset, mipMode);
        }
    });
    scheduleRender();
}
