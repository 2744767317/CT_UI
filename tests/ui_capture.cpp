#include "mainwindow.h"

#include <QApplication>
#include <QDir>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>

#include <iostream>

namespace {

QPushButton *buttonByAccessibleName(MainWindow &window, const QString &name)
{
    const auto buttons = window.findChildren<QPushButton *>();
    for (auto *button : buttons) {
        if (button->accessibleName() == name)
            return button;
    }
    return nullptr;
}

bool capture(MainWindow &window, const QString &path)
{
    QApplication::processEvents();
    const QPixmap pixmap = window.screen()->grabWindow(window.winId());
    return pixmap.save(path, "PNG");
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const QDir output(argc > 1 ? QString::fromLocal8Bit(argv[1]) : QDir::currentPath());
    MainWindow window;
    window.showMaximized();
    QApplication::processEvents();

    const QStringList names = {"patient", "safety", "range", "editor"};
    const QStringList actions = {"patientNextButton", "safetyNextButton", "rangeNextButton"};
    for (int page = 0; page < names.size(); ++page) {
        const QString path = output.filePath(QString("ui-page-%1-%2.png").arg(page + 1).arg(names.at(page)));
        if (!capture(window, path)) {
            std::cerr << "Unable to save " << path.toStdString() << '\n';
            return 1;
        }
        if (page < actions.size()) {
            auto *next = buttonByAccessibleName(window, actions.at(page));
            if (!next)
                return 1;
            next->click();
            QApplication::processEvents();
        }
    }
    std::cout << "Captured four workflow pages\n";
    return 0;
}
