#include "markupsnode.h"

#include "markupsmetrics.h"

void refreshMarkupsMetrics(MarkupsNode *node)
{
    if (!node)
        return;
    switch (node->type) {
    case MarkupsNodeType::Point:
        node->metric = 0.0;
        node->displayText = node->label;
        break;
    case MarkupsNodeType::Line:
        if (node->controlPoints.size() >= 2) {
            node->metric = MarkupsMetrics::distanceMm(node->controlPoints[0],
                                                     node->controlPoints[1]);
            node->displayText = QStringLiteral("%1: %2mm")
                                    .arg(node->label)
                                    .arg(node->metric, 0, 'f', 2);
        } else {
            node->metric = 0.0;
            node->displayText = node->label;
        }
        break;
    case MarkupsNodeType::Angle:
        if (node->controlPoints.size() >= 3) {
            node->metric = MarkupsMetrics::angleDegrees(node->controlPoints[0],
                                                       node->controlPoints[1],
                                                       node->controlPoints[2]);
            node->displayText = QStringLiteral("%1: %2°")
                                    .arg(node->label)
                                    .arg(node->metric, 0, 'f', 1);
        } else {
            node->metric = 0.0;
            node->displayText = node->label;
        }
        break;
    case MarkupsNodeType::ClosedCurve:
        node->metric = MarkupsMetrics::perimeterMm(node->controlPoints, true);
        node->displayText = QStringLiteral("%1: %2mm")
                                .arg(node->label)
                                .arg(node->metric, 0, 'f', 2);
        break;
    }
}
