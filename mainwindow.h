#pragma once

#include <QMainWindow>

class AcquisitionPanel;
class InspectorPanel;
class QLabel;
class QPushButton;
class QSplitter;
class QStackedWidget;
class ViewportGrid;
class QWidget;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    QWidget *buildTopBar();
    QWidget *buildWorkflowBar();
    QWidget *buildGlobalToolbar();
    QWidget *buildImagingWorkspace();
    QWidget *buildStatusBar();
    void setWorkflowPage(int index, bool advance = false);

    AcquisitionPanel *m_acquisition = nullptr;
    InspectorPanel *m_inspector = nullptr;
    ViewportGrid *m_viewports = nullptr;
    QSplitter *m_splitter = nullptr;
    QStackedWidget *m_pages = nullptr;
    QWidget *m_globalToolbar = nullptr;
    QWidget *m_bottomStatusBar = nullptr;
    QLabel *m_coordinateStatus = nullptr;
    QLabel *m_workflowStatus = nullptr;
    QPushButton *m_stepButtons[4] = {nullptr, nullptr, nullptr, nullptr};
    int m_currentPage = 0;
    int m_maxReachedPage = 0;
};
