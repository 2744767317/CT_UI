#include "workflowcontroller.h"

#include <QStringList>

WorkflowController::WorkflowController(QObject *parent)
    : QObject(parent)
{
}

QString WorkflowController::stateName() const
{
    if (m_locked)
        return QStringLiteral("异常 / 锁定");

    static const QStringList names = {
        QStringLiteral("患者确认"),
        QStringLiteral("联锁检查"),
        QStringLiteral("扫描范围"),
        QStringLiteral("影像工作站")
    };
    return names.value(m_currentStep);
}

void WorkflowController::advance()
{
    if (m_locked || m_currentStep >= 3)
        return;

    ++m_currentStep;
    m_maxReachedStep = qMax(m_maxReachedStep, m_currentStep);
    emit stateChanged();
}

void WorkflowController::back()
{
    if (m_locked || m_currentStep <= 0)
        return;

    --m_currentStep;
    emit stateChanged();
}

void WorkflowController::goToStep(int step)
{
    if (m_locked || !canVisit(step) || m_currentStep == step)
        return;

    m_currentStep = step;
    emit stateChanged();
}

void WorkflowController::setLocked(bool locked)
{
    if (m_locked == locked)
        return;

    m_locked = locked;
    emit stateChanged();
}

bool WorkflowController::canVisit(int step) const
{
    return step >= 0 && step <= m_maxReachedStep && step < 4;
}
