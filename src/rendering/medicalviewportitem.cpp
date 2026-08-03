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
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>

#include <algorithm>
#include <cstring>

namespace {

class ViewportPipeline final : public vtkObject
{
public:
    static ViewportPipeline *New();
    vtkTypeMacro(ViewportPipeline, vtkObject);

    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkRenderer> segmentationRenderer;
    vtkSmartPointer<vtkImageData> image;
    vtkSmartPointer<vtkImageData> mask;
    vtkSmartPointer<vtkImageSliceMapper> sliceMapper;
    vtkSmartPointer<vtkImageSlice> sliceActor;
    vtkSmartPointer<vtkImageSliceMapper> maskMapper;
    vtkSmartPointer<vtkImageSlice> maskActor;
    vtkSmartPointer<vtkGPUVolumeRayCastMapper> volumeMapper;
    vtkSmartPointer<vtkColorTransferFunction> color;
    vtkSmartPointer<vtkPiecewiseFunction> opacity;
    vtkSmartPointer<vtkVolume> volumeActor;
    vtkSmartPointer<vtkActor> segmentationActor;
};

vtkStandardNewMacro(ViewportPipeline);

vtkSmartPointer<vtkImageData> vtkImageFromVolume(const VolumeSnapshot &snapshot)
{
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(snapshot.dimensions.data());
    image->SetSpacing(snapshot.spacing.data());
    image->SetOrigin(snapshot.origin.data());
    image->SetDirectionMatrix(snapshot.direction.data());
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
    image->SetOrigin(snapshot.origin.data());
    image->SetDirectionMatrix(snapshot.direction.data());
    image->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
    std::memcpy(image->GetScalarPointer(), snapshot.pixels.data(), snapshot.pixels.size());
    return image;
}

int orientationFor(MedicalViewportItem::ViewType type)
{
    if (type == MedicalViewportItem::ViewType::Sagittal)
        return 0;
    if (type == MedicalViewportItem::ViewType::Coronal)
        return 1;
    return 2;
}

void applyTransferFunction(ViewportPipeline *pipeline, double width, double level)
{
    if (!pipeline->color || !pipeline->opacity)
        return;

    const double low = level - width * 0.5;
    const double mid = level;
    const double high = level + width * 0.5;
    pipeline->color->RemoveAllPoints();
    pipeline->color->AddRGBPoint(low, 0.0, 0.0, 0.0);
    pipeline->color->AddRGBPoint(mid, 0.58, 0.52, 0.48);
    pipeline->color->AddRGBPoint(high, 1.0, 0.95, 0.88);
    pipeline->color->AddRGBPoint(high + width, 1.0, 1.0, 1.0);
    pipeline->opacity->RemoveAllPoints();
    pipeline->opacity->AddPoint(low, 0.0);
    pipeline->opacity->AddPoint(mid, 0.03);
    pipeline->opacity->AddPoint(high, 0.38);
    pipeline->opacity->AddPoint(high + width, 0.72);
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
        pipeline->maskActor->GetProperty()->SetLookupTable(lookup);
        pipeline->maskActor->GetProperty()->UseLookupTableScalarRangeOn();
        pipeline->maskActor->SetVisibility(showSegmentation);
        pipeline->renderer->AddViewProp(pipeline->maskActor);
    }
}

void configureVolume(ViewportPipeline *pipeline, bool mip, double cropMinimum,
                     double cropMaximum, double width, double level,
                     bool showSegmentation)
{
    pipeline->volumeMapper = vtkSmartPointer<vtkGPUVolumeRayCastMapper>::New();
    pipeline->volumeMapper->SetInputData(pipeline->image);
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
    applyTransferFunction(pipeline, width, level);
    auto property = vtkSmartPointer<vtkVolumeProperty>::New();
    property->SetColor(pipeline->color);
    property->SetScalarOpacity(pipeline->opacity);
    property->ShadeOn();
    property->SetInterpolationTypeToLinear();
    property->SetAmbient(0.25);
    property->SetDiffuse(0.72);
    property->SetSpecular(0.16);

    pipeline->volumeActor = vtkSmartPointer<vtkVolume>::New();
    pipeline->volumeActor->SetMapper(pipeline->volumeMapper);
    pipeline->volumeActor->SetProperty(property);
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
        pipeline->segmentationActor->GetProperty()->SetColor(0.95, 0.38, 0.08);
        pipeline->segmentationActor->GetProperty()->SetOpacity(0.82);
        pipeline->segmentationActor->GetProperty()->SetAmbient(0.34);
        pipeline->segmentationActor->GetProperty()->SetDiffuse(0.66);
        pipeline->segmentationActor->GetProperty()->SetSpecular(0.12);
        pipeline->segmentationActor->SetVisibility(showSegmentation);
        pipeline->segmentationRenderer->AddActor(pipeline->segmentationActor);
    }
}

