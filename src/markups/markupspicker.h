#pragma once

#include "src/dicom/medicaldatacontroller.h"

#include <QVector3D>
#include <QString>

#include <array>
#include <vector>

namespace MarkupsPicker {

struct ImagePresentation
{
    // Affine transform in image physical X/Y coordinates. It is applied before
    // the DICOM image-to-world matrix, exactly like vtkImageSlice::UserMatrix.
    std::array<double, 4> linear {1.0, 0.0, 0.0, 1.0};
    std::array<double, 2> offset {0.0, 0.0};
};

ImagePresentation imagePresentationFor(const VolumeSnapshot &volume, bool projection,
                                       const QString &patientOrientation,
                                       int rotationQuarterTurns,
                                       bool flipHorizontal, bool flipVertical);

void viewAxes(int viewType, int *axisX, int *axisY, int *axisZ);
std::array<double, 2> slicePhysicalSize(const VolumeSnapshot &volume, int viewType);
std::array<double, 2> sliceViewPhysicalSize(const VolumeSnapshot &volume, int viewType,
                                            const ImagePresentation &presentation = {});

/// slicePosition([0,1]) → 切片索引。显示、落点、透明度必须共用同一种换算，
/// 否则截断/四舍五入不一致会让标注落到显示切片的相邻层（含轴翻转时被遮挡）。
int sliceIndexFromPosition(double slicePosition, int sliceCount);

QVector3D voxelToWorld(const VolumeSnapshot &volume, int i, int j, int k);
bool worldToVoxel(const VolumeSnapshot &volume, const QVector3D &world,
                  int *i, int *j, int *k);

/// 将切片视口点击映射为世界坐标；失败返回 false。
bool mapClickToWorld(const VolumeSnapshot &volume, int viewType, double slicePosition,
                     double itemX, double itemY, double viewportWidth, double viewportHeight,
                     QVector3D *worldOut, int *voxelOut = nullptr,
                     const ImagePresentation &presentation = {});

/// 世界点相对当前切片的层差（体素层）。
int sliceDelta(const VolumeSnapshot &volume, int viewType, double slicePosition,
               const QVector3D &world);

/// Signed perpendicular distance (mm) from a world point to the current slice plane.
double signedSliceDistanceMm(const VolumeSnapshot &volume, int viewType,
                             double slicePosition, const QVector3D &world);

/// Half thickness of the displayed single-slice slab in millimeters.
double sliceSlabHalfThicknessMm(const VolumeSnapshot &volume, int viewType);

/// Slicer-style control point visibility: projection is off, so only the current slab is visible.
bool isPointDisplayableOnSlice(const VolumeSnapshot &volume, int viewType,
                               double slicePosition, const QVector3D &world);

/// Intersect a non-coplanar world-space segment with the current slice plane.
bool segmentSlicePlaneIntersection(const VolumeSnapshot &volume, int viewType,
                                   double slicePosition, const QVector3D &a,
                                   const QVector3D &b, QVector3D *intersectionOut);

/// Clip a world-space segment to the physical slab represented by the current slice.
bool clipSegmentToSliceSlab(const VolumeSnapshot &volume, int viewType,
                            double slicePosition, const QVector3D &a,
                            const QVector3D &b, QVector3D *clippedA,
                            QVector3D *clippedB);

/// Clip an open or closed polyline into contiguous runs that are visible on the current slice.
std::vector<std::vector<QVector3D>> clipPolylineToSliceSlab(
    const VolumeSnapshot &volume, int viewType, double slicePosition,
    const std::vector<QVector3D> &points, bool closed);

/// 世界坐标 → 体数据图像物理坐标（ijk * spacing，供 VTK UserMatrix=dataToWorld 使用）。
std::array<double, 3> worldToImagePhysical(const VolumeSnapshot &volume,
                                           const QVector3D &world);

/// Project a world point onto the slice displayed by a 2D viewport and return
/// image physical coordinates (ijk * spacing) for VTK's data-to-world matrix.
std::array<double, 3> worldToSliceImagePhysical(const VolumeSnapshot &volume,
                                                int viewType,
                                                double slicePosition,
                                                const QVector3D &world);

/// 世界坐标 → 切片视口像素；失败返回 false。
bool worldToDisplay(const VolumeSnapshot &volume, int viewType, double slicePosition,
                    double viewportWidth, double viewportHeight, const QVector3D &world,
                    double *displayX, double *displayY,
                    const ImagePresentation &presentation = {});

} // namespace MarkupsPicker
