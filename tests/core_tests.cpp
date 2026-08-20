#include "src/application/workflowcontroller.h"
#include "src/annotation/annotationcontroller.h"
#include "src/dicom/dicompresentation.h"
#include "src/dicom/dicomtextcodec.h"
#include "src/dicom/medicaldatacontroller.h"
#include "src/markups/markupsmetrics.h"
#include "src/markups/markupspicker.h"
#include "src/markups/markupsscene.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfoList>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVariantMap>
#include <QtTest>

#include <algorithm>
#include <cmath>
#include <limits>

class CoreTests final : public QObject
{
    Q_OBJECT

private slots:
    void workflowGuardsFutureSteps();
    void demoVolumeAndThresholdAreAvailable();
    void windowingTransactionPublishesOnce();
    void asynchronousSegmentationPublishesStatistics();
    void volumeNodeLifecycle();
    void markupsMetricsAndEditing();
    void closedCurveMeasurementUsesSmoothCurve();
    void physicalSlicePickingMatchesDisplay();
    void markupPointsProjectAcrossSliceViews();
    void markupsRespectSliceSlabAndIntersections();
    void presentationAndViewTransformPickingRoundTrips();
    void pointListAndCurveFollowSlicerSemantics();
    void annotationsAreIsolatedPerDataset();
    void projectionPresentationPolarityIsDetected();
    void dicomTextCharacterSetsAndDamagedLabels();
    void regionGrowingSeedStateAndValidation();
    void realDicomRoundTripWhenConfigured();
    void casePackageRoundTripWhenConfigured();
    void recursiveLidcRootLoadsCtAndDx();
    void realLidcRegionGrowingPerformance();
    void mixedRootLoadsUnsignedDx();
};

void CoreTests::workflowGuardsFutureSteps()
{
    WorkflowController workflow;
    QCOMPARE(workflow.currentStep(), 0);
    QVERIFY(!workflow.canVisit(1));
    workflow.goToStep(3);
    QCOMPARE(workflow.currentStep(), 0);

    workflow.advance();
    QCOMPARE(workflow.currentStep(), 1);
    QVERIFY(workflow.canVisit(1));
    workflow.setLocked(true);
    workflow.advance();
    QCOMPARE(workflow.currentStep(), 1);
    QCOMPARE(workflow.stateName(), QStringLiteral("异常 / 锁定"));
}

void CoreTests::demoVolumeAndThresholdAreAvailable()
{
    MedicalDataController data;
    QSignalSpy dataSpy(&data, &MedicalDataController::dataChanged);
    data.loadDemoVolume();
    QVERIFY(data.loaded());
    QVERIFY(data.volumeData());
    QCOMPARE(data.patientId(), QStringLiteral("DEMO-CT-001"));
    QCOMPARE(dataSpy.count(), 1);

    QVERIFY(data.applyThreshold(40.0, 60.0));
    QVERIFY(data.segmentationAvailable());
    const auto mask = data.maskSnapshot();
    QVERIFY(mask);
    QVERIFY(std::any_of(mask->pixels.cbegin(), mask->pixels.cend(),
                        [](unsigned char value) { return value != 0; }));
    QCOMPARE(data.segmentationMethod(), QStringLiteral("阈值分割"));
    QVERIFY(data.segmentationVoxelCount() > 0);
    QVERIFY(data.segmentationVolumeMl() > 0.0);
}

void CoreTests::asynchronousSegmentationPublishesStatistics()
{
#if !CT_ENABLE_MEDICAL_BACKEND
    QSKIP("The MinGW UI compatibility build does not run asynchronous ITK algorithms.");
#else
    MedicalDataController data;
    data.loadDemoVolume();

    QElapsedTimer dispatchTimer;
    dispatchTimer.start();
    data.applyThresholdAsync(40.0, 60.0);
    QVERIFY2(dispatchTimer.elapsed() < 500,
             "The asynchronous threshold command blocked the caller.");
    QVERIFY(data.busy());
    QTRY_VERIFY_WITH_TIMEOUT(!data.busy(), 30000);
    QVERIFY2(data.errorMessage().isEmpty(), qPrintable(data.errorMessage()));
    QCOMPARE(data.segmentationMethod(), QStringLiteral("阈值分割"));
    QVERIFY(data.segmentationVoxelCount() > 0);

    QVERIFY(data.setRegionGrowingSeed(96, 96, 80));
    dispatchTimer.restart();
    data.applyRegionGrowingFromSeedAsync(40.0, 60.0, true);
    QVERIFY2(dispatchTimer.elapsed() < 500,
             "The asynchronous region-growing command blocked the caller.");
    QVERIFY(data.busy());
    QTRY_VERIFY_WITH_TIMEOUT(!data.busy(), 30000);
    QVERIFY2(data.errorMessage().isEmpty(), qPrintable(data.errorMessage()));
    QCOMPARE(data.segmentationMethod(), QStringLiteral("种子生长（26 邻域）"));
    QVERIFY(data.segmentationVoxelCount() > 0);
    QVERIFY(data.segmentationVolumeMl() > 0.0);
#endif
}

