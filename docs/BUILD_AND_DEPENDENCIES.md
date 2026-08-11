# 构建、目录与医学三方库

本文说明当前 Windows 开发环境中的 CMake 配置、三方库引用关系、构建产物位置和常见问题。架构职责见 [ARCHITECTURE.md](ARCHITECTURE.md)。

## 1. 源码与构建目录

项目使用 out-of-source build，源码和生成物必须分开：

```text
E:/A/CT_UI/                 Git 仓库，只存源码、QML、文档和测试代码
E:/A/CT_UI-build/
  msvc-v143-debug/          MSVC CMake 缓存和构建产物
  mingw-ui-debug/           MinGW UI 兼容构建产物
```

构建目录可以随时删除并由 CMake 重新生成。不要把以下内容移动回源码仓库：

```text
CMakeCache.txt  CMakeFiles/  artifacts/  Testing/
*.obj  *.lib  *.dll  *.exe  *_autogen/  .qtc/package-manager/
```

`.qtcreator/`、`.qtc_clangd/` 和 `CMakeUserPresets.json` 是本机 IDE 配置，也不提交 Git。测试源码位于 `tests/`，本地 DICOM 数据位于仓库外，并通过环境变量提供给测试。

## 2. CMake 文件职责

| 文件 | 职责 |
| --- | --- |
| `CMakeLists.txt` | Qt 查找、共享核心库、主程序装配、医学/兼容实现选择、测试子目录 |
| `CMakePresets.json` | 固定生成器、工具集、Qt 路径、SDK 根目录和输出目录 |
| `cmake/CompilerOptions.cmake` | 警告级别、UTF-8、统一产物目录 |
| `cmake/MedicalDependencies.cmake` | VTK/ITK/RTK/CUDA 的查找、校验和组件选择 |
| `cmake/RuntimeDeployment.cmake` | 复制目标实际依赖的医学运行时 DLL |
| `cmake/SourceFiles.cmake` | C++ 医学实现、兼容实现和 QML 文件清单 |
| `tests/CMakeLists.txt` | 测试目标、链接库、CTest 运行环境 |

根 `CMakeLists.txt` 保持为装配层。增加源码时先判断所属层，再修改 `SourceFiles.cmake`；增加测试目标时修改 `tests/CMakeLists.txt`；增加三方库时修改 `MedicalDependencies.cmake`。

## 3. 当前开发环境

```text
Qt MSVC          F:/Qt/6.10.3/msvc2022_64
Qt MinGW         F:/Qt/6.10.3/mingw_64
Visual Studio    F:/Vsiual Stdio
Medical SDK root E:/A/GuangSuo
VTK install      E:/A/GuangSuo/VTK_INSTALL/install_debug
ITK/RTK install  E:/A/GuangSuo/ITK_INSTALL/install_debug
CUDA             C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.8
```

三方库不在仓库内。`CMakePresets.json` 中的路径是这台开发机的绝对路径，用于让 Qt Creator 和命令行得到一致结果。换机器时应复制并修改不入库的 `CMakeUserPresets.json`，或在首次配置时传入新的缓存变量；不要改业务代码。

## 4. 依赖发现顺序

推荐预设设置：

```cmake
CT_MEDICAL_SDK_ROOT=E:/A/GuangSuo
```

由此派生：

```text
CT_VTK_INSTALL_DIR = <root>/VTK_INSTALL/install_debug
CT_ITK_INSTALL_DIR = <root>/ITK_INSTALL/install_debug
VTK_DIR             = <VTK install>/lib/cmake/vtk-9.5
ITK_DIR             = <ITK install>/lib/cmake/ITK-5.4
```

`VTK_DIR` 和 `ITK_DIR` 的查找优先级：

1. CMake 缓存中显式传入的 `VTK_DIR`/`ITK_DIR`；
2. 同名环境变量；
3. `CT_MEDICAL_SDK_ROOT` 派生目录；
4. 未设置根目录时，当前开发机的默认值 `E:/A/GuangSuo`。

项目没有在 `target_link_libraries()` 中逐个硬编码 `.lib` 的绝对路径。`find_package(... CONFIG)` 读取安装目录内的 `vtk-config.cmake` 和 `ITKConfig.cmake`，由它们提供 include 路径、导入目标、Debug 库位置和传递依赖。

## 5. 实际链接关系

### Qt 6.10.3

主程序使用 `Core`、`Gui`、`Qml`、`Quick`、`QuickControls2` 和 `Concurrent`。测试额外使用 `Test`。Qt 由 `CMAKE_PREFIX_PATH` 和 `Qt6_DIR` 定位。

### VTK 9.5

