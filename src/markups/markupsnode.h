#pragma once

#include <QString>
#include <QVector3D>

#include <vector>

enum class MarkupsNodeType {
    Point = 0,
    Line = 1,
    Angle = 2,
    ClosedCurve = 3
};

struct MarkupsNode {
    int id = 0;
    MarkupsNodeType type = MarkupsNodeType::Point;
    QString label;
    QString displayText;
    QString color = QStringLiteral("#E53935");
    bool visible = true;
    double metric = 0.0;
    std::vector<QVector3D> controlPoints; // world mm
};

void refreshMarkupsMetrics(MarkupsNode *node);
