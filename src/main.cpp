#include "src/annotation/annotationcontroller.h"
#include "src/application/workflowcontroller.h"
#include "src/dicom/medicaldatacontroller.h"
#include "src/rendering/medicalviewportitem.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
#if CT_ENABLE_MEDICAL_BACKEND
    // QQuickVTKItem 必须在 QGuiApplication 创建前选定图形 API，避免 Qt Quick 与 VTK
    // 分别创建不兼容的渲染后端。
    QQuickVTKItem::setGraphicsApi();
#endif
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("光索科技"));
    QCoreApplication::setApplicationName(QStringLiteral("CT_UI"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.3.0"));

    qmlRegisterType<MedicalViewportItem>("GuangSuo.CT.Rendering", 1, 0, "MedicalViewport");
    qmlRegisterUncreatableType<AnnotationController>(
        "GuangSuo.CT", 1, 0, "AnnotationTool",
        QStringLiteral("Access via annotationController context property"));

    WorkflowController workflow;
    MedicalDataController medicalData;
    AnnotationController annotationController;
    annotationController.setMedicalData(&medicalData);

    // 命令行入口用于自动化冒烟测试和开发调试，不参与正式患者工作流。
    const QStringList arguments = QCoreApplication::arguments();
    if (arguments.contains(QStringLiteral("--demo")))
        medicalData.loadDemoVolume();
    const qsizetype dicomIndex = arguments.indexOf(QStringLiteral("--dicom"));
    if (dicomIndex >= 0 && dicomIndex + 1 < arguments.size()) {
        medicalData.importDicom(QUrl::fromLocalFile(arguments.at(dicomIndex + 1)));
        if (!medicalData.loaded()) {
            int fallbackIndex = -1;
            for (const QVariant &entry : medicalData.seriesChoices()) {
                const QVariantMap choice = entry.toMap();
                const int index = choice.value(QStringLiteral("index")).toInt();
                if (fallbackIndex < 0)
                    fallbackIndex = index;
                if (choice.value(QStringLiteral("modality")).toString() == QStringLiteral("CT")) {
                    fallbackIndex = index;
                    break;
                }
            }
            if (fallbackIndex >= 0)
                medicalData.selectSeries(fallbackIndex);
        }
    }
    if (arguments.contains(QStringLiteral("--workstation"))) {
        workflow.advance();
        workflow.advance();
        workflow.advance();
    }
    QQmlApplicationEngine engine;
    // QML 只持有控制器引用；DICOM 像素、ITK 处理和 VTK 管线均由 C++ 层管理。
    engine.rootContext()->setContextProperty(QStringLiteral("workflowController"), &workflow);
    engine.rootContext()->setContextProperty(QStringLiteral("medicalData"), &medicalData);
    engine.rootContext()->setContextProperty(QStringLiteral("annotationController"),
                                             &annotationController);
    engine.rootContext()->setContextProperty(
        QStringLiteral("medicalBackendEnabled"),
        QVariant::fromValue(static_cast<bool>(CT_ENABLE_MEDICAL_BACKEND)));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("GuangSuo.CT"), QStringLiteral("Main"));
    return app.exec();
}
