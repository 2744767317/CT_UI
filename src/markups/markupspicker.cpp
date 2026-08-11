#include "markupspicker.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <QStringList>

namespace MarkupsPicker {

void viewCameraAxes(int viewType, double *right, double *up);

namespace {

int automaticProjectionQuarterTurns(const QString &orientation)
{
    const QStringList axes = orientation.split(QChar(u'\\'), Qt::SkipEmptyParts);
    if (axes.size() < 2)
        return 0;
    const QString first = axes.at(0).trimmed().toUpper();
    const QString second = axes.at(1).trimmed().toUpper();
    if (second == QStringLiteral("F"))
        return 0;
    if (second == QStringLiteral("H"))
        return 2;
    if (first == QStringLiteral("H"))
        return 1;
    if (first == QStringLiteral("F"))
        return 3;
    return 0;
}

std::array<double, 3> applyPresentation(const ImagePresentation &presentation,
                                        const std::array<double, 3> &physical)
{
    return {
        presentation.linear[0] * physical[0]
            + presentation.linear[1] * physical[1] + presentation.offset[0],
        presentation.linear[2] * physical[0]
            + presentation.linear[3] * physical[1] + presentation.offset[1],
        physical[2]
    };
}

std::array<double, 3> presentedPhysicalToWorld(const VolumeSnapshot &volume,
                                                const ImagePresentation &presentation,
                                                const std::array<double, 3> &physical)
{
    const auto presented = applyPresentation(presentation, physical);
    std::array<double, 3> world {volume.origin[0], volume.origin[1], volume.origin[2]};
    for (int row = 0; row < 3; ++row) {
        world[static_cast<std::size_t>(row)] +=
            volume.direction[row * 3 + 0] * presented[0]
            + volume.direction[row * 3 + 1] * presented[1]
            + volume.direction[row * 3 + 2] * presented[2];
    }
    return world;
}

struct SliceScreenGeometry
{
    int axisX = 0;
    int axisY = 1;
    int axisZ = 2;
    double right[3] {1.0, 0.0, 0.0};
    double up[3] {0.0, 1.0, 0.0};
    double minimumRight = 0.0;
    double maximumRight = 0.0;
    double minimumUp = 0.0;
    double maximumUp = 0.0;
    double baseRight = 0.0;
    double baseUp = 0.0;
    double xRight = 0.0;
    double xUp = 0.0;
    double yRight = 0.0;
    double yUp = 0.0;
};

double dot3(const std::array<double, 3> &point, const double *axis)
{
    return point[0] * axis[0] + point[1] * axis[1] + point[2] * axis[2];
}

SliceScreenGeometry sliceScreenGeometry(const VolumeSnapshot &volume, int viewType,
                                        double slicePosition,
                                        const ImagePresentation &presentation)
{
    SliceScreenGeometry geometry;
    viewAxes(viewType, &geometry.axisX, &geometry.axisY, &geometry.axisZ);
    viewCameraAxes(viewType, geometry.right, geometry.up);

    const auto rawSize = slicePhysicalSize(volume, viewType);
    const int sliceCount = std::max(1, volume.dimensions[geometry.axisZ]);
    std::array<double, 3> basePhysical {0.0, 0.0, 0.0};
    basePhysical[static_cast<std::size_t>(geometry.axisZ)] =
        sliceIndexFromPosition(slicePosition, sliceCount) * volume.spacing[geometry.axisZ];

    const auto baseWorld = presentedPhysicalToWorld(volume, presentation, basePhysical);
    geometry.baseRight = dot3(baseWorld, geometry.right);
    geometry.baseUp = dot3(baseWorld, geometry.up);

    auto xPhysical = basePhysical;
    xPhysical[static_cast<std::size_t>(geometry.axisX)] += 1.0;
    const auto xWorld = presentedPhysicalToWorld(volume, presentation, xPhysical);
    geometry.xRight = dot3(xWorld, geometry.right) - geometry.baseRight;
    geometry.xUp = dot3(xWorld, geometry.up) - geometry.baseUp;

    auto yPhysical = basePhysical;
    yPhysical[static_cast<std::size_t>(geometry.axisY)] += 1.0;
    const auto yWorld = presentedPhysicalToWorld(volume, presentation, yPhysical);
    geometry.yRight = dot3(yWorld, geometry.right) - geometry.baseRight;
    geometry.yUp = dot3(yWorld, geometry.up) - geometry.baseUp;

    geometry.minimumRight = std::numeric_limits<double>::max();
    geometry.maximumRight = std::numeric_limits<double>::lowest();
    geometry.minimumUp = std::numeric_limits<double>::max();
    geometry.maximumUp = std::numeric_limits<double>::lowest();
    for (int corner = 0; corner < 4; ++corner) {
        auto physical = basePhysical;
        physical[static_cast<std::size_t>(geometry.axisX)] =
            (corner & 1) ? rawSize[0] : 0.0;
        physical[static_cast<std::size_t>(geometry.axisY)] =
            (corner & 2) ? rawSize[1] : 0.0;
        const auto world = presentedPhysicalToWorld(volume, presentation, physical);
        const double right = dot3(world, geometry.right);
        const double up = dot3(world, geometry.up);
        geometry.minimumRight = std::min(geometry.minimumRight, right);
        geometry.maximumRight = std::max(geometry.maximumRight, right);
        geometry.minimumUp = std::min(geometry.minimumUp, up);
        geometry.maximumUp = std::max(geometry.maximumUp, up);
    }
    return geometry;
}

} // namespace

ImagePresentation imagePresentationFor(const VolumeSnapshot &volume, bool projection,
                                       const QString &patientOrientation,
                                       int rotationQuarterTurns,
                                       bool flipHorizontal, bool flipVertical)
{
    ImagePresentation presentation;
    if (!projection)
        return presentation;

    const double width = std::max(0, volume.dimensions[0] - 1) * volume.spacing[0];
    const double height = std::max(0, volume.dimensions[1] - 1) * volume.spacing[1];
    const double centerX = width * 0.5;
    const double centerY = height * 0.5;
    auto preMultiply = [&presentation](double a00, double a01, double a10, double a11,
                                       double tx, double ty) {
        const auto oldLinear = presentation.linear;
        const auto oldOffset = presentation.offset;
        presentation.linear = {
            a00 * oldLinear[0] + a01 * oldLinear[2],
            a00 * oldLinear[1] + a01 * oldLinear[3],
            a10 * oldLinear[0] + a11 * oldLinear[2],
            a10 * oldLinear[1] + a11 * oldLinear[3]
        };
        presentation.offset = {
            a00 * oldOffset[0] + a01 * oldOffset[1] + tx,
            a10 * oldOffset[0] + a11 * oldOffset[1] + ty
        };
    };
    auto centered = [&](double a00, double a01, double a10, double a11) {
        preMultiply(a00, a01, a10, a11,
                    centerX - a00 * centerX - a01 * centerY,
                    centerY - a10 * centerX - a11 * centerY);
    };

    centered(1.0, 0.0, 0.0, -1.0);
    const int turns = ((automaticProjectionQuarterTurns(patientOrientation)
                        + rotationQuarterTurns) % 4 + 4) % 4;
    if (turns != 0) {
        constexpr double halfPi = 1.5707963267948966;
        const double angle = halfPi * turns;
        centered(std::cos(angle), -std::sin(angle), std::sin(angle), std::cos(angle));
    }
    if (flipHorizontal)
        centered(-1.0, 0.0, 0.0, 1.0);
    if (flipVertical)
        centered(1.0, 0.0, 0.0, -1.0);
    return presentation;
}

void viewAxes(int viewType, int *axisX, int *axisY, int *axisZ)
{
    if (viewType == 1) {
        *axisX = 0;
        *axisY = 2;
        *axisZ = 1;
    } else if (viewType == 2) {
        *axisX = 1;
        *axisY = 2;
        *axisZ = 0;
    } else {
        *axisX = 0;
        *axisY = 1;
        *axisZ = 2;
    }
}

std::array<double, 2> slicePhysicalSize(const VolumeSnapshot &volume, int viewType)
{
    int axisX = 0;
    int axisY = 1;
    int axisZ = 2;
    viewAxes(viewType, &axisX, &axisY, &axisZ);
    (void)axisZ;
    const auto length = [&](int axis) {
        const int intervals = std::max(0, volume.dimensions[axis] - 1);
        return std::max(volume.spacing[axis],
                        static_cast<double>(intervals) * volume.spacing[axis]);
    };
    return {length(axisX), length(axisY)};
}

std::array<double, 2> sliceViewPhysicalSize(const VolumeSnapshot &volume, int viewType,
                                            const ImagePresentation &presentation)
{
    const auto geometry = sliceScreenGeometry(volume, viewType, 0.5, presentation);
    return {std::max(1e-9, geometry.maximumRight - geometry.minimumRight),
            std::max(1e-9, geometry.maximumUp - geometry.minimumUp)};
}

// 与 medicalviewportitem.cpp rebuildPipeline 的相机一致：轴向 +Z 观察/up +Y、
// 冠状 -Y 观察/up +Z、矢状 +X 观察/up +Z。right/up 均为世界(渲染)坐标。
// 统一 slicePosition→切片索引：用截断(floor)，与 configureSlice / 切片更新处的
// 显示切片保持一致。过去落点/透明度用 lround 而显示用截断，小数≥0.5 时标注会
// 偏到相邻层；配合切片轴翻转(direction 对角元为负)时该偏移背对相机，标注被显示
// 切片遮挡。统一后标注永远落在正在显示的那一层。
int sliceIndexFromPosition(double slicePosition, int sliceCount)
{
    const int last = std::max(0, sliceCount - 1);
    // Slider/wheel navigation produces exact logical steps as index / last, but the
    // floating-point product may land infinitesimally below the integer. Keep floor
    // semantics while preventing an accidental jump to the preceding slice.
    constexpr double stepEpsilon = 1e-9;
    return std::clamp(static_cast<int>(std::floor(slicePosition * last + stepEpsilon)),
                      0, last);
}

void viewCameraAxes(int viewType, double *right, double *up)
{
    if (viewType == 2) {          // Sagittal：相机 +X，右 = +Y，上 = +Z
        right[0] = 0.0; right[1] = 1.0; right[2] = 0.0;
        up[0] = 0.0; up[1] = 0.0; up[2] = 1.0;
    } else if (viewType == 1) {   // Coronal：相机 -Y，右 = +X，上 = +Z
        right[0] = 1.0; right[1] = 0.0; right[2] = 0.0;
        up[0] = 0.0; up[1] = 0.0; up[2] = 1.0;
    } else {                      // Axial：相机 +Z，右 = +X，上 = +Y
        right[0] = 1.0; right[1] = 0.0; right[2] = 0.0;
        up[0] = 0.0; up[1] = 1.0; up[2] = 0.0;
    }
}

// +voxelAxis 体素轴在 worldAxis 上的投影（direction 第 voxelAxis 列 · worldAxis）。
// 符号决定该体素轴沿屏幕 right/up 是正向还是反向（处理 direction 中的轴翻转）。
double axisProjection(const std::array<double, 9> &direction, int voxelAxis,
                      const double *worldAxis)
{
    return direction[0 * 3 + voxelAxis] * worldAxis[0]
        + direction[1 * 3 + voxelAxis] * worldAxis[1]
        + direction[2 * 3 + voxelAxis] * worldAxis[2];
}

QVector3D voxelToWorld(const VolumeSnapshot &volume, int i, int j, int k)
{
    const double index[3] = {
        static_cast<double>(i) * volume.spacing[0],
        static_cast<double>(j) * volume.spacing[1],
        static_cast<double>(k) * volume.spacing[2]
    };
    double world[3] = {volume.origin[0], volume.origin[1], volume.origin[2]};
    for (int row = 0; row < 3; ++row) {
        world[row] += volume.direction[row * 3 + 0] * index[0]
            + volume.direction[row * 3 + 1] * index[1]
            + volume.direction[row * 3 + 2] * index[2];
    }
    return QVector3D(static_cast<float>(world[0]),
                     static_cast<float>(world[1]),
                     static_cast<float>(world[2]));
}

bool worldToVoxel(const VolumeSnapshot &volume, const QVector3D &world,
                  int *i, int *j, int *k)
{
    if (!i || !j || !k)
        return false;
    // 当前体数据方向矩阵按列正交近似；用 LPS 线性逆变换。
    double relative[3] = {
        world.x() - volume.origin[0],
        world.y() - volume.origin[1],
        world.z() - volume.origin[2]
    };
    double indexPhysical[3] = {0.0, 0.0, 0.0};
    for (int col = 0; col < 3; ++col) {
        indexPhysical[col] = volume.direction[0 * 3 + col] * relative[0]
            + volume.direction[1 * 3 + col] * relative[1]
            + volume.direction[2 * 3 + col] * relative[2];
    }
    const double coords[3] = {
        volume.spacing[0] > 0.0 ? indexPhysical[0] / volume.spacing[0] : 0.0,
        volume.spacing[1] > 0.0 ? indexPhysical[1] / volume.spacing[1] : 0.0,
        volume.spacing[2] > 0.0 ? indexPhysical[2] / volume.spacing[2] : 0.0
    };
    *i = std::clamp(static_cast<int>(std::lround(coords[0])), 0,
                    std::max(0, volume.dimensions[0] - 1));
    *j = std::clamp(static_cast<int>(std::lround(coords[1])), 0,
                    std::max(0, volume.dimensions[1] - 1));
    *k = std::clamp(static_cast<int>(std::lround(coords[2])), 0,
                    std::max(0, volume.dimensions[2] - 1));
    return volume.dimensions[0] > 0 && volume.dimensions[1] > 0 && volume.dimensions[2] > 0;
}

bool mapClickToWorld(const VolumeSnapshot &volume, int viewType, double slicePosition,
                     double itemX, double itemY, double viewportWidth, double viewportHeight,
                     QVector3D *worldOut, int *voxelOut,
                     const ImagePresentation &presentation)
{
    if (!worldOut || viewportWidth <= 0.0 || viewportHeight <= 0.0)
        return false;
    if (volume.dimensions[0] <= 0 || volume.dimensions[1] <= 0 || volume.dimensions[2] <= 0)
        return false;

    const auto geometry = sliceScreenGeometry(volume, viewType, slicePosition, presentation);
    const double physicalWidth = geometry.maximumRight - geometry.minimumRight;
    const double physicalHeight = geometry.maximumUp - geometry.minimumUp;
    const double scale = std::min(viewportWidth / physicalWidth,
                                  viewportHeight / physicalHeight);
    if (scale <= 0.0)
        return false;
    const double offsetX = (viewportWidth - physicalWidth * scale) * 0.5;
    const double offsetY = (viewportHeight - physicalHeight * scale) * 0.5;
    if (itemX < offsetX || itemY < offsetY
        || itemX > offsetX + physicalWidth * scale
        || itemY > offsetY + physicalHeight * scale)
        return false;

    const double targetRight = geometry.minimumRight + (itemX - offsetX) / scale;
    const double targetUp = geometry.maximumUp - (itemY - offsetY) / scale;
    const double deltaRight = targetRight - geometry.baseRight;
    const double deltaUp = targetUp - geometry.baseUp;
    const double determinant = geometry.xRight * geometry.yUp
        - geometry.yRight * geometry.xUp;
    if (std::abs(determinant) < 1e-9)
        return false;
    const double xPhysical = (deltaRight * geometry.yUp
                              - geometry.yRight * deltaUp) / determinant;
    const double yPhysical = (geometry.xRight * deltaUp
                              - deltaRight * geometry.xUp) / determinant;
    const double xIndex = volume.spacing[geometry.axisX] > 0.0
        ? xPhysical / volume.spacing[geometry.axisX] : 0.0;
    const double yIndex = volume.spacing[geometry.axisY] > 0.0
        ? yPhysical / volume.spacing[geometry.axisY] : 0.0;

    std::array<int, 3> index {0, 0, 0};
    index[geometry.axisX] = std::clamp(static_cast<int>(std::lround(xIndex)), 0,
                                       volume.dimensions[geometry.axisX] - 1);
    index[geometry.axisY] = std::clamp(static_cast<int>(std::lround(yIndex)), 0,
                                       volume.dimensions[geometry.axisY] - 1);
    const int sliceCount = std::max(1, volume.dimensions[geometry.axisZ]);
    index[geometry.axisZ] = sliceIndexFromPosition(slicePosition, sliceCount);

    if (voxelOut) {
        voxelOut[0] = index[0];
        voxelOut[1] = index[1];
        voxelOut[2] = index[2];
    }
    *worldOut = voxelToWorld(volume, index[0], index[1], index[2]);
    return true;
}

int sliceDelta(const VolumeSnapshot &volume, int viewType, double slicePosition,
               const QVector3D &world)
{
    int i = 0;
    int j = 0;
    int k = 0;
    if (!worldToVoxel(volume, world, &i, &j, &k))
        return 9999;
    int axisX = 0;
    int axisY = 1;
    int axisZ = 2;
    viewAxes(viewType, &axisX, &axisY, &axisZ);
    const int coords[3] = {i, j, k};
    const int sliceCount = std::max(1, volume.dimensions[axisZ]);
    const int current = sliceIndexFromPosition(slicePosition, sliceCount);
    return coords[axisZ] - current;
}

double signedSliceDistanceMm(const VolumeSnapshot &volume, int viewType,
                             double slicePosition, const QVector3D &world)
{
    int axisX = 0;
    int axisY = 1;
    int axisZ = 2;
    viewAxes(viewType, &axisX, &axisY, &axisZ);
    (void)axisX;
    (void)axisY;
    const auto physical = worldToImagePhysical(volume, world);
    const int sliceCount = std::max(1, volume.dimensions[axisZ]);
    const int slice = sliceIndexFromPosition(slicePosition, sliceCount);
    return physical[static_cast<std::size_t>(axisZ)]
        - static_cast<double>(slice) * volume.spacing[axisZ];
}

double sliceSlabHalfThicknessMm(const VolumeSnapshot &volume, int viewType)
{
    int axisX = 0;
    int axisY = 1;
    int axisZ = 2;
    viewAxes(viewType, &axisX, &axisY, &axisZ);
    (void)axisX;
    (void)axisY;
    return std::max(0.0, volume.spacing[axisZ]) * 0.5;
}

bool isPointDisplayableOnSlice(const VolumeSnapshot &volume, int viewType,
                               double slicePosition, const QVector3D &world)
{
    constexpr double epsilonMm = 1e-6;
    return std::abs(signedSliceDistanceMm(volume, viewType, slicePosition, world))
        <= sliceSlabHalfThicknessMm(volume, viewType) + epsilonMm;
}

bool segmentSlicePlaneIntersection(const VolumeSnapshot &volume, int viewType,
                                   double slicePosition, const QVector3D &a,
                                   const QVector3D &b, QVector3D *intersectionOut)
{
    if (!intersectionOut)
        return false;
    constexpr double epsilonMm = 1e-6;
    const double da = signedSliceDistanceMm(volume, viewType, slicePosition, a);
    const double db = signedSliceDistanceMm(volume, viewType, slicePosition, b);
    if (std::abs(da) <= epsilonMm && std::abs(db) <= epsilonMm)
        return false;
    if ((da > epsilonMm && db > epsilonMm) || (da < -epsilonMm && db < -epsilonMm))
        return false;
    const double denominator = da - db;
    if (std::abs(denominator) <= epsilonMm)
        return false;
    const double t = std::clamp(da / denominator, 0.0, 1.0);
    *intersectionOut = a + static_cast<float>(t) * (b - a);
    return true;
}

bool clipSegmentToSliceSlab(const VolumeSnapshot &volume, int viewType,
                            double slicePosition, const QVector3D &a,
                            const QVector3D &b, QVector3D *clippedA,
                            QVector3D *clippedB)
{
    if (!clippedA || !clippedB)
        return false;
    constexpr double epsilonMm = 1e-6;
    const double halfThickness = sliceSlabHalfThicknessMm(volume, viewType);
    const double da = signedSliceDistanceMm(volume, viewType, slicePosition, a);
    const double db = signedSliceDistanceMm(volume, viewType, slicePosition, b);
    const double delta = db - da;

    double begin = 0.0;
    double end = 1.0;
    if (std::abs(delta) <= epsilonMm) {
        if (std::abs(da) > halfThickness + epsilonMm)
            return false;
    } else {
        const double atNegativeBoundary = (-halfThickness - da) / delta;
        const double atPositiveBoundary = (halfThickness - da) / delta;
        begin = std::max(0.0, std::min(atNegativeBoundary, atPositiveBoundary));
        end = std::min(1.0, std::max(atNegativeBoundary, atPositiveBoundary));
        if (begin > end + epsilonMm)
            return false;
    }

    const QVector3D direction = b - a;
    *clippedA = a + static_cast<float>(begin) * direction;
    *clippedB = a + static_cast<float>(end) * direction;
    return true;
}

std::vector<std::vector<QVector3D>> clipPolylineToSliceSlab(
    const VolumeSnapshot &volume, int viewType, double slicePosition,
    const std::vector<QVector3D> &points, bool closed)
{
    std::vector<std::vector<QVector3D>> runs;
    if (points.size() < 2)
        return runs;

    constexpr float joinToleranceSquared = 1e-8f;
    const std::size_t segmentCount = closed ? points.size() : points.size() - 1;
    for (std::size_t index = 0; index < segmentCount; ++index) {
        QVector3D a;
        QVector3D b;
        if (!clipSegmentToSliceSlab(volume, viewType, slicePosition,
                                    points[index], points[(index + 1) % points.size()],
                                    &a, &b)) {
            continue;
        }
        if (!runs.empty() && (runs.back().back() - a).lengthSquared()
                <= joinToleranceSquared) {
            if ((runs.back().back() - b).lengthSquared() > joinToleranceSquared)
                runs.back().push_back(b);
        } else {
            runs.push_back({a, b});
        }
    }

    if (closed && runs.size() > 1
        && (runs.back().back() - runs.front().front()).lengthSquared()
            <= joinToleranceSquared) {
        std::vector<QVector3D> merged = std::move(runs.back());
        runs.pop_back();
        merged.insert(merged.end(), std::next(runs.front().begin()), runs.front().end());
        runs.front() = std::move(merged);
    }
    return runs;
}

std::array<double, 3> worldToImagePhysical(const VolumeSnapshot &volume,
                                           const QVector3D &world)
{
    const double relative[3] = {
        world.x() - volume.origin[0],
        world.y() - volume.origin[1],
        world.z() - volume.origin[2]
    };
    std::array<double, 3> indexPhysical {0.0, 0.0, 0.0};
    for (int col = 0; col < 3; ++col) {
        indexPhysical[static_cast<std::size_t>(col)] =
            volume.direction[0 * 3 + col] * relative[0]
            + volume.direction[1 * 3 + col] * relative[1]
            + volume.direction[2 * 3 + col] * relative[2];
    }
    return indexPhysical;
}

std::array<double, 3> worldToSliceImagePhysical(const VolumeSnapshot &volume,
                                                int viewType,
                                                double slicePosition,
                                                const QVector3D &world)
{
    auto physical = worldToImagePhysical(volume, world);
    int axisX = 0;
    int axisY = 1;
    int axisZ = 2;
    viewAxes(viewType, &axisX, &axisY, &axisZ);
    (void)axisX;
    (void)axisY;

    const int sliceCount = std::max(1, volume.dimensions[axisZ]);
    const int slice = sliceIndexFromPosition(slicePosition, sliceCount);
    physical[static_cast<std::size_t>(axisZ)] =
        static_cast<double>(slice) * volume.spacing[axisZ];
    return physical;
}

bool worldToDisplay(const VolumeSnapshot &volume, int viewType, double slicePosition,
                    double viewportWidth, double viewportHeight, const QVector3D &world,
                    double *displayX, double *displayY,
                    const ImagePresentation &presentation)
{
    if (!displayX || !displayY || viewportWidth <= 0.0 || viewportHeight <= 0.0)
        return false;

    const auto physical = worldToImagePhysical(volume, world);
    const auto geometry = sliceScreenGeometry(volume, viewType, slicePosition, presentation);
    const double physicalWidth = geometry.maximumRight - geometry.minimumRight;
    const double physicalHeight = geometry.maximumUp - geometry.minimumUp;
    const double scale = std::min(viewportWidth / physicalWidth,
                                  viewportHeight / physicalHeight);
    if (scale <= 0.0)
        return false;
    const double offsetX = (viewportWidth - physicalWidth * scale) * 0.5;
    const double offsetY = (viewportHeight - physicalHeight * scale) * 0.5;
    const auto presentedWorld = presentedPhysicalToWorld(volume, presentation, physical);
    const double right = dot3(presentedWorld, geometry.right);
    const double up = dot3(presentedWorld, geometry.up);
    *displayX = offsetX + (right - geometry.minimumRight) * scale;
    *displayY = offsetY + (geometry.maximumUp - up) * scale;
    return true;
}

} // namespace MarkupsPicker
