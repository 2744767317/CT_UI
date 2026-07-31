#include "mainwindow.h"
#include "ui/dicomdialog.h"
#include "ui/safetylockdialog.h"

#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QToolButton>

#include <iostream>

namespace {

bool require(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

int visibleViewportCount(MainWindow &window)
{
    int count = 0;
    const auto viewports = window.findChildren<QWidget *>("viewport");
    for (const auto *viewport : viewports) {
        if (viewport->isVisible())
            ++count;
    }
    return count;
}

QPushButton *buttonByAccessibleName(MainWindow &window, const QString &name)
{
    const auto buttons = window.findChildren<QPushButton *>();
    for (auto *button : buttons) {
        if (button->accessibleName() == name)
            return button;
    }
    return nullptr;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    QApplication::processEvents();

    bool passed = true;
    auto *workflowPages = window.findChild<QStackedWidget *>("workflowPages");
    passed &= require(workflowPages && workflowPages->count() == 4, "workflow exposes four pages");
    passed &= require(workflowPages && workflowPages->currentIndex() == 0, "workflow starts at patient confirmation");
    const QStringList nextButtons = {"patientNextButton", "safetyNextButton", "rangeNextButton"};
    for (int index = 0; index < nextButtons.size(); ++index) {
        auto *next = buttonByAccessibleName(window, nextButtons.at(index));
        passed &= require(next != nullptr, "workflow primary action exists");
        if (next) {
            next->click();
            QApplication::processEvents();
            passed &= require(workflowPages->currentIndex() == index + 1, "workflow advances to next page");
        }
    }
    auto *globalToolbar = window.findChild<QWidget *>("globalToolbar");
    passed &= require(globalToolbar && globalToolbar->isVisible(), "imaging toolbar appears only in editing page");

    auto *layoutSelector = window.findChild<QComboBox *>("layoutSelector");
    passed &= require(layoutSelector != nullptr, "layout selector exists");
    if (layoutSelector) {
        const int expectedCounts[] = {4, 4, 1, 2};
        for (int index = 0; index < 4; ++index) {
            layoutSelector->setCurrentIndex(index);
            QApplication::processEvents();
            passed &= require(visibleViewportCount(window) == expectedCounts[index], "layout viewport count is correct");
        }
        layoutSelector->setCurrentIndex(0);
    }

    QToolButton *measurementButton = nullptr;
    const auto toolbarButtons = window.findChildren<QToolButton *>("globalTool");
    for (auto *button : toolbarButtons) {
        if (button->text() == QStringLiteral("测量")) {
            measurementButton = button;
            break;
        }
    }
    passed &= require(measurementButton != nullptr, "measurement toolbar button exists");
    if (measurementButton) {
        measurementButton->click();
        QApplication::processEvents();
        auto *inspectorTabs = window.findChild<QTabWidget *>("inspectorTabs");
        passed &= require(inspectorTabs && inspectorTabs->currentIndex() == 3, "measurement button opens measurement module");
    }

    DicomDialog dicom(DicomDialog::InitialPage::Pacs);
    auto *dicomTabs = dicom.findChild<QTabWidget *>("dialogTabs");
    passed &= require(dicomTabs && dicomTabs->count() == 3, "DICOM center exposes three workflows");
    passed &= require(dicomTabs && dicomTabs->currentIndex() == 1, "DICOM center opens requested workflow");

    SafetyLockDialog safety;
    passed &= require(safety.windowTitle().contains(QStringLiteral("锁定")), "safety lock dialog is available");

    if (passed)
        std::cout << "UI smoke test passed\n";
    return passed ? 0 : 1;
}
