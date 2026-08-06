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

} // namespace MarkupsMetrics
