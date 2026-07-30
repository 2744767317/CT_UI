#include "mainwindow.h"

#include <QApplication>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("EOS Administration");
    app.setOrganizationName("EOS Prototype");

    QFont font("Microsoft YaHei UI");
    font.setPointSize(9);
    app.setFont(font);

    MainWindow window;
    window.showMaximized();
    return app.exec();
}

