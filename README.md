# 光索科技 CT 影像工作站

基于 Qt 6 QML、VTK 和 ITK 的 CT/X 线桌面工作站阶段性实现。界面采用四页任务流：患者确认、联锁检查、扫描范围、影像工作站。QML 负责界面呈现，患者/流程状态、DICOM、分割和 VTK 渲染保留在 C++ 层。

> 当前版本用于工程开发与交互验证，尚未完成医疗器械注册、临床验证、剂量控制、设备通信和诊断级精度验证，不得用于临床诊断。

## 当前能力

- 后台递归扫描多层目录，识别 `.dcm` 及无扩展名 DICOM；
- 按患者、检查和序列展示候选 CT/X 线影像，支持模态筛选与患者搜索；
- 显示患者、检查、序列、模态、体素尺寸和物理间距；
- 载入 CT 三维序列和单幅 X 线影像；
- Axial、Coronal、Sagittal MPR 与 GPU 体绘制；
- 窗宽窗位、平移/缩放交互、切片浏览和近似物理长度测量；
- ITK 二值阈值分割与 Connected Threshold 种子生长；
- 二维分割叠加、三维分割表面、MIP 和 Z 轴裁剪；
- 将原始 DICOM 实例完整复制到指定目录。

当前“DICOM 导出”不会生成修改后的派生 DICOM，也不会写回标签或像素数据。后续需要单独实现 DICOM Secondary Capture、SEG 或其他派生对象写出，并增加匿名化策略。

## 工具链

推荐且已验证的完整构建：

```text
Qt       F:/Qt/6.10.3/msvc2022_64
MSVC     Visual Studio 2026 + v143 14.44.35207
VTK      E:/A/GuangSuo/VTK_INSTALL/install_debug
ITK/RTK  E:/A/GuangSuo/ITK_INSTALL/install_debug
CUDA     C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.8
```

仓库也提供 MinGW 兼容预设。由于现有 VTK/ITK 是 MSVC ABI 的二进制包，MinGW 预设仅构建同一套 QML 界面和后端占位实现。要在 MinGW 下启用医学后端，必须先用相同 MinGW 工具链重编 VTK、ITK 和 RTK。

## 构建与测试

### Qt Creator

Qt Creator 20 自动创建的 `Desktop Qt 6.10.3 MSVC2022 64bit` Kit 当前实际指向 `cl.exe 14.50.35717`（v145）。当前 DICOM、ITK 分割和 VTK/QML 工作站已经在该 Kit 下完成配置、链接和运行验证，可以直接用于本阶段开发。

首次重新打开项目时：

1. 选择 `CT UI - MSVC v143 Debug (Recommended)` CMake Preset；
2. 保持构建类型为 `Debug`；
3. 确认构建目录为 `E:/A/CT_UI-build/msvc-v143-debug`；
4. 构建并运行目标 `CT_UI`。

项目根目录的 `CT UI - MSVC v143 Debug (Recommended)` Preset 仍是发布和 CUDA/RTK 开发基线。它由 Visual Studio 2026 调用 v143 14.44.35207；CUDA 12.8 不支持当前 v145 编译器。只检查 QML 界面时也可将 `CT_ENABLE_MEDICAL_BACKEND=OFF`。

CMake 会在未手工指定时自动匹配以下实际配置文件，并在构建后把 VTK/ITK 的传递 DLL 复制到程序目录：

```text
E:/A/GuangSuo/VTK_INSTALL/install_debug/lib/cmake/vtk-9.5/vtk-config.cmake
E:/A/GuangSuo/ITK_INSTALL/install_debug/lib/cmake/ITK-5.4/ITKConfig.cmake
```

因此使用 Qt Creator Kit 或推荐预设时，不需要再手工填写 `VTK_DIR`、`ITK_DIR` 或医学库的运行时 `PATH`。Qt 的 DLL 与 `platforms` 插件由 Qt Creator Kit 管理；制作独立安装包时再使用同版本的 `windeployqt` 完整部署，不能只把 `Qt6*.dll` 零散复制到程序目录。

完整 MSVC Debug 构建：

```powershell
cmake --preset msvc-v143-debug
cmake --build --preset msvc-v143-debug
ctest --preset msvc-v143-debug
```

MinGW UI 兼容构建：

```powershell
cmake --preset mingw-ui-debug
cmake --build --preset mingw-ui-debug
ctest --preset mingw-ui-debug
```

程序位置：

```text
E:/A/CT_UI-build/msvc-v143-debug/artifacts/Debug/bin/CT_UI.exe
E:/A/CT_UI-build/mingw-ui-debug/artifacts/Debug/bin/CT_UI.exe
```

真实 DICOM 集成测试为可选项。既可以测试单个序列目录，也可以测试包含多个患者、CT 序列和 DX 投影的多层目录。测试覆盖递归发现、后台扫描、CT 体数据、无扩展名 DX、患者标签和原始实例副本导出：

```powershell
$env:CT_UI_TEST_DICOM_DIR = "E:/path/to/one/dicom/series"
$env:CT_UI_TEST_LIDC_ROOT = "E:/A/LIDC-IDRI-0001"
$env:CT_UI_TEST_XRAY_ROOT = "E:/A/X_TEST"
ctest --preset msvc-v143-debug
```

目录中存在多个可加载对象时，程序不会擅自选择切片最多的序列，而是在患者确认页弹出序列选择器。DX/CR 等投摄影像按单个 SOP Instance 加载为二维影像，不会误组装成三维体数据。

## 开发启动参数

可直接载入演示数据或指定 DICOM 目录并进入影像工作站：

```powershell
CT_UI.exe --demo --workstation
CT_UI.exe --dicom E:/path/to/dicom --workstation
```

## 代码结构

```text
qml/                          QML 应用壳层、四页流程和工作站组件
src/application/             流程状态机
src/dicom/                   DICOM、患者信息和 ITK 分割
src/rendering/               QQuickVTKItem、MPR、体绘制和分割表面
tests/core_tests.cpp          状态机、演示体、分割和可选真实 DICOM 测试
CMakePresets.json             MSVC 主构建与 MinGW UI 兼容构建
```

详细的页面职责、组件映射和后续阶段计划见 [DESIGN_SPEC.md](DESIGN_SPEC.md)。构建目录、绝对路径和 VTK/ITK/RTK 实际链接关系见 [docs/BUILD_AND_DEPENDENCIES.md](docs/BUILD_AND_DEPENDENCIES.md)。
