#pragma once

#include <QVector3D>

#include <vector>

namespace MarkupsMetrics {

double distanceMm(const QVector3D &a, const QVector3D &b);
double angleDegrees(const QVector3D &a, const QVector3D &vertex, const QVector3D &b);
double perimeterMm(const std::vector<QVector3D> &points, bool closed);

} // namespace MarkupsMetrics