当前组件覆盖 `QQuickVTKItem`、MPR、图像映射、GPU 体绘制、MIP 和分割表面：

```text
CommonCore  CommonDataModel
FiltersCore  FiltersGeneral  FiltersGeometry
ImagingCore  ImagingColor
InteractionStyle
RenderingCore  RenderingOpenGL2  RenderingVolumeOpenGL2
GUISupportQtQuick
```

`vtk_module_autoinit()` 为已链接渲染模块生成正确的初始化代码。

### ITK 5.4.5

```text
ITKCommon          基础图像和数据结构
ITKIOGDCM          DICOM/GDCM 读取
ITKThresholding    二值阈值分割
ITKRegionGrowing   Connected Threshold 种子生长
```

### RTK 2.5 与 CUDA 12.8

RTK 已安装在 ITK 目录中，但当前工作站没有调用投影重建管线，因此默认：

```cmake
CT_ENABLE_RTK_BACKEND=OFF
```

此时 RTK 和 CUDA 不参与查找、编译或运行时部署。将来实现投影重建后，使用 MSVC v143 并启用：

```powershell
cmake --preset msvc-v143-debug --fresh -DCT_ENABLE_RTK_BACKEND=ON
```

启用后 CMake 才执行 `find_package(CUDAToolkit 12.8 REQUIRED)`，并把 `RTK` 加入 ITK Components。只有打开开关并不代表已实现重建 UI 或算法调用。

## 6. 主要 CMake 选项

| 变量 | 默认值 | 说明 |
| --- | --- | --- |
| `CT_ENABLE_MEDICAL_BACKEND` | MSVC `ON`，MinGW `OFF` | 启用真实 DICOM、ITK 和 VTK 实现 |
| `CT_ENABLE_RTK_BACKEND` | `OFF` | 查找并链接 RTK/CUDA，为未来重建功能预留 |
| `CT_UI_BUILD_TESTS` | `ON` | 配置并注册测试目标；普通构建仍不编译该目标 |
| `CT_UI_ENABLE_QML_CACHEGEN` | `OFF` | 生成 QML AOT C++ 缓存；Debug 关闭可明显减少编译量 |
| `CT_MEDICAL_SDK_ROOT` | `E:/A/GuangSuo` | VTK/ITK 安装目录的共同父目录 |
| `CT_VTK_INSTALL_DIR` | 根目录派生 | 覆盖 VTK 安装前缀 |
| `CT_ITK_INSTALL_DIR` | 根目录派生 | 覆盖 ITK/RTK 安装前缀 |
| `VTK_DIR` | 自动定位 | 直接指定 VTK Config Package 目录 |
| `ITK_DIR` | 自动定位 | 直接指定 ITK Config Package 目录 |

现有医学 SDK 只有 Debug 配置。使用医学后端时必须构建 Debug；要增加 Release，需要先用匹配的 MSVC 工具集构建 VTK、ITK、RTK Release 安装树，再增加对应 preset，不能把 Debug/Release 库混链。

## 7. MSVC 构建

推荐命令：

```powershell
cmake --preset msvc-v143-debug --fresh
cmake --build --preset msvc-v143-debug --target CT_UI --parallel 4
cmake --build --preset msvc-v143-debug --target CT_UI_core_tests --parallel 4
ctest --preset msvc-v143-debug --output-on-failure
```

`Visual Studio 18 2026` 是多配置生成器，构建和测试 preset 都显式选择 `Debug`。`--fresh` 会忽略旧 CMake 缓存，适合项目改名、移动或依赖路径变化后使用。

Qt Creator 操作：

1. 打开 `E:/A/CT_UI/CMakeLists.txt`。
2. 选择 `CT UI - MSVC v143 Debug (Recommended)` preset。
3. 检查生成器为 Visual Studio 2026、架构为 x64、工具集为 v143。
4. 检查构建目录为 `E:/A/CT_UI-build/msvc-v143-debug`。
5. 构建并运行 `CT_UI`。

`CT_UI_core` 是主程序和测试共享的静态库，`MedicalDataController` 只编译一次。`CT_UI_core_tests` 使用 `EXCLUDE_FROM_ALL`，Qt Creator 普通构建不会自动构建测试；需要测试时显式选择该目标。

Debug 默认 `CT_UI_ENABLE_QML_CACHEGEN=OFF`，QML 作为资源打包并在运行时加载，避免每个 QML 文件生成一个额外 C++ 编译单元。发布时可打开该选项换取更快的 QML 启动。

Qt Creator 自动创建的 Kit 可能使用 v145。当前 VTK/ITK 工作站可在已验证的 Kit 中使用，但 CUDA 12.8/RTK 开发仍以 preset 固定的 v143 为基线。

