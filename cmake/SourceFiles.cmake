include_guard(GLOBAL)

# 核心库在主程序和测试之间共享，避免医学控制器被重复编译。
set(CT_UI_CORE_COMMON_SOURCES
    src/application/workflowcontroller.cpp
    src/application/workflowcontroller.h
    src/annotation/annotationcontroller.cpp
    src/annotation/annotationcontroller.h
    src/markups/markupsmetrics.cpp
    src/markups/markupsmetrics.h
    src/markups/markupsnode.cpp
    src/markups/markupsnode.h
    src/markups/markupspicker.cpp
    src/markups/markupspicker.h
    src/markups/markupsscene.cpp
    src/markups/markupsscene.h
    src/dicom/dicompresentation.h
    src/dicom/dicomtextcodec.cpp
    src/dicom/dicomtextcodec.h
    src/dicom/medicaldatacontroller.h
)

# 渲染层只属于桌面程序，不让核心测试引入 QQuickVTKItem 和 VTK。
set(CT_UI_RENDERING_COMMON_SOURCES
    src/rendering/medicalviewportitem.h
)

# MSVC 医学后端：真实 DICOM/ITK/VTK 实现。
set(CT_UI_MEDICAL_CORE_SOURCES
    src/dicom/medicaldatacontroller.cpp
)
set(CT_UI_MEDICAL_RENDERING_SOURCES
    src/rendering/medicalviewportitem.cpp
)

# MinGW 兼容模式：保持 QML API 一致，但不跨 ABI 链接 MSVC 医学库。
set(CT_UI_COMPATIBILITY_CORE_SOURCES
    src/dicom/medicaldatacontroller_stub.cpp
)
set(CT_UI_COMPATIBILITY_RENDERING_SOURCES
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
    qml/workstation/MarkupsTreePanel.qml
)
