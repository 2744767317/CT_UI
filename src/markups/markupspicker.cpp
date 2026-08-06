#include "markupspicker.h"

#include <algorithm>
#include <cmath>

namespace MarkupsPicker {

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

// 与 medicalviewportitem.cpp rebuildPipeline 的相机一致：轴向 +Z 观察/up +Y、
// 冠状 -Y 观察/up +Z、矢状 +X 观察/up +Z。right/up 均为世界(渲染)坐标。
// 统一 slicePosition→切片索引：用截断(floor)，与 configureSlice / 切片更新处的
// 显示切片保持一致。过去落点/透明度用 lround 而显示用截断，小数≥0.5 时标注会
// 偏到相邻层；配合切片轴翻转(direction 对角元为负)时该偏移背对相机，标注被显示
// 切片遮挡。统一后标注永远落在正在显示的那一层。
int sliceIndexFromPosition(double slicePosition, int sliceCount)
{
    const int last = std::max(0, sliceCount - 1);
    return std::clamp(static_cast<int>(slicePosition * last), 0, last);
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
                     QVector3D *worldOut, int *voxelOut)
{
    if (!worldOut || viewportWidth <= 0.0 || viewportHeight <= 0.0)
        return false;
    if (volume.dimensions[0] <= 0 || volume.dimensions[1] <= 0 || volume.dimensions[2] <= 0)
        return false;

    int axisX = 0;
    int axisY = 1;
    int axisZ = 2;
    viewAxes(viewType, &axisX, &axisY, &axisZ);

    const double dimX = static_cast<double>(volume.dimensions[axisX]);
    const double dimY = static_cast<double>(volume.dimensions[axisY]);
    const double scale = std::min(viewportWidth / dimX, viewportHeight / dimY);
    if (scale <= 0.0)
        return false;
    const double offsetX = (viewportWidth - dimX * scale) * 0.5;
    const double offsetY = (viewportHeight - dimY * scale) * 0.5;
    if (itemX < offsetX || itemY < offsetY
        || itemX > offsetX + dimX * scale || itemY > offsetY + dimY * scale)
        return false;

    const double u = (itemX - offsetX) / scale - 0.5;
    const double vFromTop = (itemY - offsetY) / scale - 0.5;
    const double v = dimY - 1.0 - vFromTop;

    // 默认假设 +axisX 朝屏幕右、+axisY 朝屏幕上；当 direction 含轴翻转
    // （如倒序存储的 DICOM 切片轴为负）时该假设失效，需按投影符号反转，
    // 否则冠状/矢状视图的点击落点会上下/左右镜像错位。
    double right[3];
    double up[3];
    viewCameraAxes(viewType, right, up);
    const double uu = axisProjection(volume.direction, axisX, right) < 0.0
        ? (dimX - 1.0 - u) : u;
    const double vv = axisProjection(volume.direction, axisY, up) < 0.0
        ? (dimY - 1.0 - v) : v;

    std::array<int, 3> index {0, 0, 0};
    index[axisX] = std::clamp(static_cast<int>(std::lround(uu)), 0, volume.dimensions[axisX] - 1);
    index[axisY] = std::clamp(static_cast<int>(std::lround(vv)), 0, volume.dimensions[axisY] - 1);
    const int sliceCount = std::max(1, volume.dimensions[axisZ]);
    index[axisZ] = sliceIndexFromPosition(slicePosition, sliceCount);

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

bool worldToDisplay(const VolumeSnapshot &volume, int viewType, double /*slicePosition*/,
                    double viewportWidth, double viewportHeight, const QVector3D &world,
                    double *displayX, double *displayY)
{
    if (!displayX || !displayY || viewportWidth <= 0.0 || viewportHeight <= 0.0)
        return false;
    int i = 0;
    int j = 0;
    int k = 0;
    if (!worldToVoxel(volume, world, &i, &j, &k))
        return false;

    int axisX = 0;
    int axisY = 1;
    int axisZ = 2;
    viewAxes(viewType, &axisX, &axisY, &axisZ);
    const int coords[3] = {i, j, k};
    (void)axisZ;

    const double dimX = static_cast<double>(volume.dimensions[axisX]);
    const double dimY = static_cast<double>(volume.dimensions[axisY]);
    const double scale = std::min(viewportWidth / dimX, viewportHeight / dimY);
    if (scale <= 0.0)
        return false;
    const double offsetX = (viewportWidth - dimX * scale) * 0.5;
    const double offsetY = (viewportHeight - dimY * scale) * 0.5;

    // 与 mapClickToWorld 互逆且方向感知：direction 含轴翻转时同样按投影符号反转，
    // 使悬停/命中测试的屏幕投影与实际渲染一致。
    double right[3];
    double up[3];
    viewCameraAxes(viewType, right, up);
    const double u = axisProjection(volume.direction, axisX, right) < 0.0
        ? (dimX - 1.0 - static_cast<double>(coords[axisX]))
        : static_cast<double>(coords[axisX]);
    const double v = axisProjection(volume.direction, axisY, up) < 0.0
        ? (dimY - 1.0 - static_cast<double>(coords[axisY]))
        : static_cast<double>(coords[axisY]);
    const double vFromTop = dimY - 1.0 - v;
    *displayX = offsetX + (u + 0.5) * scale;
    *displayY = offsetY + (vFromTop + 0.5) * scale;
    return true;
}

} // namespace MarkupsPicker
