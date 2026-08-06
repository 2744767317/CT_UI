#include "medicalviewportitem.h"

#include "src/markups/markupspicker.h"

#include <vtkActor.h>
#include <vtkActor2D.h>
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
#include <vtkSphereSource.h>
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include <vtkTubeFilter.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include <QMetaObject>
#include <QPointer>
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
    vtkSmartPointer<vtkMatrix4x4> worldToRenderData;
    vtkSmartPointer<vtkImageData> image;
    vtkSmartPointer<vtkImageData> mask;
    vtkSmartPointer<vtkImageSliceMapper> sliceMapper;
    vtkSmartPointer<vtkImageSlice> sliceActor;
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

int automaticProjectionQuarterTurns(const QString &orientation)
{
    const QStringList axes = orientation.split(QChar(u'\\'), Qt::SkipEmptyParts);
    if (axes.size() < 2)
        return 0;
    const QString first = axes.at(0).trimmed().toUpper();
    const QString second = axes.at(1).trimmed().toUpper();
    if (second == QStringLiteral("F"))
        return 0;
    if (second == QStringLiteral("H"))
        return 2;
    if (first == QStringLiteral("H"))
        return 1;
    if (first == QStringLiteral("F"))
        return 3;
    return 0;
}

vtkSmartPointer<vtkMatrix4x4> displayTransformFor(const VolumeSnapshot &snapshot,
                                                  bool projection,
                                                  const QString &patientOrientation,
                                                  int rotationQuarterTurns,
                                                  bool flipHorizontal,
                                                  bool flipVertical)
{
    auto transform = vtkSmartPointer<vtkMatrix4x4>::New();
    transform->Identity();
    if (!projection)
        return transform;

    const double width = std::max(0, snapshot.dimensions[0] - 1) * snapshot.spacing[0];
    const double height = std::max(0, snapshot.dimensions[1] - 1) * snapshot.spacing[1];
    const double centerX = width * 0.5;
    const double centerY = height * 0.5;

    auto preMultiply = [&transform](vtkMatrix4x4 *operation) {
        auto combined = vtkSmartPointer<vtkMatrix4x4>::New();
        vtkMatrix4x4::Multiply4x4(operation, transform, combined);
        transform = combined;
    };
    auto centeredOperation = [centerX, centerY](double a00, double a01,
                                                 double a10, double a11) {
        auto operation = vtkSmartPointer<vtkMatrix4x4>::New();
        operation->Identity();
        operation->SetElement(0, 0, a00);
        operation->SetElement(0, 1, a01);
        operation->SetElement(1, 0, a10);
        operation->SetElement(1, 1, a11);
        operation->SetElement(0, 3, centerX - a00 * centerX - a01 * centerY);
        operation->SetElement(1, 3, centerY - a10 * centerX - a11 * centerY);
        return operation;
    };

    // DICOM pixel row zero is the top row; VTK image coordinates grow upward.
    preMultiply(centeredOperation(1.0, 0.0, 0.0, -1.0));

    const int automaticTurns = automaticProjectionQuarterTurns(patientOrientation);
    const int turns = ((automaticTurns + rotationQuarterTurns) % 4 + 4) % 4;
    if (turns != 0) {
        constexpr double halfPi = 1.5707963267948966;
        const double angle = halfPi * turns;
        preMultiply(centeredOperation(std::cos(angle), -std::sin(angle),
                                      std::sin(angle), std::cos(angle)));
    }
    if (flipHorizontal)
        preMultiply(centeredOperation(-1.0, 0.0, 0.0, 1.0));
    if (flipVertical)
        preMultiply(centeredOperation(1.0, 0.0, 0.0, -1.0));
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
                    bool showImage, bool showSegmentation, double segmentationOpacity)
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
                     bool projectionData, bool showImage, bool showSegmentation,
                     double segmentationOpacity, int rotationQuarterTurns,
                     bool flipHorizontal, bool flipVertical, double cropMinimum,
                     double cropMaximum, double width, double level,
                     const QString &patientOrientation)
{
    pipeline->renderer->RemoveAllViewProps();
    pipeline->image = nullptr;
    pipeline->mask = nullptr;
    pipeline->sliceMapper = nullptr;
    pipeline->sliceActor = nullptr;
    pipeline->maskMapper = nullptr;
    pipeline->maskActor = nullptr;
    pipeline->maskLookup = nullptr;
    pipeline->volumeMapper = nullptr;
    pipeline->volumeProperty = nullptr;
    pipeline->volumeActor = nullptr;
    pipeline->segmentationActor = nullptr;
    pipeline->annotationProps.clear();

    if (!volume || volume->pixels.empty())
        return;

    pipeline->image = vtkImageFromVolume(*volume);
    updatePatientTransform(pipeline, *volume);
    // 显示翻转/旋转只作用于切片视图；3D 必须保持真实 LPS 世界坐标，
    // 否则 vtkVolume(GPU 路径) 与标注 vtkActor 对翻转矩阵应用不一致而错位。
    const bool applyDisplayTransform = projectionData
        && type != MedicalViewportItem::ViewType::Volume3D;
    pipeline->renderTransform = displayTransformFor(*volume, applyDisplayTransform,
                                                     patientOrientation,
                                                     rotationQuarterTurns,
                                                     flipHorizontal, flipVertical);
    auto combined = vtkSmartPointer<vtkMatrix4x4>::New();
    vtkMatrix4x4::Multiply4x4(pipeline->dataToWorld, pipeline->renderTransform, combined);
    pipeline->renderTransform = combined;
    pipeline->worldToRenderData = vtkSmartPointer<vtkMatrix4x4>::New();
    vtkMatrix4x4::Invert(pipeline->renderTransform, pipeline->worldToRenderData);
    if (mask && mask->dimensions == volume->dimensions)
        pipeline->mask = vtkImageFromMask(*mask);

    if (type == MedicalViewportItem::ViewType::Volume3D)
        configureVolume(pipeline, mip, cropMinimum, cropMaximum, preset,
                        showImage, showSegmentation, segmentationOpacity, width, level);
    else
        configureSlice(pipeline, type, slicePosition, width, level, showImage,
                       showSegmentation, segmentationOpacity);

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
    const bool projectionData = m_controller && m_controller->projectionData();
    const QString patientOrientation = m_controller
        ? (m_pairedProjection ? m_controller->projectionPairOrientation()
                              : m_controller->patientOrientation())
        : QString();
    rebuildPipeline(pipeline, renderWindow, m_volume, m_mask, m_viewType,
                    m_slicePosition, m_mip, m_volumePreset, projectionData,
                    m_showImage, m_showSegmentation, m_segmentationOpacity,
                    m_rotationQuarterTurns, m_flipHorizontal, m_flipVertical,
                    m_cropMinimum, m_cropMaximum, width, level,
                    patientOrientation);
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

bool MedicalViewportItem::mapClickToVoxel(double itemX, double itemY, bool updateSeed)
{
    if (m_viewType == ViewType::Volume3D || !m_controller || !m_volume
        || width() <= 0.0 || height() <= 0.0) {
        emit voxelPickFailed(QStringLiteral("当前视图无法拾取体素。"));
        return false;
    }

    QVector3D world;
    int voxel[3] = {0, 0, 0};
    if (!MarkupsPicker::mapClickToWorld(*m_volume, static_cast<int>(m_viewType), m_slicePosition,
                                        itemX, itemY, width(), height(), &world, voxel)) {
        emit voxelPickFailed(QStringLiteral("点击位置不在当前切片图像内。"));
        return false;
    }

    int hu = 0;
    const auto &dims = m_volume->dimensions;
    const auto &pixels = m_volume->pixels;
    const std::size_t flat = static_cast<std::size_t>(voxel[0])
        + static_cast<std::size_t>(voxel[1]) * static_cast<std::size_t>(dims[0])
        + static_cast<std::size_t>(voxel[2]) * static_cast<std::size_t>(dims[0])
              * static_cast<std::size_t>(dims[1]);
    if (flat < pixels.size())
        hu = pixels[flat];

    if (updateSeed && !m_controller->setRegionGrowingSeed(voxel[0], voxel[1], voxel[2])) {
        emit voxelPickFailed(QStringLiteral("无法设置种子点。"));
        return false;
    }

    const double normalizedX = std::clamp(itemX / width(), 0.0, 1.0);
    const double normalizedY = std::clamp(itemY / height(), 0.0, 1.0);
    const int value = updateSeed ? m_controller->regionGrowingSeedValue() : hu;
    emit voxelPicked(voxel[0], voxel[1], voxel[2], value, normalizedX, normalizedY);
    return true;
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
    double bestDist2 = tolerancePx * tolerancePx;
    int bestNodeId = -1;
    int bestPointIndex = -1;

    for (const QVariant &entry : items) {
        const QVariantMap item = entry.toMap();
        if (!item.value(QStringLiteral("visible"), true).toBool())
            continue;
        const int nodeId = item.value(QStringLiteral("id")).toInt();
        const QVariantList points = item.value(QStringLiteral("points")).toList();
        for (int pointIndex = 0; pointIndex < points.size(); ++pointIndex) {
            const QVariantMap point = points.at(pointIndex).toMap();
            const QVector3D world(point.value(QStringLiteral("x")).toFloat(),
                                 point.value(QStringLiteral("y")).toFloat(),
                                 point.value(QStringLiteral("z")).toFloat());
            if (std::abs(MarkupsPicker::sliceDelta(*m_volume, static_cast<int>(m_viewType),
                                                   m_slicePosition, world)) > 0)
                continue;
            double dx = 0.0;
            double dy = 0.0;
            if (!MarkupsPicker::worldToDisplay(*m_volume, static_cast<int>(m_viewType),
                                               m_slicePosition, width(), height(),
                                               world, &dx, &dy))
                continue;
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

    dispatch_async([items, volume, show, viewType, slicePosition, is3d](
                       vtkRenderWindow *, vtkUserData userData) {
        auto *pipeline = ViewportPipeline::SafeDownCast(userData);
        if (!pipeline || !pipeline->renderer)
            return;

        for (const auto &prop : pipeline->annotationProps)
            pipeline->renderer->RemoveViewProp(prop);
        pipeline->annotationProps.clear();

        if (!show || !volume || items.isEmpty() || !pipeline->renderTransform)
            return;

        constexpr double kRedR = 0.898;
        constexpr double kRedG = 0.224;
        constexpr double kRedB = 0.208;
        const double spacingMax = std::max({volume->spacing[0], volume->spacing[1],
                                            volume->spacing[2]});
        const double radius = spacingMax * (is3d ? 4.5 : 3.6);
        const double liftOffset = spacingMax * 0.6;

        auto toRenderPoint = [&](const QVector3D &world) -> std::array<double, 3> {
            if (is3d)
                return {static_cast<double>(world.x()), static_cast<double>(world.y()),
                        static_cast<double>(world.z())};
            auto p = MarkupsPicker::worldToImagePhysical(*volume, world);
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
            prop->SetColor(kRedR, kRedG, kRedB);
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

        // 标签锚点用点的精确世界坐标，偏移改用显示像素，文字像素对齐，
        // 缩放时只在屏幕平移、不重合不跑偏。
        auto addLabel = [&](const std::array<double, 3> &p, const QString &text,
                            double opacity) {
            if (opacity < 0.40 || text.isEmpty())
                return;
            auto label = vtkSmartPointer<vtkBillboardTextActor3D>::New();
            label->SetInput(text.toUtf8().constData());
            label->SetPosition(p[0], p[1], p[2]);
            label->SetDisplayOffset(12, 12);
            label->SetForceOpaque(true);
            auto *tp = label->GetTextProperty();
            tp->SetFontSize(is3d ? 16 : 15);
            tp->SetColor(kRedR, kRedG, kRedB);
            tp->SetBold(true);
            tp->SetOpacity(std::max(0.85, opacity));
            tp->SetShadow(true);
            tp->SetBackgroundOpacity(0.35);
            tp->SetBackgroundColor(0.02, 0.03, 0.04);
            pipeline->renderer->AddViewProp(label);
            pipeline->annotationProps.push_back(label);
        };

        auto opacityForWorld = [&](const QVector3D &world) -> double {
            if (is3d)
                return 1.0;
            const int delta = std::abs(MarkupsPicker::sliceDelta(*volume, viewType,
                                                                 slicePosition, world));
            if (delta == 0)
                return 1.0;
            if (delta == 1)
                return 0.40;
            if (delta == 2)
                return 0.20;
            return 0.0;
        };

        for (const QVariant &entry : items) {
            const QVariantMap item = entry.toMap();
            if (!item.value(QStringLiteral("visible"), true).toBool())
                continue;
            const QVariantList points = item.value(QStringLiteral("points")).toList();
            if (points.isEmpty())
                continue;
            const int type = item.value(QStringLiteral("type")).toInt();
            const QString displayText = item.value(QStringLiteral("displayText")).toString();
            const QString labelText = item.value(QStringLiteral("label")).toString();

            std::vector<std::array<double, 3>> render;
            std::vector<double> opacities;
            render.reserve(static_cast<std::size_t>(points.size()));
            opacities.reserve(static_cast<std::size_t>(points.size()));

            for (const QVariant &pointEntry : points) {
                const QVariantMap point = pointEntry.toMap();
                const QVector3D world(point.value(QStringLiteral("x")).toFloat(),
                                     point.value(QStringLiteral("y")).toFloat(),
                                     point.value(QStringLiteral("z")).toFloat());
                const double opacity = opacityForWorld(world);
                render.push_back(toRenderPoint(world));
                opacities.push_back(opacity);
                if (opacity > 0.0)
                    addSphere(render.back(), opacity);
            }

            auto segOpacity = [&](std::size_t a, std::size_t b) {
                return std::min(opacities[a], opacities[b]);
            };

            if (type == 0 && !render.empty()) {
                addLabel(render[0],
                         displayText.isEmpty() ? labelText : displayText, opacities[0]);
            } else if (type == 1 && render.size() >= 2) {
                const double opacity = segOpacity(0, 1);
                if (opacity > 0.0) {
                    addLine(render[0], render[1], opacity);
                    const std::array<double, 3> mid = {
                        (render[0][0] + render[1][0]) * 0.5,
                        (render[0][1] + render[1][1]) * 0.5,
                        (render[0][2] + render[1][2]) * 0.5};
                    addLabel(mid, displayText, opacity);
                }
            } else if (type == 2 && render.size() >= 2) {
                if (const double o01 = segOpacity(0, 1); o01 > 0.0)
                    addLine(render[0], render[1], o01);
                if (render.size() >= 3) {
                    if (const double o12 = segOpacity(1, 2); o12 > 0.0)
                        addLine(render[1], render[2], o12);
                    addLabel(render[1], displayText, opacities[1]);
                }
            } else if (type == 3 && render.size() >= 2) {
                for (std::size_t i = 1; i < render.size(); ++i) {
                    const double opacity = segOpacity(i - 1, i);
                    if (opacity > 0.0)
                        addLine(render[i - 1], render[i], opacity);
                }
                if (render.size() >= 3) {
                    const double opacity = segOpacity(render.size() - 1, 0);
                    if (opacity > 0.0)
                        addLine(render.back(), render.front(), opacity);
                }
                double cx = 0.0;
                double cy = 0.0;
                double cz = 0.0;
                double labelOpacity = 0.0;
                for (std::size_t i = 0; i < render.size(); ++i) {
                    cx += render[i][0];
                    cy += render[i][1];
                    cz += render[i][2];
                    labelOpacity = std::max(labelOpacity, opacities[i]);
                }
                const double n = static_cast<double>(render.size());
                addLabel({cx / n, cy / n, cz / n}, displayText, labelOpacity);
            }
        }

        pipeline->renderer->ResetCameraClippingRange();
    });
    scheduleRender();
}

void MedicalViewportItem::reloadData()
{
    m_volume = m_controller
        ? (m_pairedProjection ? m_controller->projectionPairSnapshot()
                              : m_controller->volumeSnapshot())
        : nullptr;
    m_mask = (m_controller && !m_pairedProjection)
        ? m_controller->maskSnapshot() : nullptr;
    const auto volume = m_volume;
    const auto mask = m_mask;
    const auto type = m_viewType;
    const double slice = m_slicePosition;
    const bool mipMode = m_mip;
    const auto preset = m_volumePreset;
    const bool projectionData = m_controller && m_controller->projectionData();
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
    const QString patientOrientation = m_controller
        ? (m_pairedProjection ? m_controller->projectionPairOrientation()
                              : m_controller->patientOrientation())
        : QString();
    // Lambda 只捕获不可变快照和值类型，避免渲染线程读取 GUI 对象的可变成员。
    dispatch_async([volume, mask, type, slice, mipMode, preset, projectionData,
                    showImage, segmentation, segmentationOpacity,
                    rotationQuarterTurns, flipHorizontal, flipVertical,
                    cropMin, cropMax, width, level, patientOrientation](vtkRenderWindow *window,
                                                   vtkUserData userData) {
        auto *pipeline = ViewportPipeline::SafeDownCast(userData);
        if (pipeline)
            rebuildPipeline(pipeline, window, volume, mask, type, slice, mipMode, preset,
                            projectionData, showImage, segmentation, segmentationOpacity,
                            rotationQuarterTurns, flipHorizontal, flipVertical,
                            cropMin, cropMax, width, level, patientOrientation);
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
    });
    scheduleRender();
}
