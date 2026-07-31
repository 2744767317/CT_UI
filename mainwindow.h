#pragma once

#include <QMainWindow>

class QLabel;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTimer;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    enum class WorkflowState {
        NoPatient,
        PatientConfirmed,
        ProtocolSelected,
        Positioning,
        Ready,
        Acquiring,
        Reviewing,
        Locked
    };

    QWidget *buildTopBar();
    QWidget *buildPatientRail();
    QWidget *buildCenterWorkspace();
    QWidget *buildContextRail();
    QWidget *buildWorklistPage();
    QWidget *buildProtocolPage();
    QWidget *buildPositioningPage();
    QWidget *buildReadyPage();
    QWidget *buildAcquisitionPage();
    QWidget *buildViewerPage();
    QWidget *buildLockedPage();
    QWidget *buildContextPage(const QString &title, const QString &body);

    void setState(WorkflowState state, const QString &auditMessage = {});
    void updateWorkflowUi();
    void appendAudit(const QString &message);
    void beginAcquisition();
    void toggleInterlock();

    WorkflowState m_state = WorkflowState::NoPatient;
    WorkflowState m_stateBeforeLock = WorkflowState::NoPatient;
    bool m_patientVerified = false;
    bool m_protocolLocked = false;
    int m_acquisitionProgress = 0;

    QLabel *m_stateLabel = nullptr;
    QLabel *m_headerHint = nullptr;
    QLabel *m_patientName = nullptr;
    QLabel *m_patientMeta = nullptr;
    QLabel *m_protocolValue = nullptr;
    QLabel *m_exposureBadge = nullptr;
    QLabel *m_contextTitle = nullptr;
    QLabel *m_auditLabel = nullptr;
    QLabel *m_acquisitionPercent = nullptr;
    QProgressBar *m_acquisitionBar = nullptr;
    QPushButton *m_primaryAction = nullptr;
    QPushButton *m_interlockButton = nullptr;
    QStackedWidget *m_pages = nullptr;
    QStackedWidget *m_contextPages = nullptr;
    QTableWidget *m_worklist = nullptr;
    QTimer *m_acquisitionTimer = nullptr;
};