void CoreTests::volumeNodeLifecycle()
{
    MedicalDataController data;
    data.loadDemoVolume();
    QCOMPARE(data.volumeNodes().size(), 1);
    QCOMPARE(data.selectedVolumeIndex(), 0);
    QVERIFY(data.activeVolumeVisible());

    QVERIFY(data.renameVolume(0, QStringLiteral("Renamed CT")));
    QCOMPARE(data.seriesDescription(), QStringLiteral("Renamed CT"));
    QCOMPARE(data.volumeNodes().front().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Renamed CT"));

    QVERIFY(data.setVolumeVisibility(0, false));
    QVERIFY(!data.activeVolumeVisible());
    QVERIFY(data.setVolumeVisibility(0, true));
    QVERIFY(data.activeVolumeVisible());

    QVERIFY(data.removeVolume(0));
    QVERIFY(!data.loaded());
    QVERIFY(data.volumeNodes().isEmpty());
    QCOMPARE(data.selectedVolumeIndex(), -1);
}

void CoreTests::windowingTransactionPublishesOnce()
{
    MedicalDataController data;
    data.loadDemoVolume();
    QSignalSpy windowingSpy(&data, &MedicalDataController::windowingChanged);

    data.setWindowing(1500.0, -600.0);
    QCOMPARE(data.windowWidth(), 1500.0);
    QCOMPARE(data.windowLevel(), -600.0);
    QCOMPARE(windowingSpy.count(), 1);

    data.setWindowing(1500.0, -600.0);
    QCOMPARE(windowingSpy.count(), 1);
}

void CoreTests::dicomTextCharacterSetsAndDamagedLabels()
{
    const std::string chineseWindowPreset("\xD3\xC3\xBB\xA7\xD1\xA1\xD4\xF1\xCF\xEE", 10);
    QCOMPARE(DicomTextCodec::decode(chineseWindowPreset, QStringLiteral("GB18030")),
             QStringLiteral("用户选择项"));

    const char damagedBytes[] = {
        '?', '1', '?', static_cast<char>(0xA8), static_cast<char>(0xB4),
        static_cast<char>(0xA8), static_cast<char>(0xA8), '?', '3',
        static_cast<char>(0xA1), static_cast<char>(0xE8), '2', 'a',
        static_cast<char>(0xA8), static_cast<char>(0xA2), '?', '?', '?',
        static_cast<char>(0xA1), static_cast<char>(0xEA), static_cast<char>(0xA1),
        static_cast<char>(0xA7), 'E', 'O', 'S', ' ', '?', '?',
        static_cast<char>(0xA8), static_cast<char>(0xA2), static_cast<char>(0xA1),
        static_cast<char>(0xE9), '?', '?', static_cast<char>(0xA1),
        static_cast<char>(0xEA), '?', ' '};
    const QString damaged = DicomTextCodec::decode(
        std::string(damagedBytes, sizeof(damagedBytes)), QStringLiteral("GB18030"));
    QVERIFY(!DicomTextCodec::isUsableDisplayText(damaged));
    QVERIFY(DicomTextCodec::isUsableDisplayText(QStringLiteral("Chest? AP")));
    QVERIFY(DicomTextCodec::isUsableDisplayText(QStringLiteral("胸部正位")));
}

void CoreTests::markupsMetricsAndEditing()
{
    QCOMPARE(MarkupsMetrics::distanceMm(QVector3D(0, 0, 0), QVector3D(3, 4, 0)), 5.0);
    QCOMPARE(MarkupsMetrics::angleDegrees(QVector3D(1, 0, 0), QVector3D(0, 0, 0),
                                          QVector3D(0, 1, 0)), 90.0);
    QCOMPARE(MarkupsMetrics::perimeterMm(
                 {QVector3D(0, 0, 0), QVector3D(3, 0, 0), QVector3D(3, 4, 0)}, true),
             12.0);

    MarkupsScene scene;
    scene.setTool(MarkupsTool::Length);
    QVERIFY(scene.addWorldPoint(QVector3D(0, 0, 0)));
    QVERIFY(scene.addWorldPoint(QVector3D(3, 4, 0)));
    QCOMPARE(scene.measureCount(), 1);
    QVERIFY(scene.updateControlPoint(1, 1, QVector3D(0, 10, 0)));
    QCOMPARE(scene.nodes().front().metric, 10.0);
}

void CoreTests::closedCurveMeasurementUsesSmoothCurve()
{
    const std::vector<QVector3D> controls {
        QVector3D(0, 0, 0), QVector3D(10, 0, 0),
        QVector3D(10, 10, 0), QVector3D(0, 10, 0)};
    const auto samples = MarkupsMetrics::closedCurveSamples(controls, 16);
    QCOMPARE(samples.size(), std::size_t(64));
    for (std::size_t index = 0; index < controls.size(); ++index)
        QCOMPARE(samples[index * 16], controls[index]);

    const double curveLength = MarkupsMetrics::closedCurveLengthMm(controls, 16);
    QVERIFY(curveLength > MarkupsMetrics::perimeterMm(controls, true));
    QVERIFY(curveLength < 45.0);

    MarkupsScene scene;
    scene.setTool(MarkupsTool::Perimeter);
    for (const QVector3D &point : controls)
        QVERIFY(scene.addWorldPoint(point));
    QVERIFY(scene.hasActive());
    QVERIFY(!scene.activeNode()->closed);
    const auto openSamples = MarkupsMetrics::curveSamples(controls, false, 16);
    QCOMPARE(openSamples.front(), controls.front());
    QCOMPARE(openSamples.back(), controls.back());
    QVERIFY(scene.finishActive());
    QCOMPARE(scene.measureCount(), 1);
    QCOMPARE(scene.nodes().front().controlPoints.size(), controls.size());
    QVERIFY(!scene.nodes().front().closed);
    QVERIFY(std::abs(scene.nodes().front().metric
                     - MarkupsMetrics::curveLengthMm(controls, false)) < 1e-5);
}

void CoreTests::physicalSlicePickingMatchesDisplay()
{
    VolumeSnapshot volume;
    volume.dimensions = {512, 512, 133};
    volume.spacing = {0.7, 0.7, 2.5};
    volume.origin = {0.0, 0.0, 0.0};
    volume.direction = {1.0, 0.0, 0.0,
                        0.0, 1.0, 0.0,
                        0.0, 0.0, 1.0};

    constexpr double viewportWidth = 800.0;
    constexpr double viewportHeight = 600.0;
    const QVector3D world = MarkupsPicker::voxelToWorld(volume, 100, 10, 40);
    double displayX = 0.0;
    double displayY = 0.0;
    QVERIFY(MarkupsPicker::worldToDisplay(volume, 1, 0.5,
                                           viewportWidth, viewportHeight, world,
                                           &displayX, &displayY));

    QVector3D pickedWorld;
    int voxel[3] = {-1, -1, -1};
    QVERIFY(MarkupsPicker::mapClickToWorld(volume, 1, 0.5,
                                            displayX, displayY,
                                            viewportWidth, viewportHeight,
                                            &pickedWorld, voxel));
    QCOMPARE(voxel[0], 100);
    QCOMPARE(voxel[1], 255);
    QCOMPARE(voxel[2], 40);

    const auto physicalSize = MarkupsPicker::slicePhysicalSize(volume, 1);
    QCOMPARE(physicalSize[0], 357.7);
    QCOMPARE(physicalSize[1], 330.0);
    const double scale = std::min(viewportWidth / physicalSize[0],
                                  viewportHeight / physicalSize[1]);
    const double offsetX = (viewportWidth - physicalSize[0] * scale) * 0.5;
    QVERIFY(std::abs(displayX - (offsetX + 70.0 * scale)) < 1e-3);
    QVERIFY(std::abs(displayY - ((330.0 - 100.0) * scale)) < 1e-3);
}

void CoreTests::markupPointsProjectAcrossSliceViews()
{
    VolumeSnapshot volume;
    volume.dimensions = {11, 21, 31};
    volume.spacing = {0.5, 1.0, 2.0};
    volume.origin = {10.0, 20.0, 30.0};
    volume.direction = {1.0, 0.0, 0.0,
                        0.0, 1.0, 0.0,
                        0.0, 0.0, 1.0};
    const QVector3D world = MarkupsPicker::voxelToWorld(volume, 3, 7, 9);

    const auto axial = MarkupsPicker::worldToSliceImagePhysical(volume, 0, 0.5, world);
    QCOMPARE(axial[0], 1.5);
    QCOMPARE(axial[1], 7.0);
    QCOMPARE(axial[2], 30.0);

    const auto coronal = MarkupsPicker::worldToSliceImagePhysical(volume, 1, 0.5, world);
    QCOMPARE(coronal[0], 1.5);
    QCOMPARE(coronal[1], 10.0);
    QCOMPARE(coronal[2], 18.0);

    const auto sagittal = MarkupsPicker::worldToSliceImagePhysical(volume, 2, 0.5, world);
    QCOMPARE(sagittal[0], 2.5);
    QCOMPARE(sagittal[1], 7.0);
    QCOMPARE(sagittal[2], 18.0);

    volume.direction = {-1.0, 0.0, 0.0,
                         0.0, 1.0, 0.0,
                         0.0, 0.0, -1.0};
    const QVector3D flippedWorld = MarkupsPicker::voxelToWorld(volume, 3, 7, 9);
    const auto flipped = MarkupsPicker::worldToSliceImagePhysical(
        volume, 0, 0.5, flippedWorld);
    QCOMPARE(flipped[0], 1.5);
    QCOMPARE(flipped[1], 7.0);
    QCOMPARE(flipped[2], 30.0);
}

void CoreTests::markupsRespectSliceSlabAndIntersections()
{
    VolumeSnapshot volume;
    volume.dimensions = {11, 21, 11};
    volume.spacing = {1.0, 2.0, 3.0};
    volume.origin = {10.0, 20.0, 30.0};
    volume.direction = {1.0, 0.0, 0.0,
                        0.0, 1.0, 0.0,
                        0.0, 0.0, 1.0};
    for (int index = 0; index < volume.dimensions[2]; ++index) {
        const double position = static_cast<double>(index) / (volume.dimensions[2] - 1);
        QCOMPARE(MarkupsPicker::sliceIndexFromPosition(position, volume.dimensions[2]),
                 index);
    }

    const QVector3D onAxial = MarkupsPicker::voxelToWorld(volume, 4, 7, 5);
    const QVector3D nextAxial = MarkupsPicker::voxelToWorld(volume, 4, 7, 6);
    QCOMPARE(MarkupsPicker::signedSliceDistanceMm(volume, 0, 0.5, onAxial), 0.0);
    QCOMPARE(MarkupsPicker::signedSliceDistanceMm(volume, 0, 0.5, nextAxial), 3.0);
    QCOMPARE(MarkupsPicker::sliceSlabHalfThicknessMm(volume, 0), 1.5);
    QVERIFY(MarkupsPicker::isPointDisplayableOnSlice(volume, 0, 0.5, onAxial));
    QVERIFY(!MarkupsPicker::isPointDisplayableOnSlice(volume, 0, 0.5, nextAxial));

    const QVector3D onCoronal = MarkupsPicker::voxelToWorld(volume, 4, 10, 5);
    const QVector3D onSagittal = MarkupsPicker::voxelToWorld(volume, 5, 7, 5);
    QCOMPARE(MarkupsPicker::signedSliceDistanceMm(volume, 1, 0.5, onCoronal), 0.0);
    QCOMPARE(MarkupsPicker::signedSliceDistanceMm(volume, 2, 0.5, onSagittal), 0.0);

    const QVector3D below = MarkupsPicker::voxelToWorld(volume, 2, 7, 4);
    const QVector3D above = MarkupsPicker::voxelToWorld(volume, 8, 7, 6);
    QVector3D intersection;
    QVERIFY(MarkupsPicker::segmentSlicePlaneIntersection(
        volume, 0, 0.5, below, above, &intersection));
    QVERIFY(std::abs(intersection.x() - 15.0f) < 1e-5f);
    QVERIFY(std::abs(intersection.y() - 34.0f) < 1e-5f);
    QVERIFY(std::abs(intersection.z() - 45.0f) < 1e-5f);

    QVector3D clippedA;
    QVector3D clippedB;
    QVERIFY(MarkupsPicker::clipSegmentToSliceSlab(
        volume, 0, 0.5, below, above, &clippedA, &clippedB));
    QVERIFY(std::abs(clippedA.z() - 43.5f) < 1e-5f);
    QVERIFY(std::abs(clippedB.z() - 46.5f) < 1e-5f);

    const QVector3D offA = MarkupsPicker::voxelToWorld(volume, 2, 7, 7);
    const QVector3D offB = MarkupsPicker::voxelToWorld(volume, 8, 7, 7);
    QVERIFY(!MarkupsPicker::segmentSlicePlaneIntersection(
        volume, 0, 0.5, offA, offB, &intersection));
    QVERIFY(!MarkupsPicker::clipSegmentToSliceSlab(
        volume, 0, 0.5, offA, offB, &clippedA, &clippedB));

    const auto runs = MarkupsPicker::clipPolylineToSliceSlab(
        volume, 0, 0.5, {offA, below, above, offB}, false);
    QCOMPARE(runs.size(), std::size_t(2));
    for (const auto &run : runs) {
        QVERIFY(run.size() >= std::size_t(2));
        for (const QVector3D &point : run) {
            QVERIFY(std::abs(MarkupsPicker::signedSliceDistanceMm(
                        volume, 0, 0.5, point)) <= 1.50001);
        }
    }
}

void CoreTests::presentationAndViewTransformPickingRoundTrips()
{
    VolumeSnapshot xray;
    xray.dimensions = {1896, 4183, 1};
    xray.spacing = {0.179363, 0.179363, 1.0};
    xray.origin = {0.0, 0.0, 0.0};
    xray.direction = {1.0, 0.0, 0.0,
                      0.0, 1.0, 0.0,
                      0.0, 0.0, 1.0};
    constexpr double viewportWidth = 760.0;
    constexpr double viewportHeight = 920.0;
    const QVector3D xrayWorld = MarkupsPicker::voxelToWorld(xray, 417, 3210, 0);

    for (int turns = 0; turns < 4; ++turns) {
        for (bool flipHorizontal : {false, true}) {
            for (bool flipVertical : {false, true}) {
                const auto presentation = MarkupsPicker::imagePresentationFor(
                    xray, true, QStringLiteral("L\\F"), turns,
                    flipHorizontal, flipVertical);
                double displayX = 0.0;
                double displayY = 0.0;
                QVERIFY(MarkupsPicker::worldToDisplay(
                    xray, 0, 0.0, viewportWidth, viewportHeight, xrayWorld,
                    &displayX, &displayY, presentation));
                QVector3D picked;
                int voxel[3] {-1, -1, -1};
                QVERIFY(MarkupsPicker::mapClickToWorld(
                    xray, 0, 0.0, displayX, displayY, viewportWidth, viewportHeight,
                    &picked, voxel, presentation));
                QCOMPARE(voxel[0], 417);
                QCOMPARE(voxel[1], 3210);
                QCOMPARE(voxel[2], 0);
            }
        }
    }

    const auto rotated = MarkupsPicker::imagePresentationFor(
        xray, true, QStringLiteral("L\\F"), 1, false, false);
    const auto rotatedSize = MarkupsPicker::sliceViewPhysicalSize(xray, 0, rotated);
    const auto rawSize = MarkupsPicker::slicePhysicalSize(xray, 0);
    QVERIFY(std::abs(rotatedSize[0] - rawSize[1]) < 1e-6);
    QVERIFY(std::abs(rotatedSize[1] - rawSize[0]) < 1e-6);

    VolumeSnapshot pairedXray = xray;
    pairedXray.dimensions = {1764, 4183, 1};
    const QVector3D pairedWorld = MarkupsPicker::voxelToWorld(pairedXray, 1200, 2750, 0);
    const auto pairedPresentation = MarkupsPicker::imagePresentationFor(
        pairedXray, true, QStringLiteral("P\\F"), 0, false, false);
    double pairedBaseX = 0.0;
    double pairedBaseY = 0.0;
    QVERIFY(MarkupsPicker::worldToDisplay(
        pairedXray, 0, 0.0, viewportWidth, viewportHeight, pairedWorld,
        &pairedBaseX, &pairedBaseY, pairedPresentation));
    constexpr double pairedZoom = 3.2;
    constexpr double pairedPanX = -91.0;
    constexpr double pairedPanY = 64.0;
    const double pairedDisplayX = viewportWidth * 0.5
        + pairedZoom * (pairedBaseX - viewportWidth * 0.5) + pairedPanX;
    const double pairedDisplayY = viewportHeight * 0.5
        + pairedZoom * (pairedBaseY - viewportHeight * 0.5) + pairedPanY;
    const double pairedUnzoomedX = viewportWidth * 0.5
        + (pairedDisplayX - viewportWidth * 0.5 - pairedPanX) / pairedZoom;
    const double pairedUnzoomedY = viewportHeight * 0.5
        + (pairedDisplayY - viewportHeight * 0.5 - pairedPanY) / pairedZoom;
    int pairedVoxel[3] {-1, -1, -1};
    QVector3D pairedPickedWorld;
    QVERIFY(MarkupsPicker::mapClickToWorld(
        pairedXray, 0, 0.0, pairedUnzoomedX, pairedUnzoomedY,
        viewportWidth, viewportHeight, &pairedPickedWorld, pairedVoxel,
        pairedPresentation));
    QCOMPARE(pairedVoxel[0], 1200);
    QCOMPARE(pairedVoxel[1], 2750);
    QCOMPARE(pairedVoxel[2], 0);

    VolumeSnapshot lidc;
    lidc.dimensions = {512, 512, 133};
    lidc.spacing = {0.703125, 0.703125, 2.5};
    lidc.origin = {-166.0, -171.7, -10.0};
    lidc.direction = {1.0, 0.0, 0.0,
                      0.0, 1.0, 0.0,
                      0.0, 0.0, -1.0};
    constexpr double slicePosition = 64.0 / 132.0;
    const QVector3D lidcWorld = MarkupsPicker::voxelToWorld(lidc, 337, 128, 64);
    double baseX = 0.0;
    double baseY = 0.0;
    QVERIFY(MarkupsPicker::worldToDisplay(
        lidc, 0, slicePosition, 900.0, 650.0, lidcWorld, &baseX, &baseY));
    constexpr double zoom = 2.75;
    constexpr double panX = 137.0;
    constexpr double panY = -83.0;
    const double displayedX = 450.0 + zoom * (baseX - 450.0) + panX;
    const double displayedY = 325.0 + zoom * (baseY - 325.0) + panY;
    const double unzoomedX = 450.0 + (displayedX - 450.0 - panX) / zoom;
    const double unzoomedY = 325.0 + (displayedY - 325.0 - panY) / zoom;
    int pickedVoxel[3] {-1, -1, -1};
    QVector3D pickedWorld;
    QVERIFY(MarkupsPicker::mapClickToWorld(
        lidc, 0, slicePosition, unzoomedX, unzoomedY, 900.0, 650.0,
        &pickedWorld, pickedVoxel));
    QCOMPARE(pickedVoxel[0], 337);
    QCOMPARE(pickedVoxel[1], 128);
    QCOMPARE(pickedVoxel[2], 64);
}

void CoreTests::pointListAndCurveFollowSlicerSemantics()
{
    MarkupsScene points;
    points.setTool(MarkupsTool::Mark);
    QVERIFY(points.addWorldPoint(QVector3D(1, 2, 3)));
    QVERIFY(points.addWorldPoint(QVector3D(4, 5, 6)));
    QVERIFY(points.hasActive());
    QCOMPARE(points.markCount(), 1);
    QVERIFY(points.nodes().empty());
    QVERIFY(points.finishActive());
    QCOMPARE(points.nodes().size(), std::size_t(1));
    QCOMPARE(points.nodes().front().controlPoints.size(), std::size_t(2));

    MarkupsScene projectionLine;
    projectionLine.setTool(MarkupsTool::Length);
    QVERIFY(projectionLine.addWorldPoint(QVector3D(1, 2, 0),
                                         QStringLiteral("projection-primary")));
    QVERIFY(!projectionLine.addWorldPoint(QVector3D(3, 4, 0),
                                          QStringLiteral("projection-pair")));
    QVERIFY(projectionLine.addWorldPoint(QVector3D(3, 4, 0),
                                         QStringLiteral("projection-primary")));
    QCOMPARE(projectionLine.nodes().size(), std::size_t(1));
    QCOMPARE(projectionLine.nodes().front().viewId,
             QStringLiteral("projection-primary"));

    MarkupsScene curve;
    curve.setTool(MarkupsTool::Perimeter);
    const std::vector<QVector3D> controls {
        QVector3D(0, 0, 0), QVector3D(10, 0, 0), QVector3D(10, 10, 0)};
    for (const QVector3D &point : controls)
        QVERIFY(curve.addWorldPoint(point));
    QVERIFY(curve.finishActive());
    QVERIFY(!curve.nodes().front().closed);
    QVERIFY(std::abs(curve.nodes().front().metric
                     - MarkupsMetrics::curveLengthMm(controls, false)) < 1e-5);
}

void CoreTests::annotationsAreIsolatedPerDataset()
{
    MedicalDataController data;
    AnnotationController annotations;
    annotations.setMedicalData(&data);

    data.loadDemoVolume();
    const QString firstId = data.activeVolumeId();
    QVERIFY(!firstId.isEmpty());
    annotations.setToolType(AnnotationController::MarkTool);
    QVERIFY(annotations.addWorldPoint(1.0, 2.0, 3.0));
    QCOMPARE(annotations.markCountFor(firstId), 1);

    // Reloading the same source updates its node and must preserve annotations.
    data.loadDemoVolume();
    QCOMPARE(data.activeVolumeId(), firstId);
    QCOMPARE(annotations.markCount(), 1);

    // Removing the dataset also removes its annotation scene.
    QSignalSpy annotationSpy(&annotations, &AnnotationController::annotationsChanged);
    QVERIFY(data.removeVolume(0));
    QVERIFY(data.volumeNodes().isEmpty());
    QVERIFY(data.activeVolumeId().isEmpty());
    QVERIFY(annotationSpy.count() > 0);
    QCOMPARE(annotations.markCount(), 0);
    QCOMPARE(annotations.markCountFor(firstId), 0);
    data.loadDemoVolume();
    QCOMPARE(annotations.markCount(), 0);
}

void CoreTests::projectionPresentationPolarityIsDetected()
{
    QVERIFY(!DicomPresentation::grayscaleInverted(
        QStringLiteral("MONOCHROME2"), QStringLiteral("IDENTITY")));
    QVERIFY(DicomPresentation::grayscaleInverted(
        QStringLiteral("MONOCHROME1"), QString()));
    QVERIFY(DicomPresentation::grayscaleInverted(
        QStringLiteral("MONOCHROME2"), QStringLiteral("INVERSE")));
    QVERIFY(DicomPresentation::grayscaleInverted(
        QStringLiteral("  monochrome1  "), QStringLiteral(" inverse ")));
    QVERIFY(!DicomPresentation::grayscaleInverted(QString(), QString()));
}

void CoreTests::regionGrowingSeedStateAndValidation()
{
    MedicalDataController data;
    QSignalSpy seedSpy(&data, &MedicalDataController::regionGrowingSeedChanged);
    data.loadDemoVolume();
    QVERIFY(!data.regionGrowingSeedValid());

    QVERIFY(data.setRegionGrowingSeed(96, 96, 80));
    QVERIFY(data.regionGrowingSeedValid());
    QCOMPARE(data.regionGrowingSeedX(), 96);
    QCOMPARE(data.regionGrowingSeedY(), 96);
    QCOMPARE(data.regionGrowingSeedZ(), 80);
    QCOMPARE(data.regionGrowingSeedValue(), 45);

    QVERIFY(!data.applyRegionGrowingFromSeed(
        std::numeric_limits<double>::quiet_NaN(), 60.0));
    QVERIFY(data.errorMessage().contains(QStringLiteral("HU")));
    QVERIFY(!data.applyRegionGrowingFromSeed(-1000.0, -500.0));
    QVERIFY(data.errorMessage().contains(QStringLiteral("45 HU")));
    QVERIFY(data.applyRegionGrowingFromSeed(40.0, 60.0));
    QVERIFY(data.segmentationAvailable());
    const auto mask = data.maskSnapshot();
    QVERIFY(mask);
    QCOMPARE(static_cast<qint64>(std::count(mask->pixels.cbegin(), mask->pixels.cend(), 1)),
             data.segmentationVoxelCount());
    QVERIFY(std::all_of(mask->pixels.cbegin(), mask->pixels.cend(),
                        [](unsigned char value) { return value <= 1; }));

    data.clearRegionGrowingSeed();
    QVERIFY(!data.regionGrowingSeedValid());
    QCOMPARE(data.statusMessage(), QStringLiteral("种子点已清除"));
    QVERIFY(data.errorMessage().isEmpty());
    QVERIFY(seedSpy.count() >= 3);
}

void CoreTests::realDicomRoundTripWhenConfigured()
{
#if CT_ENABLE_MEDICAL_BACKEND
    const QString dicomDirectory = qEnvironmentVariable("CT_UI_TEST_DICOM_DIR");
    if (dicomDirectory.isEmpty())
        QSKIP("Set CT_UI_TEST_DICOM_DIR to run the real DICOM integration test.");

    MedicalDataController data;
    QVERIFY2(data.importDicom(QUrl::fromLocalFile(dicomDirectory)),
             qPrintable(data.errorMessage()));
    QVERIFY(data.loaded());
    QVERIFY(data.volumeData());
    QVERIFY(data.dimensionsText() != QStringLiteral("--"));

    QTemporaryDir exportDirectory;
    QVERIFY(exportDirectory.isValid());
    QVERIFY2(data.exportDicomCopy(QUrl::fromLocalFile(exportDirectory.path())),
             qPrintable(data.errorMessage()));
    const QFileInfoList exported = QDir(exportDirectory.path()).entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot);
    QVERIFY(!exported.isEmpty());
    QVERIFY(std::all_of(exported.cbegin(), exported.cend(),
                        [](const QFileInfo &file) { return file.size() > 0; }));
#else
    QSKIP("The MinGW UI compatibility build does not link the medical backend.");
#endif
}

