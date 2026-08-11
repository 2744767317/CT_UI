#pragma once

#include <QVector3D>

#include <vector>

namespace MarkupsMetrics {

double distanceMm(const QVector3D &a, const QVector3D &b);
double angleDegrees(const QVector3D &a, const QVector3D &vertex, const QVector3D &b);
double perimeterMm(const std::vector<QVector3D> &points, bool closed);
std::vector<QVector3D> curveSamples(const std::vector<QVector3D> &controlPoints,
                                    bool closed, int samplesPerSegment = 16);
double curveLengthMm(const std::vector<QVector3D> &controlPoints,
                     bool closed, int samplesPerSegment = 16);
std::vector<QVector3D> closedCurveSamples(const std::vector<QVector3D> &controlPoints,
                                          int samplesPerSegment = 16);
double closedCurveLengthMm(const std::vector<QVector3D> &controlPoints,
                           int samplesPerSegment = 16);

} // namespace MarkupsMetrics
