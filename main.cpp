#include "mainwindow.h"

#include <QApplication>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("光索科技正交投影 CT 影像工作站");
    app.setOrganizationName("Medical Imaging Systems");

    QFont font("Microsoft YaHei UI");
    font.setPointSize(10);
    app.setFont(font);

    MainWindow window;
    window.showMaximized();
    return app.exec();
}
