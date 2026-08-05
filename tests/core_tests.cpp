#include "src/application/workflowcontroller.h"
#include "src/dicom/medicaldatacontroller.h"

#include <QDir>
#include <QFileInfoList>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVariantMap>
#include <QtTest>

#include <algorithm>

class CoreTests final : public QObject
{
    Q_OBJECT

private slots:
    void workflowGuardsFutureSteps();
    void demoVolumeAndThresholdAreAvailable();
    void volumeNodeLifecycle();
    void regionGrowingSeedStateAndValidation();
    void realDicomRoundTripWhenConfigured();
    void recursiveLidcRootLoadsCtAndDx();
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

    QVERIFY(!data.applyRegionGrowingFromSeed(-1000.0, -500.0));
    QVERIFY(data.errorMessage().contains(QStringLiteral("45 HU")));
    QVERIFY(data.applyRegionGrowingFromSeed(40.0, 60.0));
    QVERIFY(data.segmentationAvailable());

    data.clearRegionGrowingSeed();
    QVERIFY(!data.regionGrowingSeedValid());
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
    data.importDicomAsync(QUrl::fromLocalFile(root));
    QTRY_VERIFY_WITH_TIMEOUT(!data.busy(), 120000);
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

    QVERIFY2(data.selectSeries(ctIndex), qPrintable(data.errorMessage()));
    QVERIFY(data.loaded());
    QVERIFY(data.volumeData());
    QCOMPARE(data.modality(), QStringLiteral("CT"));
    QCOMPARE(data.patientId(), QStringLiteral("LIDC-IDRI-0001"));

    QVERIFY2(data.selectSeries(dxIndex), qPrintable(data.errorMessage()));
    QVERIFY(data.loaded());
    QVERIFY(!data.volumeData());
    QCOMPARE(data.modality(), QStringLiteral("DX"));
    QCOMPARE(data.volumeSnapshot()->dimensions[2], 1);
    QCOMPARE(data.volumeNodes().size(), 2);
    QVERIFY(data.selectVolume(0));
    QCOMPARE(data.modality(), QStringLiteral("CT"));
    QVERIFY(data.selectVolume(1));
    QCOMPARE(data.modality(), QStringLiteral("DX"));
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

    int dxIndex = -1;
    for (const QVariant &entry : data.seriesChoices()) {
        const QVariantMap choice = entry.toMap();
        if (choice.value(QStringLiteral("modality")) == QStringLiteral("DX")) {
            dxIndex = choice.value(QStringLiteral("index")).toInt();
            break;
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
