#pragma once

#include <QFrame>

#include <functional>

class QLabel;
class QPushButton;
class QStackedWidget;
class QTabWidget;
class QProgressBar;
class QTimer;

class AcquisitionPanel final : public QFrame
{
public:
    explicit AcquisitionPanel(QWidget *parent = nullptr);
    void setWorkflowChangedCallback(std::function<void(const QString &, const QString &)> callback);
    void setEditingMode();

private:
    QWidget *buildPreparationPage();
    QWidget *buildDataPage();
    void advancePreparation();

    int m_preparationStage = 0;
    QLabel *m_stageLabel = nullptr;
    QLabel *m_stageHint = nullptr;
    QLabel *m_positionStatus = nullptr;
    QLabel *m_progressLabel = nullptr;
    QPushButton *m_primaryButton = nullptr;
    QTabWidget *m_tabs = nullptr;
    QProgressBar *m_progress = nullptr;
    QTimer *m_timer = nullptr;
    std::function<void(const QString &, const QString &)> m_workflowChangedCallback;
};
