#include "medicalviewportitem.h"

#include "src/markups/markupsmetrics.h"
#include "src/markups/markupspicker.h"

#include <vtkActor.h>
#include <vtkActor2D.h>
#include <vtkArcSource.h>
#include <vtkBillboardTextActor3D.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkColorTransferFunction.h>
#include <vtkCoordinate.h>
#include <vtkFlyingEdges3D.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkImageData.h>
#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkInteractorStyleImage.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkLineSource.h>
#include <vtkLookupTable.h>
#include <vtkLight.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkProperty.h>
#include <vtkProperty2D.h>
#include <vtkPropPicker.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkShortArray.h>
#include <vtkSphereSource.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkTubeFilter.h>
#include <vtkUnsignedCharArray.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include <QStringList>
#include <QVariantMap>

namespace {

// 该对象随 QQuickVTKItem 的渲染上下文创建和销毁，成员只能在 VTK 渲染线程访问。
class ViewportPipeline final : public vtkObject
{
public:
    static ViewportPipeline *New();
    vtkTypeMacro(ViewportPipeline, vtkObject);

    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkMatrix4x4> dataToWorld;
    vtkSmartPointer<vtkMatrix4x4> worldToData;
    vtkSmartPointer<vtkMatrix4x4> renderTransform;
    vtkSmartPointer<vtkImageData> image;
    vtkSmartPointer<vtkImageData> mask;
    // VTK 标量数组只读引用快照内存；管线必须持有所有者直到数组被替换。
    std::shared_ptr<const VolumeSnapshot> imageOwner;
    std::shared_ptr<const MaskSnapshot> maskOwner;
    vtkSmartPointer<vtkImageSliceMapper> sliceMapper;
    vtkSmartPointer<vtkImageSlice> sliceActor;
    vtkSmartPointer<vtkLookupTable> imageLookup;
    vtkSmartPointer<vtkImageSliceMapper> maskMapper;
    vtkSmartPointer<vtkImageSlice> maskActor;
    vtkSmartPointer<vtkLookupTable> maskLookup;
    vtkSmartPointer<vtkGPUVolumeRayCastMapper> volumeMapper;
    vtkSmartPointer<vtkColorTransferFunction> color;
    vtkSmartPointer<vtkPiecewiseFunction> opacity;
    vtkSmartPointer<vtkVolumeProperty> volumeProperty;
    vtkSmartPointer<vtkVolume> volumeActor;
    vtkSmartPointer<vtkActor> segmentationActor;
    std::vector<vtkSmartPointer<vtkProp>> annotationProps;
    MarkupsPicker::ImagePresentation imagePresentation;
    std::array<double, 3> sliceCameraCenter {0.0, 0.0, 0.0};
    bool sliceCameraCenterValid = false;
};

vtkStandardNewMacro(ViewportPipeline);

vtkSmartPointer<vtkImageData> vtkImageFromVolume(const VolumeSnapshot &snapshot)
{
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(snapshot.dimensions.data());
    image->SetSpacing(snapshot.spacing.data());
    image->SetOrigin(0.0, 0.0, 0.0);
    auto scalars = vtkSmartPointer<vtkShortArray>::New();
    scalars->SetNumberOfComponents(1);
    scalars->SetArray(const_cast<short *>(snapshot.pixels.data()),
                      static_cast<vtkIdType>(snapshot.pixels.size()), 1);
    image->GetPointData()->SetScalars(scalars);
    return image;
}

vtkSmartPointer<vtkImageData> vtkImageFromMask(const MaskSnapshot &snapshot)
{
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(snapshot.dimensions.data());
    image->SetSpacing(snapshot.spacing.data());
    image->SetOrigin(0.0, 0.0, 0.0);
    auto scalars = vtkSmartPointer<vtkUnsignedCharArray>::New();
    scalars->SetNumberOfComponents(1);
    scalars->SetArray(const_cast<unsigned char *>(snapshot.pixels.data()),
                      static_cast<vtkIdType>(snapshot.pixels.size()), 1);
    image->GetPointData()->SetScalars(scalars);
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

vtkSmartPointer<vtkMatrix4x4> displayTransformFor(
    const MarkupsPicker::ImagePresentation &presentation)
{
    auto transform = vtkSmartPointer<vtkMatrix4x4>::New();
    transform->Identity();
    transform->SetElement(0, 0, presentation.linear[0]);
    transform->SetElement(0, 1, presentation.linear[1]);
    transform->SetElement(1, 0, presentation.linear[2]);
    transform->SetElement(1, 1, presentation.linear[3]);
    transform->SetElement(0, 3, presentation.offset[0]);
    transform->SetElement(1, 3, presentation.offset[1]);
    return transform;
}

int orientationFor(MedicalViewportItem::ViewType type)
{
    if (type == MedicalViewportItem::ViewType::Sagittal)
        return 0;
    if (type == MedicalViewportItem::ViewType::Coronal)
        return 1;
    return 2;
}

void fitSliceCamera(ViewportPipeline *pipeline, MedicalViewportItem::ViewType type,
                    const VolumeSnapshot &volume, double viewportWidth,
                    double viewportHeight, double zoom, double panX, double panY)
{
    if (!pipeline || !pipeline->renderer
        || type == MedicalViewportItem::ViewType::Volume3D
        || viewportWidth <= 0.0 || viewportHeight <= 0.0)
        return;
    const auto size = MarkupsPicker::sliceViewPhysicalSize(
        volume, static_cast<int>(type), pipeline->imagePresentation);
    const double aspect = viewportWidth / viewportHeight;
    const double visibleHeight = std::max(size[1], size[0] / aspect);
    auto *camera = pipeline->renderer->GetActiveCamera();
    zoom = std::clamp(zoom, 0.25, 20.0);
    camera->SetParallelScale(visibleHeight * 0.5 / zoom);
    if (!pipeline->sliceCameraCenterValid)
        camera->GetFocalPoint(pipeline->sliceCameraCenter.data());

    double direction[3];
    double up[3];
    double right[3];
    camera->GetDirectionOfProjection(direction);
    camera->GetViewUp(up);
    vtkMath::Cross(direction, up, right);
    vtkMath::Normalize(right);
    vtkMath::Normalize(up);
    const double worldPerPixel = 2.0 * camera->GetParallelScale() / viewportHeight;
    double focal[3];
    for (int axis = 0; axis < 3; ++axis) {
        focal[axis] = pipeline->sliceCameraCenter[static_cast<std::size_t>(axis)]
            - panX * worldPerPixel * right[axis]
            + panY * worldPerPixel * up[axis];
    }
    const double distance = std::max(camera->GetDistance(), 1.0);
    camera->SetFocalPoint(focal);
    camera->SetPosition(focal[0] - direction[0] * distance,
                        focal[1] - direction[1] * distance,
                        focal[2] - direction[2] * distance);
    pipeline->sliceCameraCenterValid = true;
}

void applyVolumePreset(ViewportPipeline *pipeline,
                       MedicalViewportItem::VolumePreset preset, bool mip,
                       double windowWidth, double windowLevel)
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

    // 将预设的归一化颜色/不透明度控制点映射到当前窗宽窗位，使二维窗位工具
    // 能像 Slicer 一样实时改变三维可见的组织密度范围。
    const double width = std::max(1.0, windowWidth);
    const double low = windowLevel - width * 0.5;
    const double high = windowLevel + width * 0.5;
    const auto at = [low, width](double position) { return low + width * position; };

    if (mip) {
        pipeline->color->AddRGBPoint(low, 0.0, 0.0, 0.0);
        pipeline->color->AddRGBPoint(high, 1.0, 1.0, 1.0);
        pipeline->opacity->AddPoint(low, 0.0);
        pipeline->opacity->AddPoint(high, 1.0);
        property->ShadeOff();
        property->SetAmbient(1.0);
        return;
    }

    switch (preset) {
    case MedicalViewportItem::VolumePreset::BonePreset:
        pipeline->color->AddRGBPoint(low, 0.0, 0.0, 0.0);
        pipeline->color->AddRGBPoint(at(0.30), 0.45, 0.20, 0.10);
        pipeline->color->AddRGBPoint(at(0.58), 0.76, 0.48, 0.25);
        pipeline->color->AddRGBPoint(at(0.78), 0.92, 0.76, 0.52);
        pipeline->color->AddRGBPoint(high, 1.0, 0.93, 0.80);
        pipeline->opacity->AddPoint(low, 0.0);
        pipeline->opacity->AddPoint(at(0.34), 0.0);
        pipeline->opacity->AddPoint(at(0.50), 0.16);
        pipeline->opacity->AddPoint(high, 0.58);
        property->SetAmbient(0.2);
        property->SetDiffuse(1.0);
        property->SetSpecular(0.0);
        property->SetSpecularPower(1.0);
        break;
    case MedicalViewportItem::VolumePreset::LungPreset:
        pipeline->color->AddRGBPoint(low, 0.12, 0.18, 0.28);
        pipeline->color->AddRGBPoint(at(0.38), 0.20, 0.42, 0.52);
        pipeline->color->AddRGBPoint(at(0.58), 0.56, 0.72, 0.64);
        pipeline->color->AddRGBPoint(at(0.78), 0.92, 0.84, 0.68);
        pipeline->color->AddRGBPoint(high, 1.0, 0.95, 0.90);
        pipeline->opacity->AddPoint(low, 0.0);
        pipeline->opacity->AddPoint(at(0.24), 0.0);
        pipeline->opacity->AddPoint(at(0.42), 0.12);
        pipeline->opacity->AddPoint(at(0.72), 0.22);
        pipeline->opacity->AddPoint(high, 0.05);
        property->SetAmbient(0.42);
        property->SetDiffuse(0.58);
        property->SetSpecular(0.0);
        property->SetSpecularPower(1.0);
        break;
    case MedicalViewportItem::VolumePreset::SoftTissuePreset:
        pipeline->color->AddRGBPoint(low, 0.0, 0.0, 0.0);
        pipeline->color->AddRGBPoint(at(0.30), 0.18, 0.08, 0.06);
        pipeline->color->AddRGBPoint(at(0.55), 0.66, 0.36, 0.28);
        pipeline->color->AddRGBPoint(at(0.78), 0.92, 0.72, 0.62);
        pipeline->color->AddRGBPoint(high, 1.0, 0.95, 0.90);
        pipeline->opacity->AddPoint(low, 0.0);
        pipeline->opacity->AddPoint(at(0.30), 0.0);
        pipeline->opacity->AddPoint(at(0.52), 0.10);
        pipeline->opacity->AddPoint(at(0.78), 0.32);
        pipeline->opacity->AddPoint(high, 0.52);
        property->ShadeOff();
        property->SetAmbient(0.2);
        property->SetDiffuse(1.0);
        property->SetSpecular(0.0);
        property->SetSpecularPower(1.0);
        break;
    case MedicalViewportItem::VolumePreset::ChestContrastPreset:
    default:
        pipeline->color->AddRGBPoint(low, 0.0, 0.0, 0.0);
        pipeline->color->AddRGBPoint(at(0.28), 0.55, 0.25, 0.15);
        pipeline->color->AddRGBPoint(at(0.52), 0.88, 0.60, 0.29);
        pipeline->color->AddRGBPoint(at(0.76), 1.0, 0.94, 0.95);
        pipeline->color->AddRGBPoint(high, 0.83, 0.66, 1.0);
        pipeline->opacity->AddPoint(low, 0.0);
        pipeline->opacity->AddPoint(at(0.30), 0.0);
        pipeline->opacity->AddPoint(at(0.52), 0.22);
        pipeline->opacity->AddPoint(at(0.76), 0.52);
        pipeline->opacity->AddPoint(high, 0.62);
        break;
    }
}

void configureSlice(ViewportPipeline *pipeline, MedicalViewportItem::ViewType type,
                    double slicePosition, double width, double level,
                    bool invertGrayscale, bool showImage, bool showSegmentation,
                    double segmentationOpacity)
{
    const int orientation = orientationFor(type);
    const int sliceCount = pipeline->image->GetDimensions()[orientation];
    const int slice = MarkupsPicker::sliceIndexFromPosition(slicePosition, sliceCount);

    pipeline->sliceMapper = vtkSmartPointer<vtkImageSliceMapper>::New();
    pipeline->sliceMapper->SetInputData(pipeline->image);
    pipeline->sliceMapper->SetOrientation(orientation);
    pipeline->sliceMapper->SetSliceNumber(slice);
    pipeline->sliceActor = vtkSmartPointer<vtkImageSlice>::New();
    pipeline->sliceActor->SetMapper(pipeline->sliceMapper);
    pipeline->sliceActor->SetUserMatrix(pipeline->renderTransform);
    pipeline->sliceActor->GetProperty()->SetColorWindow(width);
    pipeline->sliceActor->GetProperty()->SetColorLevel(level);
    if (invertGrayscale) {
        pipeline->imageLookup = vtkSmartPointer<vtkLookupTable>::New();
        constexpr int lookupSize = 4096;
        pipeline->imageLookup->SetNumberOfTableValues(lookupSize);
        for (int index = 0; index < lookupSize; ++index) {
            const double gray = 1.0 - static_cast<double>(index) / (lookupSize - 1);
            pipeline->imageLookup->SetTableValue(index, gray, gray, gray, 1.0);
        }
        pipeline->imageLookup->Build();
        pipeline->sliceActor->GetProperty()->SetLookupTable(pipeline->imageLookup);
        pipeline->sliceActor->GetProperty()->UseLookupTableScalarRangeOff();
    }
    pipeline->sliceActor->GetProperty()->SetInterpolationTypeToLinear();
    pipeline->sliceActor->SetVisibility(showImage);
    pipeline->renderer->AddViewProp(pipeline->sliceActor);

    if (pipeline->mask) {
        pipeline->maskLookup = vtkSmartPointer<vtkLookupTable>::New();
        pipeline->maskLookup->SetNumberOfTableValues(2);
        pipeline->maskLookup->SetRange(0.0, 1.0);
        pipeline->maskLookup->SetTableValue(0, 0.0, 0.0, 0.0, 0.0);
        pipeline->maskLookup->SetTableValue(1, 0.95, 0.38, 0.08, segmentationOpacity);
        pipeline->maskLookup->Build();
        pipeline->maskMapper = vtkSmartPointer<vtkImageSliceMapper>::New();
        pipeline->maskMapper->SetInputData(pipeline->mask);
        pipeline->maskMapper->SetOrientation(orientation);
        pipeline->maskMapper->SetSliceNumber(slice);
        pipeline->maskActor = vtkSmartPointer<vtkImageSlice>::New();
        pipeline->maskActor->SetMapper(pipeline->maskMapper);
        pipeline->maskActor->SetUserMatrix(pipeline->renderTransform);
        pipeline->maskActor->GetProperty()->SetLookupTable(pipeline->maskLookup);
        pipeline->maskActor->GetProperty()->UseLookupTableScalarRangeOn();
        pipeline->maskActor->SetVisibility(showSegmentation);
        pipeline->renderer->AddViewProp(pipeline->maskActor);
    }
}

void configureVolume(ViewportPipeline *pipeline, bool mip, double cropMinimum,
                     double cropMaximum, MedicalViewportItem::VolumePreset preset,
                     bool showImage, bool showSegmentation, double segmentationOpacity,
                     double windowWidth, double windowLevel)
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
    applyVolumePreset(pipeline, preset, mip, windowWidth, windowLevel);

