#include "src/application/workflowcontroller.h"
#include "src/dicom/medicaldatacontroller.h"
#include "src/rendering/medicalviewportitem.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QTimer>

int main(int argc, char *argv[])
{
#if CT_ENABLE_MEDICAL_BACKEND
    QQuickVTKItem::setGraphicsApi();
#endif
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("光索科技"));
    QCoreApplication::setApplicationName(QStringLiteral("CT_UI"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.3.0"));

    qmlRegisterType<MedicalViewportItem>("GuangSuo.CT.Rendering", 1, 0, "MedicalViewport");

    WorkflowController workflow;
    MedicalDataController medicalData;
    const QStringList arguments = QCoreApplication::arguments();
    if (arguments.contains(QStringLiteral("--demo")))
        medicalData.loadDemoVolume();
    if (arguments.contains(QStringLiteral("--threshold-demo")))
        medicalData.applyThreshold(300.0, 2500.0);
    if (arguments.contains(QStringLiteral("--workstation"))) {
        workflow.advance();
        workflow.advance();
        workflow.advance();
    }
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("workflowController"), &workflow);
    engine.rootContext()->setContextProperty(QStringLiteral("medicalData"), &medicalData);
    engine.rootContext()->setContextProperty(
        QStringLiteral("medicalBackendEnabled"),
        QVariant::fromValue(static_cast<bool>(CT_ENABLE_MEDICAL_BACKEND)));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("GuangSuo.CT"), QStringLiteral("Main"));

    const qsizetype screenshotIndex = arguments.indexOf(QStringLiteral("--screenshot"));
    if (screenshotIndex >= 0 && screenshotIndex + 1 < arguments.size()) {
        const QString screenshotPath = arguments.at(screenshotIndex + 1);
        QTimer::singleShot(3000, &app, [&engine, screenshotPath] {
            const auto windows = engine.rootObjects();
            auto *window = windows.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(windows.constFirst());
            const auto result = window ? window->contentItem()->grabToImage() : nullptr;
            if (!result) {
                qCritical("Unable to save UI screenshot");
                QCoreApplication::exit(2);
                return;
            }
            QObject::connect(result.data(), &QQuickItemGrabResult::ready, qApp,
                             [result, screenshotPath] {
                if (!result->saveToFile(screenshotPath)) {
                    qCritical("Unable to save UI screenshot");
                    QCoreApplication::exit(2);
                    return;
                }
                QCoreApplication::exit(0);
            });
        });
    }
    return app.exec();
}