void CoreTests::recursiveLidcRootLoadsCtAndDx()
{
#if CT_ENABLE_MEDICAL_BACKEND
    const QString root = qEnvironmentVariable("CT_UI_TEST_LIDC_ROOT");
    if (root.isEmpty())
        QSKIP("Set CT_UI_TEST_LIDC_ROOT to test recursive CT/DX discovery.");

    MedicalDataController data;
    AnnotationController annotations;
    annotations.setMedicalData(&data);
    QElapsedTimer scanTimer;
    scanTimer.start();
    data.importDicomAsync(QUrl::fromLocalFile(root));
    QTRY_VERIFY_WITH_TIMEOUT(!data.busy(), 120000);
    qInfo() << "Real LIDC scan:" << scanTimer.elapsed() << "ms for"
            << data.seriesChoices().size() << "candidates";
    QVERIFY2(data.errorMessage().isEmpty(), qPrintable(data.errorMessage()));
    QVERIFY(data.seriesChoices().size() >= 3);

    int ctIndex = -1;
    int dxIndex = -1;
    for (const QVariant &entry : data.seriesChoices()) {
        const QVariantMap choice = entry.toMap();
        if (choice.value(QStringLiteral("modality")) == QStringLiteral("CT"))
            ctIndex = choice.value(QStringLiteral("index")).toInt();
        if (choice.value(QStringLiteral("modality")) == QStringLiteral("DX") && dxIndex < 0)
            dxIndex = choice.value(QStringLiteral("index")).toInt();
    }
    QVERIFY(ctIndex >= 0);
    QVERIFY(dxIndex >= 0);

    QElapsedTimer dispatchTimer;
    dispatchTimer.start();
    data.selectSeriesAsync(ctIndex);
    QVERIFY2(dispatchTimer.elapsed() < 500,
             "The asynchronous CT decode command blocked the caller.");
    QVERIFY(data.busy());
    QTRY_VERIFY_WITH_TIMEOUT(!data.busy(), 120000);
    qInfo() << "Real LIDC CT pixel decode:" << dispatchTimer.elapsed() << "ms";
    QVERIFY2(data.errorMessage().isEmpty(), qPrintable(data.errorMessage()));
    QVERIFY(data.loaded());
    QVERIFY(data.volumeData());
    QCOMPARE(data.modality(), QStringLiteral("CT"));
    QCOMPARE(data.patientId(), QStringLiteral("LIDC-IDRI-0001"));
    const QString ctVolumeId = data.activeVolumeId();
    annotations.setToolType(AnnotationController::MarkTool);
    QVERIFY(annotations.addWorldPoint(1.0, 2.0, 3.0));
    QCOMPARE(annotations.markCountFor(ctVolumeId), 1);

    dispatchTimer.restart();
    data.selectSeriesAsync(dxIndex);
    QVERIFY2(dispatchTimer.elapsed() < 500,
             "The asynchronous projection decode command blocked the caller.");
    QVERIFY(data.busy());
    QTRY_VERIFY_WITH_TIMEOUT(!data.busy(), 120000);
    QVERIFY2(data.errorMessage().isEmpty(), qPrintable(data.errorMessage()));
    QVERIFY(data.loaded());
    QVERIFY(!data.volumeData());
    QCOMPARE(data.modality(), QStringLiteral("DX"));
    QCOMPARE(data.volumeSnapshot()->dimensions[2], 1);
    const QString dxVolumeId = data.activeVolumeId();
    QVERIFY(dxVolumeId != ctVolumeId);
    QCOMPARE(annotations.markCount(), 0);
    const bool projectionInverted = data.projectionInverted();
    const bool projectionPairInverted = data.projectionPairInverted();
    QCOMPARE(data.volumeNodes().size(), 2);
    QVERIFY(data.selectVolume(0));
    QCOMPARE(data.modality(), QStringLiteral("CT"));
    QCOMPARE(annotations.markCount(), 1);
    QVERIFY(data.selectVolume(1));
    QCOMPARE(data.modality(), QStringLiteral("DX"));
    QCOMPARE(annotations.markCount(), 0);
    QCOMPARE(data.projectionInverted(), projectionInverted);
    QCOMPARE(data.projectionPairInverted(), projectionPairInverted);
#else
    QSKIP("The MinGW UI compatibility build does not link the medical backend.");
#endif
}

