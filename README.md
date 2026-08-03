# 光索科技 CT 影像工作站

基于 Qt 6 QML、VTK 和 ITK 的 CT/X 线桌面工作站阶段性实现。界面采用四页任务流：患者确认、联锁检查、扫描范围、影像工作站。QML 负责界面呈现，患者/流程状态、DICOM、分割和 VTK 渲染保留在 C++ 层。

> 当前版本用于工程开发与交互验证，尚未完成医疗器械注册、临床验证、剂量控制、设备通信和诊断级精度验证，不得用于临床诊断。

## 当前能力

- 导入单个 DICOM 文件，或从目录中选择实例最多的 DICOM 序列；
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
out/build/msvc-v143-debug/Debug/CT_UI.exe
out/build/mingw-ui-debug/CT_UI.exe
```

真实 DICOM 集成测试为可选项。将环境变量指向单个序列目录后运行 MSVC 测试，可验证导入、体数据和原始实例副本导出：

```powershell
$env:CT_UI_TEST_DICOM_DIR = "E:/path/to/one/dicom/series"
ctest --preset msvc-v143-debug
```

## 开发验收参数

以下参数用于 UI/渲染回归，不属于临床工作流：

```powershell
CT_UI.exe --demo --workstation
CT_UI.exe --demo --threshold-demo --workstation --screenshot out/ct-ui.png
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

详细的页面职责、组件映射和后续阶段计划见 [DESIGN_SPEC.md](DESIGN_SPEC.md)。