    pipeline->volumeActor = vtkSmartPointer<vtkVolume>::New();
    pipeline->volumeActor->SetMapper(pipeline->volumeMapper);
    pipeline->volumeActor->SetProperty(pipeline->volumeProperty);
    pipeline->volumeActor->SetUserMatrix(pipeline->renderTransform);
    pipeline->volumeActor->SetVisibility(showImage);
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
        pipeline->segmentationActor->SetUserMatrix(pipeline->renderTransform);
        pipeline->segmentationActor->GetProperty()->SetColor(0.95, 0.38, 0.08);
        pipeline->segmentationActor->GetProperty()->SetOpacity(segmentationOpacity);
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
                     bool projectionData, bool invertGrayscale,
                     bool showImage, bool showSegmentation,
                     double segmentationOpacity, int rotationQuarterTurns,
                      bool flipHorizontal, bool flipVertical, double cropMinimum,
                      double cropMaximum, double width, double level,
                      const QString &patientOrientation, double viewZoom,
                      double viewPanX, double viewPanY)
{
    pipeline->renderer->RemoveAllViewProps();
    pipeline->image = nullptr;
    pipeline->mask = nullptr;
    pipeline->imageOwner.reset();
    pipeline->maskOwner.reset();
    pipeline->sliceMapper = nullptr;
    pipeline->sliceActor = nullptr;
    pipeline->imageLookup = nullptr;
    pipeline->maskMapper = nullptr;
    pipeline->maskActor = nullptr;
    pipeline->maskLookup = nullptr;
    pipeline->volumeMapper = nullptr;
    pipeline->volumeProperty = nullptr;
    pipeline->volumeActor = nullptr;
    pipeline->segmentationActor = nullptr;
    pipeline->annotationProps.clear();
    pipeline->sliceCameraCenterValid = false;

    if (!volume || volume->pixels.empty())
        return;

    pipeline->imageOwner = volume;
    pipeline->image = vtkImageFromVolume(*pipeline->imageOwner);
    updatePatientTransform(pipeline, *volume);
    // 显示翻转/旋转只作用于切片视图；3D 必须保持真实 LPS 世界坐标，
    // 否则 vtkVolume(GPU 路径) 与标注 vtkActor 对翻转矩阵应用不一致而错位。
    const bool applyDisplayTransform = projectionData
        && type != MedicalViewportItem::ViewType::Volume3D;
    pipeline->imagePresentation = MarkupsPicker::imagePresentationFor(
        *volume, applyDisplayTransform, patientOrientation, rotationQuarterTurns,
        flipHorizontal, flipVertical);
    pipeline->renderTransform = displayTransformFor(pipeline->imagePresentation);
    auto combined = vtkSmartPointer<vtkMatrix4x4>::New();
    vtkMatrix4x4::Multiply4x4(pipeline->dataToWorld, pipeline->renderTransform, combined);
    pipeline->renderTransform = combined;
    if (mask && mask->dimensions == volume->dimensions) {
        pipeline->maskOwner = mask;
        pipeline->mask = vtkImageFromMask(*pipeline->maskOwner);
    }

    if (type == MedicalViewportItem::ViewType::Volume3D)
        configureVolume(pipeline, mip, cropMinimum, cropMaximum, preset,
                        showImage, showSegmentation, segmentationOpacity, width, level);
    else
        configureSlice(pipeline, type, slicePosition, width, level, invertGrayscale,
                       showImage, showSegmentation, segmentationOpacity);

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
    if (type != MedicalViewportItem::ViewType::Volume3D) {
        camera->GetFocalPoint(pipeline->sliceCameraCenter.data());
        pipeline->sliceCameraCenterValid = true;
        const int *viewportSize = renderWindow->GetSize();
        fitSliceCamera(pipeline, type, *volume,
                       static_cast<double>(viewportSize[0]),
                       static_cast<double>(viewportSize[1]),
                       viewZoom, viewPanX, viewPanY);
    }
    pipeline->renderer->ResetCameraClippingRange();
}

} // namespace