void CoreTests::casePackageRoundTripWhenConfigured()
{
#if CT_ENABLE_MEDICAL_BACKEND
    const QString dicomDirectory = qEnvironmentVariable("CT_UI_TEST_DICOM_DIR");
    if (dicomDirectory.isEmpty())
        QSKIP("Set CT_UI_TEST_DICOM_DIR to run the case-package integration test.");

    MedicalDataController source;
    AnnotationController sourceAnnotations;
    sourceAnnotations.setMedicalData(&source);
    QVERIFY2(source.importDicom(QUrl::fromLocalFile(dicomDirectory)),
             qPrintable(source.errorMessage()));
    if (!source.loaded()) {
        int ctIndex = -1;
        for (const QVariant &entry : source.seriesChoices()) {
            const QVariantMap choice = entry.toMap();
            if (choice.value(QStringLiteral("modality")).toString() == QStringLiteral("CT")) {
                ctIndex = choice.value(QStringLiteral("index")).toInt();
                break;
            }
        }
        QVERIFY(ctIndex >= 0);
        QVERIFY2(source.selectSeries(ctIndex), qPrintable(source.errorMessage()));
    }
    QVERIFY(source.volumeData());
    const auto volume = source.volumeSnapshot();
    QVERIFY(volume);
    QVERIFY2(source.applyThreshold(-1000.0, 500.0), qPrintable(source.errorMessage()));
    const qint64 sourceVoxelCount = source.segmentationVoxelCount();
    QVERIFY(sourceVoxelCount > 0);
    sourceAnnotations.setToolType(AnnotationController::LengthTool);
    QVERIFY(sourceAnnotations.addWorldPoint(0.0, 0.0, 0.0));
    QVERIFY(sourceAnnotations.addWorldPoint(10.0, 0.0, 0.0));
    QCOMPARE(sourceAnnotations.measureCount(), 1);

    QTemporaryDir exportRoot;
    QVERIFY(exportRoot.isValid());
    QVERIFY2(source.exportCasePackage(QUrl::fromLocalFile(exportRoot.path()),
                                      sourceAnnotations.items()),
             qPrintable(source.errorMessage()));
    const QFileInfoList packages = QDir(exportRoot.path()).entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot);
    QCOMPARE(packages.size(), 1);
    const QDir package(packages.front().absoluteFilePath());
    QVERIFY(QFileInfo::exists(package.filePath(QStringLiteral("case.json"))));
    QVERIFY(QFileInfo::exists(package.filePath(QStringLiteral("segmentation/mask.raw"))));
    QVERIFY(!QDir(package.filePath(QStringLiteral("dicom"))).entryList(
        QDir::Files | QDir::NoDotAndDotDot).isEmpty());

    MedicalDataController restored;
    AnnotationController restoredAnnotations;
    restoredAnnotations.setMedicalData(&restored);
    QVERIFY2(restored.importDicom(QUrl::fromLocalFile(package.absolutePath())),
             qPrintable(restored.errorMessage()));
    QVERIFY(restored.loaded());
    QVERIFY(restored.segmentationAvailable());
    QCOMPARE(restored.segmentationVoxelCount(), sourceVoxelCount);
    QCOMPARE(restoredAnnotations.measureCount(), 1);
    QVERIFY(!restored.operationHistory().isEmpty());
