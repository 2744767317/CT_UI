#pragma once

#include <QWidget>

#include <functional>

class PatientConfirmationPage final : public QWidget
{
public:
    explicit PatientConfirmationPage(QWidget *parent = nullptr);
    void setContinueCallback(std::function<void()> callback);

private:
    std::function<void()> m_continueCallback;
};

class SafetyCheckPage final : public QWidget
{
public:
    explicit SafetyCheckPage(QWidget *parent = nullptr);
    void setBackCallback(std::function<void()> callback);
    void setContinueCallback(std::function<void()> callback);

private:
    std::function<void()> m_backCallback;
    std::function<void()> m_continueCallback;
};

class ScanRangePage final : public QWidget
{
public:
    explicit ScanRangePage(QWidget *parent = nullptr);
    void setBackCallback(std::function<void()> callback);
    void setContinueCallback(std::function<void()> callback);

private:
    std::function<void()> m_backCallback;
    std::function<void()> m_continueCallback;
};
