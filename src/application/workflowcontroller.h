#pragma once

#include <QObject>

class WorkflowController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentStep READ currentStep NOTIFY stateChanged)
    Q_PROPERTY(int maxReachedStep READ maxReachedStep NOTIFY stateChanged)
    Q_PROPERTY(QString stateName READ stateName NOTIFY stateChanged)
    Q_PROPERTY(bool locked READ locked NOTIFY stateChanged)

public:
    explicit WorkflowController(QObject *parent = nullptr);

    int currentStep() const { return m_currentStep; }
    int maxReachedStep() const { return m_maxReachedStep; }
    QString stateName() const;
    bool locked() const { return m_locked; }

    Q_INVOKABLE void advance();
    Q_INVOKABLE void back();
    Q_INVOKABLE void goToStep(int step);
    Q_INVOKABLE void setLocked(bool locked);
    Q_INVOKABLE bool canVisit(int step) const;

signals:
    void stateChanged();

private:
    int m_currentStep = 0;
    int m_maxReachedStep = 0;
    bool m_locked = false;
};