#else
    QSKIP("The medical backend is required for case-package integration.");
#endif
}

void CoreTests::realLidcRegionGrowingPerformance()
{
#if CT_ENABLE_MEDICAL_BACKEND
    const QString root = qEnvironmentVariable("CT_UI_TEST_LIDC_ROOT");
    if (root.isEmpty())
        QSKIP("Set CT_UI_TEST_LIDC_ROOT to benchmark region growing on real CT data.");

    MedicalDataController data;
    QVERIFY2(data.importDicom(QUrl::fromLocalFile(root)), qPrintable(data.errorMessage()));
    int ctIndex = -1;
    for (const QVariant &entry : data.seriesChoices()) {
        const QVariantMap choice = entry.toMap();
        if (choice.value(QStringLiteral("modality")) == QStringLiteral("CT")) {
            ctIndex = choice.value(QStringLiteral("index")).toInt();
            break;
        }
    }
    QVERIFY(ctIndex >= 0);
    QVERIFY2(data.selectSeries(ctIndex), qPrintable(data.errorMessage()));
    const auto volume = data.volumeSnapshot();
    QVERIFY(volume);

    const int seedX = std::min(148, volume->dimensions[0] - 1);
    const int seedY = std::min(229, volume->dimensions[1] - 1);
    const int seedZ = std::min(43, volume->dimensions[2] - 1);
    QVERIFY(data.setRegionGrowingSeed(seedX, seedY, seedZ));
    const int seedValue = data.regionGrowingSeedValue();
    QElapsedTimer timer;
    timer.start();
    QVERIFY2(data.applyRegionGrowing(seedX, seedY, seedZ,
                                     seedValue - 100, seedValue + 100, true),
             qPrintable(data.errorMessage()));
    const qint64 elapsedMs = timer.elapsed();
    qInfo() << "Real LIDC 26-neighbour region growing:" << elapsedMs << "ms for"
            << data.segmentationVoxelCount() << "voxels";
    QVERIFY2(elapsedMs < 15000, "Region growing exceeded the interactive response budget.");
    QVERIFY(data.segmentationVoxelCount() > 0);
#else
    QSKIP("The MinGW UI compatibility build does not link the medical backend.");
#endif
}

