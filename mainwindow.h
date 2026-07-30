#pragma once

#include <QMainWindow>

class QStackedWidget;
class QPushButton;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void setMode(int index);

    QStackedWidget *m_pages = nullptr;
    QPushButton *m_modeButtons[3] = {nullptr, nullptr, nullptr};
};

