# 构建目录与医学三方库

## 目录职责

```text
CT_UI/
  cmake/          CMake 模块：依赖发现、编译选项、输出和源码清单
  docs/           构建与依赖说明
  qml/            当前 QML 界面
  src/            当前 C++ 业务、DICOM 和 VTK 渲染代码
  tests/          当前自动化测试

CT_UI-build/      CMake 缓存、目标文件、生成代码、DLL 和 EXE
```

`CT_UI-build` 与源码目录同级，不提交到 Git。删除它不会丢失源码，重新运行 CMake 即可生成。

## 当前机器的引用方式

项目没有把 Qt、VTK、ITK 或 RTK 复制进仓库。推荐预设使用以下外部绝对路径：

```text
Qt SDK root       F:/Qt/6.10.3/msvc2022_64
Medical SDK root  E:/A/GuangSuo
VTK install       E:/A/GuangSuo/VTK_INSTALL/install_debug
ITK/RTK install   E:/A/GuangSuo/ITK_INSTALL/install_debug
CUDA              C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.8
```

依赖发现集中在 `cmake/MedicalDependencies.cmake`。推荐预设只设置一个医学库根目录：

```cmake
CT_MEDICAL_SDK_ROOT=E:/A/GuangSuo
```

CMake 随后查找：

```text
VTK_DIR = <root>/VTK_INSTALL/install_debug/lib/cmake/vtk-9.5
ITK_DIR = <root>/ITK_INSTALL/install_debug/lib/cmake/ITK-5.4
```

查找优先级是：显式 `VTK_DIR`/`ITK_DIR`、同名环境变量、`CT_MEDICAL_SDK_ROOT` 派生路径。路径只用于配置和链接，不会把整个 SDK 复制到源码目录。

## 三个医学库的实际状态

### VTK 9.5

`find_package(VTK 9.5 CONFIG REQUIRED COMPONENTS ...)` 导入 VTK 的 CMake Targets。当前程序使用 `QQuickVTKItem`、二维切片、MPR、体绘制、MIP 和分割表面，因此链接 VTK QtQuick、Imaging、Rendering 和 Filters 模块。

### ITK 5.4.5

`find_package(ITK 5.4 CONFIG REQUIRED COMPONENTS ...)` 当前加载：

```text
ITKCommon
ITKIOGDCM
ITKThresholding
ITKRegionGrowing
```

它们分别支持基础图像对象、DICOM/GDCM 读取、阈值分割和种子生长。

### RTK 2.5

本机 ITK 安装中确实存在：

```text
lib/itkRTK-5.4.lib
bin/itkRTK-5.4.dll
lib/cmake/ITK-5.4/Modules/RTK.cmake
```

但当前阶段没有 RTK 重建调用，默认 `CT_ENABLE_RTK_BACKEND=OFF`，所以 RTK 和 CUDA 不会被强制链接。将来开始投影重建时，使用 MSVC v143 并显式设置：

```text
CT_ENABLE_RTK_BACKEND=ON
```

此时 CMake 才会把 `RTK` 加入 ITK Components，并要求 CUDA 12.8。

## 编译和运行时文件

CMake 使用 `.lib` 完成链接。构建结束后，`cmake/RuntimeDeployment.cmake` 根据目标的真实依赖，只把已链接的 VTK/ITK/RTK DLL 复制到可执行文件目录。Qt DLL 和 `platforms` 插件由 Qt Creator Kit 管理；制作独立安装包时使用同版本 `windeployqt`。

推荐 MSVC 输出：

```text
E:/A/CT_UI-build/msvc-v143-debug/artifacts/Debug/bin/CT_UI.exe
E:/A/CT_UI-build/msvc-v143-debug/artifacts/Debug/tests/CT_UI_core_tests.exe
```

## Qt Creator 正确打开方式

1. 打开 `E:/A/CT_UI/CMakeLists.txt`。
2. 在初始配置中选择 `CT UI - MSVC v143 Debug (Recommended)` Preset。
3. 确认构建目录为 `E:/A/CT_UI-build/msvc-v143-debug`。
4. 构建目标 `CT_UI`，再运行。

如果工程移动过位置并出现 `CMakeCache.txt directory is different`，不要编辑缓存内容。删除旧构建目录，或在 Qt Creator 执行“构建 > 清除 CMake 配置”，然后重新配置。

MinGW 预设只用于 QML/UI 兼容性验证。现有 VTK、ITK、RTK 是 MSVC ABI，不能直接由 MinGW 链接。