void CoreTests::mixedRootLoadsUnsignedDx()
{
#if CT_ENABLE_MEDICAL_BACKEND
    const QString root = qEnvironmentVariable("CT_UI_TEST_XRAY_ROOT");
    if (root.isEmpty())
        QSKIP("Set CT_UI_TEST_XRAY_ROOT to test mixed extensionless DICOM media.");

    MedicalDataController data;
    QVERIFY2(data.importDicom(QUrl::fromLocalFile(root)), qPrintable(data.errorMessage()));
    QVERIFY(data.seriesChoices().size() >= 14);

    for (const QVariant &entry : data.seriesChoices()) {
        const QVariantMap choice = entry.toMap();
        qInfo().noquote() << QStringLiteral("X-ray root candidate %1: %2 / %3 / %4 / %5")
                                .arg(choice.value(QStringLiteral("index")).toInt())
                                .arg(choice.value(QStringLiteral("patientId")).toString())
                                .arg(choice.value(QStringLiteral("modality")).toString())
                                .arg(choice.value(QStringLiteral("description")).toString())
                                .arg(choice.value(QStringLiteral("dimensions")).toString());
    }

    int splitSpineSeriesCount = 0;
    for (const QVariant &entry : data.seriesChoices()) {
        const QVariantMap choice = entry.toMap();
        if (choice.value(QStringLiteral("modality")).toString() != QStringLiteral("CT")
            || choice.value(QStringLiteral("description")).toString()
                   != QStringLiteral("Spine 2.0"))
            continue;
        const int index = choice.value(QStringLiteral("index")).toInt();
        QVERIFY2(data.selectSeries(index), qPrintable(data.errorMessage()));
        ++splitSpineSeriesCount;
    }
    // Both patients contain a Series UID with mixed matrices.  The scanner
    // must expose each homogeneous matrix as a separately loadable candidate.
    QCOMPARE(splitSpineSeriesCount, 4);

    int dxIndex = -1;
    bool damagedLabelSampleFound = false;
    for (const QVariant &entry : data.seriesChoices()) {
        const QVariantMap choice = entry.toMap();
        if (choice.value(QStringLiteral("modality")) == QStringLiteral("DX")) {
            if (dxIndex < 0 || choice.value(QStringLiteral("patientId"))
                                     == QStringLiteral("12117230"))
                dxIndex = choice.value(QStringLiteral("index")).toInt();
            if (choice.value(QStringLiteral("patientId")) == QStringLiteral("12117230")) {
                damagedLabelSampleFound = true;
                break;
            }
        }
    }
    QVERIFY(dxIndex >= 0);
    QVERIFY2(data.selectSeries(dxIndex), qPrintable(data.errorMessage()));
    QVERIFY(data.loaded());
    QVERIFY(!data.volumeData());
    QCOMPARE(data.modality(), QStringLiteral("DX"));
    QVERIFY(data.windowWidth() > 1000.0);
    QVERIFY(data.volumeSnapshot()->dimensions[0] > 1000);
    QVERIFY(data.volumeSnapshot()->dimensions[1] > 1000);
    QVERIFY(data.projectionData());
    QVERIFY(data.pairedProjectionAvailable());
    if (damagedLabelSampleFound)
        QCOMPARE(data.studyDescription(), QStringLiteral("DX 检查"));
    QVERIFY(!data.projectionViewLabel().isEmpty());
    QVERIFY(!data.projectionPairViewLabel().isEmpty());

    QTemporaryDir exportDirectory;
    QVERIFY(exportDirectory.isValid());
    QVERIFY2(data.exportDicomCopy(QUrl::fromLocalFile(exportDirectory.path())),
             qPrintable(data.errorMessage()));
    QCOMPARE(QDir(exportDirectory.path()).entryList(QDir::Files).size(), 1);
#else
    QSKIP("The MinGW UI compatibility build does not link the medical backend.");
#endif
}

QTEST_GUILESS_MAIN(CoreTests)

#include "core_tests.moc"
