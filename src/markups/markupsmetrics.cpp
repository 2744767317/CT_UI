#include "markupsmetrics.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace MarkupsMetrics {

double distanceMm(const QVector3D &a, const QVector3D &b)
{
    return static_cast<double>((a - b).length());
}

double angleDegrees(const QVector3D &a, const QVector3D &vertex, const QVector3D &b)
{
    const QVector3D va = a - vertex;
    const QVector3D vb = b - vertex;
    if (va.lengthSquared() < 1e-12f || vb.lengthSquared() < 1e-12f)
        return 0.0;
    const double cosAngle = std::clamp(double(QVector3D::dotProduct(va.normalized(),
                                                                     vb.normalized())),
                                       -1.0, 1.0);
    return qRadiansToDegrees(std::acos(cosAngle));
}

double perimeterMm(const std::vector<QVector3D> &points, bool closed)
{
    if (points.size() < 2)
        return 0.0;
    double total = 0.0;
    for (std::size_t index = 1; index < points.size(); ++index)
        total += distanceMm(points[index - 1], points[index]);
    if (closed && points.size() >= 3)
        total += distanceMm(points.back(), points.front());
    return total;
}

std::vector<QVector3D> curveSamples(const std::vector<QVector3D> &controlPoints,
                                    bool closed, int samplesPerSegment)
{
    if (controlPoints.size() < 2 || (closed && controlPoints.size() < 3))
        return controlPoints;

    samplesPerSegment = std::max(2, samplesPerSegment);
    std::vector<QVector3D> samples;
    const std::size_t segmentCount = closed
        ? controlPoints.size() : controlPoints.size() - 1;
    samples.reserve(segmentCount * static_cast<std::size_t>(samplesPerSegment)
                    + (closed ? 0 : 1));

    const std::size_t count = controlPoints.size();
    for (std::size_t segment = 0; segment < segmentCount; ++segment) {
        const QVector3D &p0 = closed
            ? controlPoints[(segment + count - 1) % count]
            : controlPoints[segment == 0 ? 0 : segment - 1];
        const QVector3D &p1 = controlPoints[segment];
        const QVector3D &p2 = controlPoints[closed ? (segment + 1) % count : segment + 1];
        const QVector3D &p3 = closed
            ? controlPoints[(segment + 2) % count]
            : controlPoints[std::min(segment + 2, count - 1)];

        for (int sample = 0; sample < samplesPerSegment; ++sample) {
            const float t = static_cast<float>(sample)
                / static_cast<float>(samplesPerSegment);
            const float t2 = t * t;
            const float t3 = t2 * t;
            samples.push_back(0.5f * ((2.0f * p1)
                + (-p0 + p2) * t
                + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
                + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3));
        }
    }
    if (!closed)
        samples.push_back(controlPoints.back());
    return samples;
}

double curveLengthMm(const std::vector<QVector3D> &controlPoints,
                     bool closed, int samplesPerSegment)
{
    return perimeterMm(curveSamples(controlPoints, closed, samplesPerSegment), closed);
}

std::vector<QVector3D> closedCurveSamples(const std::vector<QVector3D> &controlPoints,
                                          int samplesPerSegment)
{
    return curveSamples(controlPoints, true, samplesPerSegment);
}

double closedCurveLengthMm(const std::vector<QVector3D> &controlPoints,
                           int samplesPerSegment)
{
    return curveLengthMm(controlPoints, true, samplesPerSegment);
}

} // namespace MarkupsMetrics
