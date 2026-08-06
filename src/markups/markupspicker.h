#pragma once

#include "src/dicom/medicaldatacontroller.h"

#include <QVector3D>

#include <array>

namespace MarkupsPicker {

void viewAxes(int viewType, int *axisX, int *axisY, int *axisZ);

/// slicePosition([0,1]) → 切片索引。显示、落点、透明度必须共用同一种换算，
/// 否则截断/四舍五入不一致会让标注落到显示切片的相邻层（含轴翻转时被遮挡）。
int sliceIndexFromPosition(double slicePosition, int sliceCount);

QVector3D voxelToWorld(const VolumeSnapshot &volume, int i, int j, int k);
bool worldToVoxel(const VolumeSnapshot &volume, const QVector3D &world,
                  int *i, int *j, int *k);

/// 将切片视口点击映射为世界坐标；失败返回 false。
bool mapClickToWorld(const VolumeSnapshot &volume, int viewType, double slicePosition,
                     double itemX, double itemY, double viewportWidth, double viewportHeight,
                     QVector3D *worldOut, int *voxelOut = nullptr);

/// 世界点相对当前切片的层差（体素层）。
int sliceDelta(const VolumeSnapshot &volume, int viewType, double slicePosition,
               const QVector3D &world);

/// 世界坐标 → 体数据图像物理坐标（ijk * spacing，供 VTK UserMatrix=dataToWorld 使用）。
std::array<double, 3> worldToImagePhysical(const VolumeSnapshot &volume,
                                           const QVector3D &world);

/// 世界坐标 → 切片视口像素；失败返回 false。
bool worldToDisplay(const VolumeSnapshot &volume, int viewType, double slicePosition,
                    double viewportWidth, double viewportHeight, const QVector3D &world,
                    double *displayX, double *displayY);

} // namespace MarkupsPicker