## 8. MinGW UI 兼容构建

```powershell
cmake --preset mingw-ui-debug --fresh
cmake --build --preset mingw-ui-debug --target CT_UI --parallel 4
cmake --build --preset mingw-ui-debug --target CT_UI_core_tests --parallel 4
ctest --preset mingw-ui-debug --output-on-failure
```

MinGW 构建选择 `medicaldatacontroller_stub.cpp` 和 `medicalviewportitem_stub.cpp`，保持 QML API 可编译和界面可运行，但不提供真实 DICOM、ITK、VTK 功能。MSVC `.lib` 与 MinGW ABI 不兼容，不能通过修改后缀或手工添加库目录解决。

## 9. DLL 部署

Windows 链接阶段使用 `.lib`，运行阶段需要对应 `.dll`。`ct_copy_medical_runtime_dlls()` 使用 CMake 的 `TARGET_RUNTIME_DLLS` 读取目标真实依赖，只复制已链接的 VTK/ITK/RTK DLL 到目标目录：

```text
E:/A/CT_UI-build/msvc-v143-debug/artifacts/Debug/bin/
```

Qt Creator 运行时会设置 Qt 环境。制作独立目录时，使用同版本：

```powershell
F:/Qt/6.10.3/msvc2022_64/bin/windeployqt.exe `
  E:/A/CT_UI-build/msvc-v143-debug/artifacts/Debug/bin/CT_UI.exe
```

不要只复制几个 `Qt6*.dll`，Qt Quick 还需要 QML 模块和 `platforms/qwindows.dll` 等插件。

## 10. 测试

测试目标在 `tests/CMakeLists.txt` 中独立声明，输出到：

```text
E:/A/CT_UI-build/msvc-v143-debug/artifacts/Debug/tests/CT_UI_core_tests.exe
```

默认测试包括工作流守卫、演示 Volume、阈值分割、Volume 节点生命周期和种子生长校验。真实数据测试在未配置数据路径时自动跳过：

```powershell
$env:CT_UI_TEST_DICOM_DIR = "E:/path/to/one/dicom/series"
$env:CT_UI_TEST_LIDC_ROOT = "E:/A/LIDC-IDRI-0001"
$env:CT_UI_TEST_XRAY_ROOT = "E:/A/X_TEST"
cmake --build --preset msvc-v143-debug --target CT_UI_core_tests --parallel 4
ctest --preset msvc-v143-debug --output-on-failure
```

CTest 不继承 Qt Creator 的运行环境，因此 `tests/CMakeLists.txt` 为测试进程补充 Qt 和医学 DLL 的 `PATH`。这只影响测试进程，不修改系统环境变量。

## 11. 常见问题

### CMakeCache 指向旧项目目录

症状：项目改名后提示 source directory 不一致。使用 `--fresh` 重新配置，或删除仓库外对应构建目录。不要移动旧缓存到新目录。

### 找不到 VTK/ITK Config

先检查以下文件真实存在：

```text
E:/A/GuangSuo/VTK_INSTALL/install_debug/lib/cmake/vtk-9.5/vtk-config.cmake
E:/A/GuangSuo/ITK_INSTALL/install_debug/lib/cmake/ITK-5.4/ITKConfig.cmake
```

若 SDK 移动，传入新的 `CT_MEDICAL_SDK_ROOT`；只有目录布局不一致时才分别设置 `VTK_DIR` 和 `ITK_DIR`。

### 找不到 DLL 或程序启动即退出

先从 Qt Creator 运行，以区分 Qt 部署问题和医学 DLL 问题。再检查 `artifacts/Debug/bin` 是否包含实际链接的 VTK/ITK DLL；独立运行前执行 `windeployqt`。

### Debug/Release 不匹配

当前安装树含 `VTK-targets-debug.cmake` 和 `ITKTargets-debug.cmake`。不要用 Release 程序链接 Debug 医学库。需要 Release 时重新构建完整 Release SDK。

### LNK4099 提示缺少 ITK/VNL PDB

现有 ITK Debug 安装包的部分静态库没有同时安装 PDB。项目只对主程序和测试目标忽略已确认的 `LNK4099`，其他链接警告仍保留，避免 Qt Creator 被数十条重复警告刷屏。需要进入这些三方库调试时，应重新构建并安装匹配的 ITK/VNL PDB。

### MinGW 无法链接医学库

这是 ABI 不兼容，不是 `link_directories()` 缺失。使用 MSVC preset，或用 MinGW 从源码重新编译 Qt 匹配的 VTK/ITK/RTK。
