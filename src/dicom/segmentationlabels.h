#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// 分割标签色表与体绘制预设分开：色相贴近对应预设，饱和度与明度更高，
// 避免叠在骨骼/肺部体绘制上时和底图糊在一起。
namespace SegmentationLabels {

inline constexpr unsigned char SoftTissue = 1;
inline constexpr unsigned char Bone = 2;
inline constexpr unsigned char Lung = 3;
inline constexpr unsigned char Other = 4;
inline constexpr unsigned char kMaxLabel = 4;
inline constexpr int kTableSize = 5;

struct Entry {
    unsigned char id;
    const char *name;
    double r;
    double g;
    double b;
};

// 软组织：体绘制中段 (0.66, 0.36, 0.28) → 更亮橙红
// 骨骼：体绘制高段 (0.92, 0.76, 0.52) → 更饱和金黄
// 肺部：体绘制中段 (0.20, 0.42, 0.52) → 更亮青蓝
// 其他：胸部增强高段 (0.83, 0.66, 1.00) → 更饱和紫
inline constexpr std::array<Entry, 4> kEntries {{
    {SoftTissue, "软组织", 0.98, 0.45, 0.22},
    {Bone, "骨骼", 1.00, 0.84, 0.38},
    {Lung, "肺部", 0.20, 0.88, 0.90},
    {Other, "其他", 0.80, 0.46, 0.98},
}};

inline constexpr int clamp(int label)
{
    if (label < static_cast<int>(SoftTissue))
        return SoftTissue;
    if (label > static_cast<int>(kMaxLabel))
        return kMaxLabel;
    return label;
}

inline const Entry &entry(int label)
{
    return kEntries[static_cast<std::size_t>(clamp(label) - 1)];
}

inline void replaceLabelRegion(std::vector<unsigned char> &mask,
                               const std::vector<unsigned char> &region,
                               unsigned char label)
{
    if (mask.size() != region.size())
        mask.assign(region.size(), 0);
    if (label == 0)
        return;
    for (std::size_t index = 0; index < mask.size(); ++index) {
        if (mask[index] == label)
            mask[index] = 0;
        if (region[index] != 0)
            mask[index] = label;
    }
}

inline std::int64_t countForeground(const std::vector<unsigned char> &mask)
{
    std::int64_t count = 0;
    for (unsigned char value : mask)
        count += value != 0;
    return count;
}

} // namespace SegmentationLabels
