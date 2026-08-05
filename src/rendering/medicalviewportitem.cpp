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
#include <QStringList>

namespace {

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
    const int slice = std::clamp(static_cast<int>(slicePosition * (sliceCount - 1)),
                                 0, std::max(0, sliceCount - 1));

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

    if (!volume || volume->pixels.empty())
        return;

    pipeline->image = vtkImageFromVolume(*volume);
    updatePatientTransform(pipeline, *volume);
    pipeline->renderTransform = displayTransformFor(*volume, projectionData,
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
            || !pipeline->image || !pipeline->worldToRenderData || !view || !controller)
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
        pipeline->worldToRenderData->MultiplyPoint(worldPoint, dataPoint);
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
            const int slice = std::clamp(static_cast<int>(slicePosition * (count - 1)),
                                         0, std::max(0, count - 1));
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
