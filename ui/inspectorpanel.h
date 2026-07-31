#pragma once

#include <QFrame>

class QLabel;
class QTabWidget;

class InspectorPanel final : public QFrame
{
public:
    explicit InspectorPanel(QWidget *parent = nullptr);
    void setActiveView(const QString &viewName);
    void showModule(int index);

private:
    QWidget *buildDisplayPage();
    QWidget *buildReconstructionPage();
    QWidget *buildSegmentationPage();
    QWidget *buildMeasurementPage();
    QWidget *buildDicomPage();

    QLabel *m_titleLabel = nullptr;
    QLabel *m_metaLabel = nullptr;
    QTabWidget *m_tabs = nullptr;
};
