include_guard(GLOBAL)

set(CT_UI_COMMON_SOURCES
    src/application/workflowcontroller.cpp
    src/application/workflowcontroller.h
    src/dicom/medicaldatacontroller.h
    src/rendering/medicalviewportitem.h
)

set(CT_UI_MEDICAL_SOURCES
    src/dicom/medicaldatacontroller.cpp
    src/rendering/medicalviewportitem.cpp
)

set(CT_UI_COMPATIBILITY_SOURCES
    src/dicom/medicaldatacontroller_stub.cpp
    src/rendering/medicalviewportitem_stub.cpp
)

set(CT_UI_QML_FILES
    qml/Main.qml
    qml/theme/Theme.qml
    qml/components/ActionButton.qml
    qml/components/StatusPill.qml
    qml/components/SeriesSelectionDialog.qml
    qml/components/ViewportPane.qml
    qml/pages/PatientPage.qml
    qml/pages/SafetyPage.qml
    qml/pages/ScanRangePage.qml
    qml/pages/WorkstationPage.qml
    qml/workstation/DataPanel.qml
    qml/workstation/InspectorPanel.qml
)