MedicalViewportItem::MedicalViewportItem(QQuickItem *parent)
    : MedicalViewportBase(parent)
{
}

int MedicalViewportItem::sliceCount() const
{
    if (!m_volume || m_viewType == ViewType::Volume3D)
        return 0;
    int axisX = 0;
    int axisY = 1;
    int axisZ = 2;
    MarkupsPicker::viewAxes(static_cast<int>(m_viewType), &axisX, &axisY, &axisZ);
    (void)axisX;
    (void)axisY;
    return std::max(1, m_volume->dimensions[axisZ]);
}

void MedicalViewportItem::geometryChange(const QRectF &newGeometry,
                                         const QRectF &oldGeometry)
{
    MedicalViewportBase::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() == oldGeometry.size() || !m_volume
        || m_viewType == ViewType::Volume3D)
        return;
    const auto volume = m_volume;
    const auto type = m_viewType;
    const double viewportWidth = newGeometry.width();
    const double viewportHeight = newGeometry.height();
    const double viewZoom = m_viewZoom;
    const double viewPanX = m_viewPanX;
    const double viewPanY = m_viewPanY;
    dispatch_async([volume, type, viewportWidth, viewportHeight,
                    viewZoom, viewPanX, viewPanY](
                       vtkRenderWindow *, vtkUserData userData) {
        auto *pipeline = ViewportPipeline::SafeDownCast(userData);
        fitSliceCamera(pipeline, type, *volume, viewportWidth, viewportHeight,
                       viewZoom, viewPanX, viewPanY);
        if (pipeline && pipeline->renderer)
            pipeline->renderer->ResetCameraClippingRange();
    });
    scheduleRender();
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
    const bool projectionData = m_controller && m_controller->projectionData();
    const bool invertGrayscale = m_controller && projectionData
        && (m_pairedProjection ? m_controller->projectionPairInverted()
                               : m_controller->projectionInverted());
    const QString patientOrientation = m_controller
        ? (m_pairedProjection ? m_controller->projectionPairOrientation()
                              : m_controller->patientOrientation())
        : QString();
    rebuildPipeline(pipeline, renderWindow, m_volume, m_mask, m_viewType,
                     m_slicePosition, m_mip, m_volumePreset, projectionData,
                    invertGrayscale, m_showImage, m_showSegmentation,
                     m_segmentationOpacity, m_rotationQuarterTurns,
                     m_flipHorizontal, m_flipVertical, m_cropMinimum,
                     m_cropMaximum, width, level, patientOrientation,
                     m_viewZoom, m_viewPanX, m_viewPanY);
    return pipeline;
}

