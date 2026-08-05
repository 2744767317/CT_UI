include_guard(GLOBAL)

# 两种编译模式共享接口头文件和工作流状态机。
set(CT_UI_COMMON_SOURCES
    src/application/workflowcontroller.cpp
    src/application/workflowcontroller.h
    src/dicom/medicaldatacontroller.h
    src/rendering/medicalviewportitem.h
)

# MSVC 医学后端：真实 DICOM/ITK/VTK 实现。
set(CT_UI_MEDICAL_SOURCES
    src/dicom/medicaldatacontroller.cpp
    src/rendering/medicalviewportitem.cpp
)

# MinGW 兼容模式：保持 QML API 一致，但不跨 ABI 链接 MSVC 医学库。
set(CT_UI_COMPATIBILITY_SOURCES
    src/dicom/medicaldatacontroller_stub.cpp
    src/rendering/medicalviewportitem_stub.cpp
)

# QML 文件显式列出，新增页面或组件时必须同步加入此清单。
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
