#pragma once

#include <QDialog>

class QTabWidget;

class DicomDialog final : public QDialog
{
public:
    enum class InitialPage { Import, Pacs, Queue };

    explicit DicomDialog(InitialPage initialPage, QWidget *parent = nullptr);

private:
    QWidget *buildImportPage();
    QWidget *buildPacsPage();
    QWidget *buildQueuePage();

    QTabWidget *m_tabs = nullptr;
};