void MedicalViewportItem::setViewType(ViewType type)
{
    if (m_viewType == type)
        return;
    m_viewType = type;
    m_viewZoom = 1.0;
    m_viewPanX = 0.0;
    m_viewPanY = 0.0;
    emit viewTypeChanged();
    emit sliceCountChanged();
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

void MedicalViewportItem::setAnnotations(AnnotationController *annotations)
{
    if (m_annotations == annotations)
        return;
    if (m_annotations)
        disconnect(m_annotations, nullptr, this, nullptr);
    m_annotations = annotations;
    if (m_annotations) {
        connect(m_annotations, &AnnotationController::annotationsChanged,
                this, &MedicalViewportItem::syncAnnotationActors);
        connect(m_annotations, &AnnotationController::visibleChanged,
                this, &MedicalViewportItem::syncAnnotationActors);
    }
    emit annotationsChanged();
    syncAnnotationActors();
}

void MedicalViewportItem::setShowAnnotations(bool visible)
{
    if (m_showAnnotations == visible)
        return;
    m_showAnnotations = visible;
    emit showAnnotationsChanged();
    syncAnnotationActors();
}

void MedicalViewportItem::setSlicePosition(double position)
{
    position = std::clamp(position, 0.0, 1.0);
    if (qFuzzyCompare(m_slicePosition, position))
        return;
    m_slicePosition = position;
    emit slicePositionChanged();
    updateRenderState();
    syncAnnotationActors();
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

void MedicalViewportItem::setPairedProjection(bool paired)
{
    if (m_pairedProjection == paired)
        return;
    m_pairedProjection = paired;
    emit pairedProjectionChanged();
    reloadData();
}

void MedicalViewportItem::setShowImage(bool visible)
{
    if (m_showImage == visible)
        return;
    m_showImage = visible;
    emit showImageChanged();
    updateRenderState();
}

void MedicalViewportItem::setSegmentationOpacity(double opacity)
{
    opacity = std::clamp(opacity, 0.0, 1.0);
    if (qFuzzyCompare(m_segmentationOpacity, opacity))
        return;
    m_segmentationOpacity = opacity;
    emit segmentationOpacityChanged();
    updateRenderState();
}

void MedicalViewportItem::setRotationQuarterTurns(int turns)
{
    turns = ((turns % 4) + 4) % 4;
    if (m_rotationQuarterTurns == turns)
        return;
    m_rotationQuarterTurns = turns;
    emit orientationChanged();
    reloadData();
}

void MedicalViewportItem::setFlipHorizontal(bool flipped)
{
    if (m_flipHorizontal == flipped)
        return;
    m_flipHorizontal = flipped;
    emit orientationChanged();
    reloadData();
}

void MedicalViewportItem::setFlipVertical(bool flipped)
{
    if (m_flipVertical == flipped)
        return;
    m_flipVertical = flipped;
    emit orientationChanged();
    reloadData();
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

bool MedicalViewportItem::mapItemPositionToWorld(double itemX, double itemY,
                                                 QVector3D *worldOut,
                                                 int *voxelOut) const
{
    if (m_viewType == ViewType::Volume3D || !m_volume || !worldOut
        || width() <= 0.0 || height() <= 0.0
        || itemX < 0.0 || itemY < 0.0 || itemX > width() || itemY > height())
        return false;

    const double unzoomedX = width() * 0.5
        + (itemX - width() * 0.5 - m_viewPanX) / m_viewZoom;
    const double unzoomedY = height() * 0.5
        + (itemY - height() * 0.5 - m_viewPanY) / m_viewZoom;
    const bool projectionData = m_controller && m_controller->projectionData();
    const QString patientOrientation = m_controller
        ? (m_pairedProjection ? m_controller->projectionPairOrientation()
                              : m_controller->patientOrientation())
        : QString();
    const auto presentation = MarkupsPicker::imagePresentationFor(
        *m_volume, projectionData && m_viewType != ViewType::Volume3D,
        patientOrientation, m_rotationQuarterTurns, m_flipHorizontal, m_flipVertical);
    return MarkupsPicker::mapClickToWorld(
        *m_volume, static_cast<int>(m_viewType), m_slicePosition,
        unzoomedX, unzoomedY, width(), height(), worldOut, voxelOut, presentation);
}

bool MedicalViewportItem::mapClickToVoxel(double itemX, double itemY, bool updateSeed)
{
    if (m_viewType == ViewType::Volume3D || !m_controller || !m_volume
        || width() <= 0.0 || height() <= 0.0
        || itemX < 0.0 || itemY < 0.0 || itemX > width() || itemY > height()) {
        emit voxelPickFailed(QStringLiteral("当前视图无法拾取体素。"));
        return false;
    }

    QVector3D world;
    int voxel[3] = {0, 0, 0};
    if (!mapItemPositionToWorld(itemX, itemY, &world, voxel)) {
        emit voxelPickFailed(QStringLiteral("点击位置不在当前切片图像内。"));
        return false;
    }

    const auto &dims = m_volume->dimensions;
    const std::size_t flat = static_cast<std::size_t>(voxel[0])
        + static_cast<std::size_t>(voxel[1]) * static_cast<std::size_t>(dims[0])
        + static_cast<std::size_t>(voxel[2]) * static_cast<std::size_t>(dims[0])
              * static_cast<std::size_t>(dims[1]);
    const int hu = flat < m_volume->pixels.size() ? m_volume->pixels[flat] : 0;
    if (updateSeed && !m_controller->setRegionGrowingSeed(voxel[0], voxel[1], voxel[2])) {
        emit voxelPickFailed(QStringLiteral("无法设置种子点。"));
        return false;
    }

    const double normalizedX = std::clamp(itemX / width(), 0.0, 1.0);
    const double normalizedY = std::clamp(itemY / height(), 0.0, 1.0);
    const int value = updateSeed ? m_controller->regionGrowingSeedValue() : hu;
    emit voxelPicked(voxel[0], voxel[1], voxel[2], value,
                     normalizedX, normalizedY);
    return true;
}

QVariantMap MedicalViewportItem::mapClickToVoxelInfo(double itemX, double itemY) const
{
    QVariantMap result;
    QVector3D world;
    int voxel[3] = {0, 0, 0};
    if (!mapItemPositionToWorld(itemX, itemY, &world, voxel))
        return result;

    result.insert(QStringLiteral("valid"), true);
    result.insert(QStringLiteral("voxelX"), voxel[0]);
    result.insert(QStringLiteral("voxelY"), voxel[1]);
    result.insert(QStringLiteral("voxelZ"), voxel[2]);
    result.insert(QStringLiteral("worldX"), world.x());
    result.insert(QStringLiteral("worldY"), world.y());
    result.insert(QStringLiteral("worldZ"), world.z());
    return result;
}

QVariantMap MedicalViewportItem::mapVoxelToDisplay(int voxelX, int voxelY,
                                                   int voxelZ) const
{
    QVariantMap result;
    if (m_viewType == ViewType::Volume3D || !m_volume
        || width() <= 0.0 || height() <= 0.0)
        return result;

    const auto &dims = m_volume->dimensions;
    if (voxelX < 0 || voxelX >= dims[0]
        || voxelY < 0 || voxelY >= dims[1]
        || voxelZ < 0 || voxelZ >= dims[2])
        return result;

    const QVector3D world = MarkupsPicker::voxelToWorld(
        *m_volume, voxelX, voxelY, voxelZ);
    if (!MarkupsPicker::isPointDisplayableOnSlice(
            *m_volume, static_cast<int>(m_viewType), m_slicePosition, world))
        return result;

    const bool projectionData = m_controller && m_controller->projectionData();
    const QString patientOrientation = m_controller
        ? (m_pairedProjection ? m_controller->projectionPairOrientation()
                              : m_controller->patientOrientation())
        : QString();
    const auto presentation = MarkupsPicker::imagePresentationFor(
        *m_volume, projectionData && m_viewType != ViewType::Volume3D,
        patientOrientation, m_rotationQuarterTurns, m_flipHorizontal, m_flipVertical);
    double displayX = 0.0;
    double displayY = 0.0;
    if (!MarkupsPicker::worldToDisplay(
            *m_volume, static_cast<int>(m_viewType), m_slicePosition,
            width(), height(), world, &displayX, &displayY, presentation))
        return result;

    displayX = width() * 0.5
        + m_viewZoom * (displayX - width() * 0.5) + m_viewPanX;
    displayY = height() * 0.5
        + m_viewZoom * (displayY - height() * 0.5) + m_viewPanY;
    result.insert(QStringLiteral("valid"), true);
    result.insert(QStringLiteral("x"), displayX);
    result.insert(QStringLiteral("y"), displayY);
    return result;
}

bool MedicalViewportItem::beginAnnotationInteraction(double itemX, double itemY,
                                                     double tolerancePx)
{
    if (m_viewType == ViewType::Volume3D || !m_volume || !m_annotations
        || !m_annotations->visible() || !m_showAnnotations
        || width() <= 0.0 || height() <= 0.0
        || itemX < 0.0 || itemY < 0.0 || itemX > width() || itemY > height()) {
        emit voxelPickFailed(QStringLiteral("当前视图无法添加标注点。"));
        return false;
    }
    if (m_annotations->toolType() == AnnotationController::NoneTool) {
        emit voxelPickFailed(QStringLiteral("请先选择标记或测量工具。"));
        return false;
    }

    if (!m_annotations->hasActive()) {
        const QVariantMap hit = hitTestControlPoint(itemX, itemY, tolerancePx);
        if (hit.contains(QStringLiteral("nodeId"))) {
            emit annotationControlPointPressed(
                hit.value(QStringLiteral("nodeId")).toInt(),
                hit.value(QStringLiteral("pointIndex")).toInt());
            return true;
        }
    }

    const bool projectionData = m_controller && m_controller->projectionData();
    QVector3D world;
    if (!mapItemPositionToWorld(itemX, itemY, &world)) {
        emit voxelPickFailed(QStringLiteral("点击位置不在当前切片图像内。"));
        return false;
    }
    const QString annotationViewId = projectionData
        ? (m_pairedProjection ? QStringLiteral("projection-pair")
                              : QStringLiteral("projection-primary"))
        : QString();
    if (!m_annotations->addWorldPointForView(
            world.x(), world.y(), world.z(), annotationViewId)) {
        emit voxelPickFailed(QStringLiteral("无法添加标注点，请重新选择测量工具。"));
        return false;
    }
    return true;
}

bool MedicalViewportItem::updateAnnotationControlPoint(int nodeId, int pointIndex,
                                                       double itemX, double itemY)
{
    if (m_viewType == ViewType::Volume3D || !m_volume || !m_annotations
        || width() <= 0.0 || height() <= 0.0
        || itemX < 0.0 || itemY < 0.0 || itemX > width() || itemY > height())
        return false;

    QVector3D world;
    if (!mapItemPositionToWorld(itemX, itemY, &world))
        return false;

    return m_annotations->updateControlPoint(
        nodeId, pointIndex, world.x(), world.y(), world.z());
}

void MedicalViewportItem::pickVoxel(double itemX, double itemY, bool updateSeed)
{
    mapClickToVoxel(itemX, itemY, updateSeed);
}

QVariantMap MedicalViewportItem::hitTestControlPoint(double itemX, double itemY,
                                                     double tolerancePx)
{
    QVariantMap result;
    if (m_viewType == ViewType::Volume3D || !m_volume || !m_annotations
        || !m_annotations->visible() || !m_showAnnotations)
        return result;

    const QVariantList items = m_annotations->items();
    const bool projectionData = m_controller && m_controller->projectionData();
    const QString patientOrientation = m_controller
        ? (m_pairedProjection ? m_controller->projectionPairOrientation()
                              : m_controller->patientOrientation())
        : QString();
    const auto presentation = MarkupsPicker::imagePresentationFor(
        *m_volume, projectionData && m_viewType != ViewType::Volume3D,
        patientOrientation, m_rotationQuarterTurns, m_flipHorizontal, m_flipVertical);
    double bestDist2 = tolerancePx * tolerancePx;
    int bestNodeId = -1;
    int bestPointIndex = -1;

    for (const QVariant &entry : items) {
        const QVariantMap item = entry.toMap();
        if (!item.value(QStringLiteral("visible"), true).toBool())
            continue;
        const QString itemViewId = item.value(QStringLiteral("viewId")).toString();
        const QString viewportViewId = projectionData
            ? (m_pairedProjection ? QStringLiteral("projection-pair")
                                  : QStringLiteral("projection-primary"))
            : QString();
        if (!viewportViewId.isEmpty() && !itemViewId.isEmpty()
            && itemViewId != viewportViewId)
            continue;
        const int nodeId = item.value(QStringLiteral("id")).toInt();
        const QVariantList points = item.value(QStringLiteral("points")).toList();
        for (int pointIndex = 0; pointIndex < points.size(); ++pointIndex) {
            const QVariantMap point = points.at(pointIndex).toMap();
            const QVector3D world(point.value(QStringLiteral("x")).toFloat(),
                                 point.value(QStringLiteral("y")).toFloat(),
                                 point.value(QStringLiteral("z")).toFloat());
            if (!MarkupsPicker::isPointDisplayableOnSlice(
                    *m_volume, static_cast<int>(m_viewType), m_slicePosition, world))
                continue;
            double dx = 0.0;
            double dy = 0.0;
            if (!MarkupsPicker::worldToDisplay(*m_volume, static_cast<int>(m_viewType),
                                               m_slicePosition, width(), height(),
                                               world, &dx, &dy, presentation))
                continue;
            dx = width() * 0.5 + m_viewZoom * (dx - width() * 0.5) + m_viewPanX;
            dy = height() * 0.5 + m_viewZoom * (dy - height() * 0.5) + m_viewPanY;
            const double dist2 = (dx - itemX) * (dx - itemX) + (dy - itemY) * (dy - itemY);
            if (dist2 <= bestDist2) {
                bestDist2 = dist2;
                bestNodeId = nodeId;
                bestPointIndex = pointIndex;
            }
        }
    }

    if (bestNodeId < 0)
        return result;
    result.insert(QStringLiteral("nodeId"), bestNodeId);
    result.insert(QStringLiteral("pointIndex"), bestPointIndex);
    return result;
}

void MedicalViewportItem::panBy(double deltaX, double deltaY)
{
    if (m_viewType == ViewType::Volume3D || !m_volume
        || !std::isfinite(deltaX) || !std::isfinite(deltaY))
        return;
    m_viewPanX += deltaX;
    m_viewPanY += deltaY;
    const auto volume = m_volume;
    const auto type = m_viewType;
    const double viewportWidth = width();
    const double viewportHeight = height();
    const double viewZoom = m_viewZoom;
    const double viewPanX = m_viewPanX;
    const double viewPanY = m_viewPanY;
    dispatch_async([volume, type, viewportWidth, viewportHeight,
                    viewZoom, viewPanX, viewPanY](vtkRenderWindow *, vtkUserData userData) {
        auto *pipeline = ViewportPipeline::SafeDownCast(userData);
        fitSliceCamera(pipeline, type, *volume, viewportWidth, viewportHeight,
                       viewZoom, viewPanX, viewPanY);
        if (pipeline && pipeline->renderer)
            pipeline->renderer->ResetCameraClippingRange();
    });
    scheduleRender();
}

void MedicalViewportItem::zoomBy(double factor, double anchorX, double anchorY)
{
    if (m_viewType == ViewType::Volume3D || !m_volume
        || !std::isfinite(factor) || factor <= 0.0)
        return;
    const double oldZoom = m_viewZoom;
    const double newZoom = std::clamp(oldZoom * factor, 0.25, 20.0);
    if (qFuzzyCompare(oldZoom, newZoom))
        return;
    const double ratio = newZoom / oldZoom;
    const double centerX = width() * 0.5;
    const double centerY = height() * 0.5;
    m_viewPanX = (1.0 - ratio) * (anchorX - centerX) + ratio * m_viewPanX;
    m_viewPanY = (1.0 - ratio) * (anchorY - centerY) + ratio * m_viewPanY;
    m_viewZoom = newZoom;

    const auto volume = m_volume;
    const auto type = m_viewType;
    const double viewportWidth = width();
    const double viewportHeight = height();
    const double viewZoom = m_viewZoom;
    const double viewPanX = m_viewPanX;
    const double viewPanY = m_viewPanY;
    dispatch_async([volume, type, viewportWidth, viewportHeight,
                    viewZoom, viewPanX, viewPanY](vtkRenderWindow *, vtkUserData userData) {
        auto *pipeline = ViewportPipeline::SafeDownCast(userData);
        fitSliceCamera(pipeline, type, *volume, viewportWidth, viewportHeight,
                       viewZoom, viewPanX, viewPanY);
        if (pipeline && pipeline->renderer)
            pipeline->renderer->ResetCameraClippingRange();
    });
    scheduleRender();
}

void MedicalViewportItem::resetView()
{
    if (m_viewType == ViewType::Volume3D || !m_volume)
        return;
    m_viewZoom = 1.0;
    m_viewPanX = 0.0;
    m_viewPanY = 0.0;
    const auto volume = m_volume;
    const auto type = m_viewType;
    const double viewportWidth = width();
    const double viewportHeight = height();
    dispatch_async([volume, type, viewportWidth, viewportHeight](
                       vtkRenderWindow *, vtkUserData userData) {
        auto *pipeline = ViewportPipeline::SafeDownCast(userData);
        fitSliceCamera(pipeline, type, *volume, viewportWidth, viewportHeight,
                       1.0, 0.0, 0.0);
        if (pipeline && pipeline->renderer)
            pipeline->renderer->ResetCameraClippingRange();
    });
    scheduleRender();
}

void MedicalViewportItem::syncAnnotationActors()
{
    const QVariantList items = (m_annotations && m_showAnnotations)
        ? m_annotations->renderItems()
        : QVariantList {};
    const auto volume = m_volume;
    const bool show = m_showAnnotations && m_annotations && m_annotations->visible();
    const int viewType = static_cast<int>(m_viewType);
    const double slicePosition = m_slicePosition;
    const bool is3d = m_viewType == ViewType::Volume3D;
    const QString annotationViewId = (m_controller && m_controller->projectionData())
        ? (m_pairedProjection ? QStringLiteral("projection-pair")
                              : QStringLiteral("projection-primary"))
        : QString();

    dispatch_async([items, volume, show, viewType, slicePosition, is3d,
                    annotationViewId](
                       vtkRenderWindow *, vtkUserData userData) {
        auto *pipeline = ViewportPipeline::SafeDownCast(userData);
        if (!pipeline || !pipeline->renderer)
            return;

        for (const auto &prop : pipeline->annotationProps)
            pipeline->renderer->RemoveViewProp(prop);
        pipeline->annotationProps.clear();

        if (!show || !volume || items.isEmpty() || !pipeline->renderTransform)
            return;

        const double spacingMax = std::max({volume->spacing[0], volume->spacing[1],
                                            volume->spacing[2]});
        int axisX = 0;
        int axisY = 1;
        int axisZ = 2;
        MarkupsPicker::viewAxes(viewType, &axisX, &axisY, &axisZ);
        const double inPlaneSpacing = std::min(volume->spacing[axisX],
                                               volume->spacing[axisY]);
        const double radius = is3d
            ? std::clamp(spacingMax * 1.4, 2.0, 5.0)
            : std::clamp(inPlaneSpacing * 4.0, 1.5, 4.0);
        const double liftOffset = spacingMax * 0.6;

        auto toRenderPoint = [&](const QVector3D &world) -> std::array<double, 3> {
            if (is3d)
                return {static_cast<double>(world.x()), static_cast<double>(world.y()),
                        static_cast<double>(world.z())};
            auto p = MarkupsPicker::worldToSliceImagePhysical(
                *volume, viewType, slicePosition, world);
            const double in[4] = {p[0], p[1], p[2], 1.0};
            double out[4] = {0.0, 0.0, 0.0, 1.0};
            pipeline->renderTransform->MultiplyPoint(in, out);
            const double w = (out[3] != 0.0) ? out[3] : 1.0;
            std::array<double, 3> r {out[0] / w, out[1] / w, out[2] / w};
            // 抬升必须在渲染(世界)空间沿“朝相机”方向，与体数据 direction 无关。
            // 若像旧代码那样在图像坐标系按固定轴抬升，当切片轴方向为负
            // (direction 对角元为 -1，如倒序存储的 DICOM) 时，抬升会背对相机，
            // 把细线和平面标签压到切片背面而被图像挡住。
            // 相机方向见 rebuildPipeline：轴向 +Z、冠状 -Y、矢状 +X。
            if (viewType == 1)      r[1] -= liftOffset;   // Coronal：相机在 -Y
            else if (viewType == 2) r[0] += liftOffset;   // Sagittal：相机在 +X
            else                    r[2] += liftOffset;   // Axial：相机在 +Z
            return r;
        };

        auto styleActor = [&](vtkActor *actor, double opacity) {
            auto *prop = actor->GetProperty();
            prop->SetColor(0.898, 0.224, 0.208);
            prop->SetOpacity(opacity);
            prop->SetLighting(false);
            prop->SetAmbient(1.0);
            prop->SetDiffuse(0.0);
            prop->SetSpecular(0.0);
            actor->SetForceOpaque(opacity >= 0.9);
        };

        auto addSphere = [&](const std::array<double, 3> &p, double opacity) {
            vtkNew<vtkSphereSource> sphere;
            sphere->SetCenter(p[0], p[1], p[2]);
            sphere->SetRadius(radius);
            sphere->SetThetaResolution(20);
            sphere->SetPhiResolution(20);
            sphere->Update();
            vtkNew<vtkPolyDataMapper> mapper;
            mapper->SetInputData(sphere->GetOutput());
            mapper->ScalarVisibilityOff();
            auto actor = vtkSmartPointer<vtkActor>::New();
            actor->SetMapper(mapper);
            styleActor(actor, opacity);
            pipeline->renderer->AddActor(actor);
            pipeline->annotationProps.push_back(actor);
        };

        // 线段用 vtkLineSource + vtkPolyDataMapper（不经 TubeFilter），
        // SetLineWidth 为屏幕像素宽度（OpenGL glLineWidth），不随缩放变化，
        // 永远可见；避免世界半径 tube 在正常缩放下降到亚像素而消失。
        auto addLine = [&](const std::array<double, 3> &a,
                           const std::array<double, 3> &b, double opacity) {
            vtkNew<vtkLineSource> line;
            line->SetPoint1(a[0], a[1], a[2]);
            line->SetPoint2(b[0], b[1], b[2]);
            line->Update();
            vtkNew<vtkPolyDataMapper> mapper;
            mapper->SetInputData(line->GetOutput());
            mapper->ScalarVisibilityOff();
            auto actor = vtkSmartPointer<vtkActor>::New();
            actor->SetMapper(mapper);
            styleActor(actor, opacity);
            actor->GetProperty()->SetLineWidth(2.0);
            pipeline->renderer->AddActor(actor);
            pipeline->annotationProps.push_back(actor);
        };

        auto addPolyline = [&](const std::vector<std::array<double, 3>> &points,
                               bool closed, double opacity) {
            if (points.size() < 2 || opacity <= 0.0)
                return;
            vtkNew<vtkPoints> vtkPointsData;
            for (const auto &point : points)
                vtkPointsData->InsertNextPoint(point[0], point[1], point[2]);
            vtkNew<vtkCellArray> lines;
            const vtkIdType cellSize = static_cast<vtkIdType>(points.size())
                + (closed ? 1 : 0);
            lines->InsertNextCell(cellSize);
            for (vtkIdType index = 0; index < static_cast<vtkIdType>(points.size()); ++index)
                lines->InsertCellPoint(index);
            if (closed)
                lines->InsertCellPoint(0);
            vtkNew<vtkPolyData> polyline;
            polyline->SetPoints(vtkPointsData);
            polyline->SetLines(lines);
            vtkNew<vtkPolyDataMapper> mapper;
            mapper->SetInputData(polyline);
            mapper->ScalarVisibilityOff();
            auto actor = vtkSmartPointer<vtkActor>::New();
            actor->SetMapper(mapper);
            styleActor(actor, opacity);
            actor->GetProperty()->SetLineWidth(2.0);
            pipeline->renderer->AddActor(actor);
            pipeline->annotationProps.push_back(actor);
        };

        // 角度标注的顶点弧线：在顶点处沿角平面画一段圆弧，连接两腿。
        // 半径取较短腿的 30%，弧靠近顶点；法线 = dA×dB 决定角平面，
        // 2D 抬升后两腿共面正对相机，3D 给真实角平面。
        auto addArc = [&](const std::array<double, 3> &vertex,
                          const std::array<double, 3> &endA,
                          const std::array<double, 3> &endB,
                          double opacity) {
            double dA[3] = {endA[0] - vertex[0], endA[1] - vertex[1], endA[2] - vertex[2]};
            double dB[3] = {endB[0] - vertex[0], endB[1] - vertex[1], endB[2] - vertex[2]};
            const double lenA = std::sqrt(dA[0] * dA[0] + dA[1] * dA[1] + dA[2] * dA[2]);
            const double lenB = std::sqrt(dB[0] * dB[0] + dB[1] * dB[1] + dB[2] * dB[2]);
            if (lenA < 1e-9 || lenB < 1e-9)
                return;
            for (int i = 0; i < 3; ++i) {
                dA[i] /= lenA;
                dB[i] /= lenB;
            }
            double normal[3] = {dA[1] * dB[2] - dA[2] * dB[1],
                                dA[2] * dB[0] - dA[0] * dB[2],
                                dA[0] * dB[1] - dA[1] * dB[0]};
            const double nLen = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1]
                                          + normal[2] * normal[2]);
            if (nLen < 1e-9)
                return;  // 三点共线
            for (int i = 0; i < 3; ++i)
                normal[i] /= nLen;
            const double dot = std::clamp(dA[0] * dB[0] + dA[1] * dB[1] + dA[2] * dB[2],
                                         -1.0, 1.0);
            const double angleDeg = std::acos(dot) * 180.0 / M_PI;
            const double radius = std::min(lenA, lenB) * 0.30;
            vtkNew<vtkArcSource> arc;
            arc->SetUseNormalAndAngle(true);
            arc->SetCenter(vertex[0], vertex[1], vertex[2]);
            arc->SetNormal(normal[0], normal[1], normal[2]);
            arc->SetPolarVector(dA[0] * radius, dA[1] * radius, dA[2] * radius);
            arc->SetAngle(angleDeg);
            arc->SetResolution(24);
            arc->Update();
            vtkNew<vtkPolyDataMapper> mapper;
            mapper->SetInputData(arc->GetOutput());
            mapper->ScalarVisibilityOff();
            auto actor = vtkSmartPointer<vtkActor>::New();
            actor->SetMapper(mapper);
            styleActor(actor, opacity);
            actor->GetProperty()->SetLineWidth(2.0);
            pipeline->renderer->AddActor(actor);
            pipeline->annotationProps.push_back(actor);
        };

        // 标签锚点用点的精确世界坐标，偏移改用显示像素，文字像素对齐，
        // 缩放时只在屏幕平移、不重合不跑偏。
        auto addLabel = [&](const std::array<double, 3> &p, const QString &text,
                            double opacity) {
            if (opacity < 0.40 || text.isEmpty())
                return;
            auto label = vtkSmartPointer<vtkBillboardTextActor3D>::New();
            label->SetInput(text.toUtf8().constData());
            label->SetPosition(p[0], p[1], p[2]);
            // Keep the label close enough that the glyph center remains the obvious
            // placement location, while still avoiding direct text/glyph overlap.
            label->SetDisplayOffset(7, 7);
            label->SetForceOpaque(true);
            auto *tp = label->GetTextProperty();
            tp->SetFontSize(is3d ? 16 : 15);
            tp->SetColor(0.898, 0.224, 0.208);
            tp->SetBold(true);
            tp->SetOpacity(std::max(0.85, opacity));
            tp->SetShadow(true);
            tp->SetBackgroundOpacity(0.35);
            tp->SetBackgroundColor(0.02, 0.03, 0.04);
            pipeline->renderer->AddViewProp(label);
            pipeline->annotationProps.push_back(label);
        };

        for (const QVariant &entry : items) {
            const QVariantMap item = entry.toMap();
            if (!item.value(QStringLiteral("visible"), true).toBool())
                continue;
            const QString itemViewId = item.value(QStringLiteral("viewId")).toString();
            if (!annotationViewId.isEmpty() && !itemViewId.isEmpty()
                && itemViewId != annotationViewId)
                continue;
            const QVariantList points = item.value(QStringLiteral("points")).toList();
            if (points.isEmpty())
                continue;
            const int type = item.value(QStringLiteral("type")).toInt();
            const bool closed = item.value(QStringLiteral("closed"), false).toBool();
            const QString displayText = item.value(QStringLiteral("displayText")).toString();
            const QString labelText = item.value(QStringLiteral("label")).toString();

            std::vector<QVector3D> worldPoints;
            worldPoints.reserve(static_cast<std::size_t>(points.size()));

            for (const QVariant &pointEntry : points) {
                const QVariantMap point = pointEntry.toMap();
                const QVector3D world(point.value(QStringLiteral("x")).toFloat(),
                                     point.value(QStringLiteral("y")).toFloat(),
                                     point.value(QStringLiteral("z")).toFloat());
                worldPoints.push_back(world);
            }

            if (is3d) {
                std::vector<std::array<double, 3>> render;
                render.reserve(worldPoints.size());
                for (const QVector3D &world : worldPoints) {
                    render.push_back(toRenderPoint(world));
                    addSphere(render.back(), 1.0);
                }
                if (type == 0) {
                    for (std::size_t index = 0; index < render.size(); ++index) {
                        const QString pointLabel = render.size() == 1
                            ? labelText
                            : QStringLiteral("%1-%2").arg(labelText).arg(index + 1);
                        addLabel(render[index], pointLabel, 1.0);
                    }
                } else if (type == 1 && render.size() >= 2) {
                    addLine(render[0], render[1], 1.0);
                    addLabel({(render[0][0] + render[1][0]) * 0.5,
                              (render[0][1] + render[1][1]) * 0.5,
                              (render[0][2] + render[1][2]) * 0.5}, displayText, 1.0);
                } else if (type == 2 && render.size() >= 2) {
                    addLine(render[0], render[1], 1.0);
                    if (render.size() >= 3) {
                        addLine(render[1], render[2], 1.0);
                        addArc(render[1], render[0], render[2], 1.0);
                        addLabel(render[1], displayText, 1.0);
                    }
                } else if (type == 3 && render.size() >= 2) {
                    const auto samples = MarkupsMetrics::curveSamples(worldPoints, closed);
                    std::vector<std::array<double, 3>> curveRender;
                    curveRender.reserve(samples.size());
                    for (const QVector3D &sample : samples)
                        curveRender.push_back(toRenderPoint(sample));
                    addPolyline(curveRender, closed, 1.0);
                    addLabel(curveRender[curveRender.size() / 2], displayText, 1.0);
                }
                continue;
            }

            std::vector<bool> pointVisible;
            std::vector<QVector3D> shownGlyphs;
            pointVisible.reserve(worldPoints.size());
            for (const QVector3D &world : worldPoints) {
                const bool visibleOnSlice = MarkupsPicker::isPointDisplayableOnSlice(
                    *volume, viewType, slicePosition, world);
                pointVisible.push_back(visibleOnSlice);
                if (visibleOnSlice) {
                    addSphere(toRenderPoint(world), 1.0);
                    shownGlyphs.push_back(world);
                }
            }

            auto addIntersection = [&](const QVector3D &world) {
                constexpr float duplicateToleranceSquared = 1e-6f;
                for (const QVector3D &shown : shownGlyphs) {
                    if ((shown - world).lengthSquared() <= duplicateToleranceSquared)
                        return;
                }
                addSphere(toRenderPoint(world), 1.0);
                shownGlyphs.push_back(world);
            };
            auto addSegmentIntersection = [&](const QVector3D &a, const QVector3D &b) {
                QVector3D intersection;
                if (MarkupsPicker::segmentSlicePlaneIntersection(
                        *volume, viewType, slicePosition, a, b, &intersection))
                    addIntersection(intersection);
            };
            auto renderVisibleRuns = [&](const std::vector<QVector3D> &polyline,
                                         bool polylineClosed) {
                const auto runs = MarkupsPicker::clipPolylineToSliceSlab(
                    *volume, viewType, slicePosition, polyline, polylineClosed);
                for (const auto &run : runs) {
                    std::vector<std::array<double, 3>> renderRun;
                    renderRun.reserve(run.size());
                    for (const QVector3D &world : run)
                        renderRun.push_back(toRenderPoint(world));
                    addPolyline(renderRun, false, 1.0);
                }
                return runs;
            };

            if (type == 0) {
                for (std::size_t index = 0; index < worldPoints.size(); ++index) {
                    if (!pointVisible[index])
                        continue;
                    const QString pointLabel = worldPoints.size() == 1
                        ? labelText
                        : QStringLiteral("%1-%2").arg(labelText).arg(index + 1);
                    addLabel(toRenderPoint(worldPoints[index]), pointLabel, 1.0);
                }
            } else if (type == 1 && worldPoints.size() >= 2) {
                const auto runs = renderVisibleRuns({worldPoints[0], worldPoints[1]}, false);
                addSegmentIntersection(worldPoints[0], worldPoints[1]);
                if (!runs.empty() && pointVisible[0] && pointVisible[1]) {
                    addLabel(toRenderPoint((worldPoints[0] + worldPoints[1]) * 0.5f),
                             displayText, 1.0);
                }
            } else if (type == 2 && worldPoints.size() >= 2) {
                renderVisibleRuns({worldPoints[0], worldPoints[1]}, false);
                addSegmentIntersection(worldPoints[0], worldPoints[1]);
                if (worldPoints.size() >= 3) {
                    renderVisibleRuns({worldPoints[1], worldPoints[2]}, false);
                    addSegmentIntersection(worldPoints[1], worldPoints[2]);
                    if (pointVisible[0] && pointVisible[1] && pointVisible[2]) {
                        const auto a = toRenderPoint(worldPoints[0]);
                        const auto vertex = toRenderPoint(worldPoints[1]);
                        const auto b = toRenderPoint(worldPoints[2]);
                        addArc(vertex, a, b, 1.0);
                        addLabel(vertex, displayText, 1.0);
                    }
                }
            } else if (type == 3 && worldPoints.size() >= 2) {
                const auto samples = MarkupsMetrics::curveSamples(worldPoints, closed);
                const auto runs = renderVisibleRuns(samples, closed);
                const std::size_t segmentCount = closed ? samples.size() : samples.size() - 1;
                for (std::size_t index = 0; index < segmentCount; ++index) {
                    addSegmentIntersection(samples[index],
                                           samples[(index + 1) % samples.size()]);
                }
                const bool anyControlPointVisible = std::any_of(
                    pointVisible.cbegin(), pointVisible.cend(), [](bool value) { return value; });
                if (!runs.empty() && anyControlPointVisible) {
                    const auto longest = std::max_element(
                        runs.cbegin(), runs.cend(), [](const auto &a, const auto &b) {
                            return a.size() < b.size();
                        });
                    addLabel(toRenderPoint((*longest)[longest->size() / 2]), displayText, 1.0);
                }
            }
        }

        pipeline->renderer->ResetCameraClippingRange();
    });
    scheduleRender();
}

void MedicalViewportItem::reloadData()
{
    const auto nextVolume = m_controller
        ? (m_pairedProjection ? m_controller->projectionPairSnapshot()
                              : m_controller->volumeSnapshot())
        : nullptr;
    if (nextVolume.get() != m_volume.get()) {
        m_viewZoom = 1.0;
        m_viewPanX = 0.0;
        m_viewPanY = 0.0;
    }
    m_volume = nextVolume;
    m_mask = (m_controller && !m_pairedProjection)
        ? m_controller->maskSnapshot() : nullptr;
    emit sliceCountChanged();
    const auto volume = m_volume;
    const auto mask = m_mask;
    const auto type = m_viewType;
    const double slice = m_slicePosition;
    const bool mipMode = m_mip;
    const auto preset = m_volumePreset;
    const bool projectionData = m_controller && m_controller->projectionData();
    const bool invertGrayscale = m_controller && projectionData
        && (m_pairedProjection ? m_controller->projectionPairInverted()
                               : m_controller->projectionInverted());
    const bool showImage = m_showImage;
    const bool segmentation = m_showSegmentation;
    const double segmentationOpacity = m_segmentationOpacity;
    const int rotationQuarterTurns = m_rotationQuarterTurns;
    const bool flipHorizontal = m_flipHorizontal;
    const bool flipVertical = m_flipVertical;
    const double cropMin = m_cropMinimum;
    const double cropMax = m_cropMaximum;
    const double width = m_controller ? m_controller->windowWidth() : 400.0;
    const double level = m_controller ? m_controller->windowLevel() : 40.0;
    const double viewZoom = m_viewZoom;
    const double viewPanX = m_viewPanX;
    const double viewPanY = m_viewPanY;
    const QString patientOrientation = m_controller
        ? (m_pairedProjection ? m_controller->projectionPairOrientation()
                              : m_controller->patientOrientation())
        : QString();
    // Lambda 只捕获不可变快照和值类型，避免渲染线程读取 GUI 对象的可变成员。
    dispatch_async([volume, mask, type, slice, mipMode, preset, projectionData,
                    invertGrayscale,
                    showImage, segmentation, segmentationOpacity,
                    rotationQuarterTurns, flipHorizontal, flipVertical,
                    cropMin, cropMax, width, level, patientOrientation,
                    viewZoom, viewPanX, viewPanY](vtkRenderWindow *window,
                                                   vtkUserData userData) {
        auto *pipeline = ViewportPipeline::SafeDownCast(userData);
        if (pipeline)
            rebuildPipeline(pipeline, window, volume, mask, type, slice, mipMode, preset,
                            projectionData, invertGrayscale, showImage, segmentation,
                            segmentationOpacity,
                            rotationQuarterTurns, flipHorizontal, flipVertical,
                            cropMin, cropMax, width, level, patientOrientation,
                            viewZoom, viewPanX, viewPanY);
    });
    syncAnnotationActors();
    scheduleRender();
}

void MedicalViewportItem::updateRenderState()
{
    const int orientation = orientationFor(m_viewType);
    const double slicePosition = m_slicePosition;
    const bool showImage = m_showImage;
    const bool segmentation = m_showSegmentation;
    const double segmentationOpacity = m_segmentationOpacity;
    const bool mipMode = m_mip;
    const auto preset = m_volumePreset;
    const double cropMin = m_cropMinimum;
    const double cropMax = m_cropMaximum;
    const double width = m_controller ? m_controller->windowWidth() : 400.0;
    const double level = m_controller ? m_controller->windowLevel() : 40.0;
    dispatch_async([orientation, slicePosition, showImage, segmentation,
                    segmentationOpacity, mipMode, preset, cropMin, cropMax,
                    width, level](vtkRenderWindow *, vtkUserData userData) {
        auto *pipeline = ViewportPipeline::SafeDownCast(userData);
        if (!pipeline)
            return;
        if (pipeline->sliceMapper && pipeline->image) {
            const int count = pipeline->image->GetDimensions()[orientation];
            const int slice = MarkupsPicker::sliceIndexFromPosition(slicePosition, count);
            pipeline->sliceMapper->SetSliceNumber(slice);
            pipeline->sliceActor->SetVisibility(showImage);
            pipeline->sliceActor->GetProperty()->SetColorWindow(width);
            pipeline->sliceActor->GetProperty()->SetColorLevel(level);
            if (pipeline->maskMapper)
                pipeline->maskMapper->SetSliceNumber(slice);
        }
        if (pipeline->maskActor)
            pipeline->maskActor->SetVisibility(segmentation);
        if (pipeline->maskLookup) {
            pipeline->maskLookup->SetTableValue(1, 0.95, 0.38, 0.08,
                                                 segmentationOpacity);
            pipeline->maskLookup->Build();
        }
        if (pipeline->segmentationActor)
            pipeline->segmentationActor->SetVisibility(segmentation);
        if (pipeline->segmentationActor)
            pipeline->segmentationActor->GetProperty()->SetOpacity(segmentationOpacity);
        if (pipeline->volumeActor)
            pipeline->volumeActor->SetVisibility(showImage);
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
            applyVolumePreset(pipeline, preset, mipMode, width, level);
        }
        if (pipeline->renderer)
            pipeline->renderer->ResetCameraClippingRange();
    });
    scheduleRender();
}
