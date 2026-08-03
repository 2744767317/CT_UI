#include "src/application/workflowcontroller.h"
#include "src/dicom/medicaldatacontroller.h"

#include <QDir>
#include <QFileInfoList>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

class CoreTests final : public QObject
{
    Q_OBJECT

private slots:
    void workflowGuardsFutureSteps();
    void demoVolumeAndThresholdAreAvailable();
    void realDicomRoundTripWhenConfigured();
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

QTEST_APPLESS_MAIN(CoreTests)

#include "core_tests.moc"
