#include "appstyle.h"

namespace AppStyle {

QString styleSheet()
{
    return QStringLiteral(R"(
        * { font-family: "Microsoft YaHei UI"; font-size: 13px; color: #d7dcdf; }
        QMainWindow, QWidget#appRoot { background: #111518; }

        QWidget#topBar { background: #202529; border-bottom: 1px solid #353d42; }
        QLabel#brand { color: #f1f3f4; font-size: 21px; font-weight: 700; }
        QLabel#brandSub { color: #829097; font-size: 11px; }
        QLabel#activeStudy { color: #f0a052; font-size: 16px; font-weight: 700; }
        QLabel#activeStudyMeta { color: #919ca2; font-size: 11px; }
        QWidget#topStatusItem { background: #292f33; border: 1px solid #3a4348; border-radius: 3px; min-width: 108px; }
        QWidget#topStatusItem[tone="accent"] { border-color: #765437; }
        QLabel#topStatusCaption { color: #7e8a90; font-size: 10px; }
        QLabel#topStatusValue { color: #d9dee0; font-size: 12px; font-weight: 600; }
        QToolButton#topIconButton { min-width: 44px; min-height: 44px; background: #292f33; border: 1px solid #424b50; border-radius: 3px; }

        QWidget#globalToolbar { background: #1b2024; border-bottom: 1px solid #333a3f; }
        QToolButton#globalTool { min-height: 36px; padding: 0 9px; background: transparent; border: 1px solid transparent; border-radius: 3px; }
        QToolButton#globalTool:hover { background: #2a3034; border-color: #424a4f; }
        QToolButton#globalTool:checked { color: #f0a052; background: #302a25; border-color: #c87832; }
        QFrame#toolbarSeparator { color: #3a4247; max-width: 1px; margin: 5px 6px; }
        QLabel#toolbarLabel { color: #7f8b91; }
        QComboBox#layoutSelector { min-height: 34px; }

        QWidget#workflowBar { background: #1b2024; border-bottom: 1px solid #343c41; }
        QPushButton#workflowStepButton { background: transparent; color: #758188; border: 0; border-bottom: 3px solid #343c41; min-height: 42px; font-size: 13px; font-weight: 600; }
        QPushButton#workflowStepButton[state="done"] { color: #a8b2b7; border-bottom-color: #637c6c; }
        QPushButton#workflowStepButton[state="active"] { color: #f0a052; border-bottom-color: #d98137; font-weight: 800; }
        QPushButton#workflowStepButton:disabled { color: #596269; }

        QWidget#workflowPage { background: #111518; }
        QLabel#workflowEyebrow { color: #d8863d; font-size: 11px; font-weight: 800; }
        QLabel#workflowTitle { color: #f0f2f3; font-size: 25px; font-weight: 800; }
        QLabel#workflowSubtitle { color: #8f9aa0; font-size: 13px; }
        QFrame#workflowSidePanel { background: #1a1f23; border-left: 1px solid #343c41; }
        QLabel#workflowPatientName { color: #f0f2f3; font-size: 24px; font-weight: 800; }
        QWidget#workflowInfoRow { border-bottom: 1px solid #30373b; }
        QLabel#infoName { color: #8e999f; }
        QLabel#infoValue { color: #e4e8ea; font-weight: 700; }
        QFrame#deviceCheckRow { background: #1b2024; border: 1px solid #343c41; border-radius: 3px; }
        QLabel#okPill { color: #a8c2af; background: #26332a; border: 1px solid #465c4d; border-radius: 3px; padding: 5px 10px; font-weight: 700; }
        QLabel#rangeValue { color: #f0a052; font-size: 16px; font-weight: 800; }
        QWidget#scanRangeCanvas { background: #070a0c; border: 1px solid #343c41; }

        QSplitter#workspaceSplitter { background: #0c1012; }
        QSplitter#workspaceSplitter::handle { background: #343c41; }
        QFrame#sidePanel { background: #1a1f23; }
        QLabel#panelEyebrow { color: #78858c; font-size: 10px; font-weight: 700; }
        QLabel#panelTitle { color: #edf0f1; font-size: 18px; font-weight: 700; }
        QLabel#patientName { color: #f0f2f3; font-size: 17px; font-weight: 700; }
        QLabel#mutedText { color: #8f9aa0; font-size: 11px; }
        QLabel#sectionTitle { color: #cfd5d8; font-weight: 700; padding-top: 2px; }
        QLabel#valueLabel { color: #edf0f1; font-weight: 700; }
        QLabel#okText { color: #82a58e; font-size: 11px; font-weight: 600; }
        QLabel#warningText { color: #e09a59; font-size: 11px; font-weight: 600; }
        QLabel#stepBadge { min-width: 24px; max-width: 24px; min-height: 24px; max-height: 24px; border-radius: 12px; background: #343b40; color: #9ca6ab; qproperty-alignment: AlignCenter; }
        QLabel#stepBadge[active="true"] { background: #d67c30; color: #16191b; font-weight: 800; }
        QFrame#sectionLine { color: #333b40; max-height: 1px; background: #333b40; border: 0; }
        QFrame#studySummary { background: #20262a; border: 1px solid #394247; border-radius: 3px; }
        QFrame#readinessBox { background: #202822; border: 1px solid #405247; border-left: 3px solid #6f947c; }

        QTabWidget#sideTabs::pane { border: 0; }
        QTabWidget#sideTabs QTabBar::tab { background: #20262a; color: #879298; min-height: 38px; min-width: 100px; border-bottom: 2px solid #343c41; }
        QTabWidget#sideTabs QTabBar::tab:selected { color: #efa057; border-bottom-color: #d77f33; }
        QTabWidget#inspectorTabs::pane { border: 0; }
        QTabWidget#inspectorTabs QTabBar::tab { background: #20262a; color: #89949a; min-height: 36px; padding: 0 9px; border-bottom: 2px solid #343c41; }
        QTabWidget#inspectorTabs QTabBar::tab:selected { color: #efa057; border-bottom-color: #d77f33; }

        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox { background: #252b2f; border: 1px solid #414a4f; border-radius: 3px; min-height: 36px; padding: 0 8px; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color: #d47b31; }
        QPushButton#secondaryButton { background: #292f33; border: 1px solid #454e53; border-radius: 3px; min-height: 38px; padding: 0 10px; }
        QPushButton#secondaryButton:hover { background: #32393e; border-color: #68747a; }
        QPushButton#secondaryButton:checked { color: #f1a158; border-color: #d47b31; background: #312a25; }
        QPushButton#primaryButton { background: #dd8335; color: #171a1c; border: 1px solid #ed994e; border-radius: 3px; min-height: 48px; font-size: 14px; font-weight: 800; }
        QPushButton#primaryButton:hover { background: #ea9145; }
        QPushButton#toolTile { background: #282e32; border: 1px solid #41494e; border-radius: 3px; min-height: 44px; padding: 0 7px; }
        QPushButton#toolTile:hover { border-color: #707b81; }
        QPushButton#toolTile:checked { color: #f1a158; border: 1px solid #d47b31; background: #332c26; }

        QCheckBox { min-height: 30px; spacing: 8px; }
        QCheckBox::indicator { width: 18px; height: 18px; background: #22282c; border: 1px solid #5b656a; }
        QCheckBox::indicator:checked { background: #cd7830; border-color: #ea984d; }
        QSlider::groove:horizontal { height: 4px; background: #3b4449; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: #8c633f; }
        QSlider::handle:horizontal { width: 15px; margin: -6px 0; background: #d6dbdd; border: 1px solid #7f898e; border-radius: 7px; }
        QProgressBar { background: #252b2f; border: 1px solid #3d464b; border-radius: 2px; min-height: 8px; max-height: 8px; }
        QProgressBar::chunk { background: #d27a31; }

        QTreeWidget { background: #171c20; border: 1px solid #343c41; outline: 0; }
        QTreeWidget::item { min-height: 30px; padding: 2px 4px; }
        QTreeWidget::item:selected { background: #3a322b; color: #f0f2f3; }
        QHeaderView::section { background: #252b2f; color: #939da2; border: 0; border-bottom: 1px solid #3a4247; padding: 6px; }
        QTableWidget { background: #171c20; alternate-background-color: #1c2226; border: 1px solid #343c41; gridline-color: #2d3438; selection-background-color: #3d342c; selection-color: #f1f2f3; outline: 0; }
        QTableWidget::item { padding: 8px 6px; border-bottom: 1px solid #2c3337; }

        QDialog#dicomDialog { background: #171c20; }
        QLabel#dialogTitle { color: #f0f2f3; font-size: 22px; font-weight: 700; }
        QTabWidget#dialogTabs::pane { border: 0; }
        QTabWidget#dialogTabs QTabBar::tab { background: #22282c; color: #8d989e; min-width: 120px; min-height: 40px; border-bottom: 2px solid #343c41; }
        QTabWidget#dialogTabs QTabBar::tab:selected { color: #efa057; border-bottom-color: #d77f33; }
        QFrame#dropZone { background: #20262a; border: 1px dashed #59656b; border-radius: 3px; min-height: 128px; }
        QDialog#safetyLockDialog { background: #171c20; }
        QLabel#dangerTitle { color: #e48a7b; font-size: 24px; font-weight: 800; }
        QLabel#dangerCode { color: #bb6a5d; font-size: 13px; font-weight: 700; }
        QLabel#dangerText { color: #dc7a6a; font-size: 11px; font-weight: 700; }
        QFrame#dangerCallout { background: #292020; border: 1px solid #5e3733; border-left: 4px solid #c65f50; }
        QFrame#deviceStateRow { background: #20262a; border: 1px solid #394247; }
        QFrame#deviceStateRow[tone="danger"] { background: #282020; border-color: #65403a; }
        QLabel#recoveryStep { background: #20262a; border-left: 2px solid #59656b; padding: 8px 10px; color: #cfd5d8; }

        QWidget#viewportGrid { background: #070a0c; }
        QWidget#viewport { background: #050708; border: 1px solid #2e3539; }
        QWidget#viewport[selected="true"] { border: 2px solid #bd7333; }

        QScrollArea { background: transparent; border: 0; }
        QScrollArea > QWidget > QWidget { background: transparent; }
        QScrollBar:vertical { background: #1d2226; width: 10px; }
        QScrollBar::handle:vertical { background: #465056; min-height: 28px; }

        QWidget#bottomStatusBar { background: #1b2024; border-top: 1px solid #343c41; }
        QLabel#statusText { color: #7f8a90; font-size: 10px; }
        QLabel#statusStrong { color: #cdd3d6; font-size: 10px; font-weight: 700; }
        QLabel#statusOk { color: #80a18a; font-size: 10px; font-weight: 700; }
    )");
}

} // namespace AppStyle