void rebuildPipeline(ViewportPipeline *pipeline, vtkRenderWindow *renderWindow,
                     const std::shared_ptr<const VolumeSnapshot> &volume,
                     const std::shared_ptr<const MaskSnapshot> &mask,
                     MedicalViewportItem::ViewType type, double slicePosition,
                     bool mip, bool showSegmentation, double cropMinimum,
                     double cropMaximum, double width, double level)
{
    pipeline->renderer->RemoveAllViewProps();
    pipeline->segmentationRenderer->RemoveAllViewProps();
    pipeline->image = nullptr;
    pipeline->mask = nullptr;
    pipeline->sliceMapper = nullptr;
    pipeline->sliceActor = nullptr;
    pipeline->maskMapper = nullptr;
    pipeline->maskActor = nullptr;
    pipeline->volumeMapper = nullptr;
    pipeline->volumeActor = nullptr;
    pipeline->segmentationActor = nullptr;

    if (!volume || volume->pixels.empty())
        return;

    pipeline->image = vtkImageFromVolume(*volume);
    if (mask && mask->dimensions == volume->dimensions)
        pipeline->mask = vtkImageFromMask(*mask);

    if (type == MedicalViewportItem::ViewType::Volume3D)
        configureVolume(pipeline, mip, cropMinimum, cropMaximum, width, level,
                        showSegmentation);
    else
        configureSlice(pipeline, type, slicePosition, width, level, showSegmentation);

    if (renderWindow->GetInteractor()) {
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
    }
    pipeline->renderer->ResetCamera();
    pipeline->segmentationRenderer->SetActiveCamera(pipeline->renderer->GetActiveCamera());
    if (type != MedicalViewportItem::ViewType::Volume3D)
        pipeline->renderer->GetActiveCamera()->ParallelProjectionOn();
    pipeline->renderer->ResetCameraClippingRange();
    pipeline->segmentationRenderer->ResetCameraClippingRange();
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
    pipeline->segmentationRenderer = vtkSmartPointer<vtkRenderer>::New();
    pipeline->segmentationRenderer->SetLayer(1);
    pipeline->segmentationRenderer->SetBackgroundAlpha(0.0);
    pipeline->segmentationRenderer->InteractiveOff();
    renderWindow->SetNumberOfLayers(2);
    renderWindow->AddRenderer(pipeline->renderer);
    renderWindow->AddRenderer(pipeline->segmentationRenderer);

    const double width = m_controller ? m_controller->windowWidth() : 400.0;
    const double level = m_controller ? m_controller->windowLevel() : 40.0;
    rebuildPipeline(pipeline, renderWindow, m_volume, m_mask, m_viewType,
                    m_slicePosition, m_mip, m_showSegmentation,
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

void MedicalViewportItem::reloadData()
{
    m_volume = m_controller ? m_controller->volumeSnapshot() : nullptr;
    m_mask = m_controller ? m_controller->maskSnapshot() : nullptr;
    const auto volume = m_volume;
    const auto mask = m_mask;
    const auto type = m_viewType;
    const double slice = m_slicePosition;
    const bool mipMode = m_mip;
    const bool segmentation = m_showSegmentation;
    const double cropMin = m_cropMinimum;
    const double cropMax = m_cropMaximum;
    const double width = m_controller ? m_controller->windowWidth() : 400.0;
    const double level = m_controller ? m_controller->windowLevel() : 40.0;
    dispatch_async([volume, mask, type, slice, mipMode, segmentation,
                    cropMin, cropMax, width, level](vtkRenderWindow *window,
                                                   vtkUserData userData) {
        auto *pipeline = ViewportPipeline::SafeDownCast(userData);
        if (pipeline)
            rebuildPipeline(pipeline, window, volume, mask, type, slice, mipMode,
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
    const double cropMin = m_cropMinimum;
    const double cropMax = m_cropMaximum;
    const double width = m_controller ? m_controller->windowWidth() : 400.0;
    const double level = m_controller ? m_controller->windowLevel() : 40.0;
    dispatch_async([orientation, slicePosition, segmentation, mipMode,
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
            applyTransferFunction(pipeline, width, level);
        }
    });
    scheduleRender();
}
