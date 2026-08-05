# 光索科技 CT 影像工作站

基于 Qt 6 QML、VTK 和 ITK 的 CT/X 线桌面影像工作站。界面采用四页任务流：患者确认、联锁检查、扫描范围、影像工作站。QML 负责表现与交互，C++ 负责流程状态、DICOM 数据、ITK 算法和 VTK 渲染。

> 当前版本用于工程开发和交互验证，尚未完成医疗器械注册、临床验证、剂量控制、设备通信和诊断级精度验证，不得用于临床诊断。

## 当前能力

- 后台递归扫描目录，识别 `.dcm` 和无扩展名 DICOM；
- 按患者、检查、序列展示 CT、DX、CR 等候选数据；
- 显示患者、检查、序列、模态、图像方向、体素尺寸和物理间距；
- 加载 CT 三维序列，以及单幅或正侧位成对 X 线影像；
- Axial、Coronal、Sagittal MPR 和 GPU 体绘制；
- 窗宽窗位、缩放、平移、切片浏览、旋转、翻转和近似物理长度测量；
- ITK 二值阈值分割和 Connected Threshold 种子生长；
- 切片分割叠加、三维分割表面、MIP 和 Z 轴裁剪；
- 多个 Volume 驻留、切换、重命名、显隐和移除；
- 将当前数据的原始 DICOM 实例完整复制到指定目录。

当前“DICOM 导出”只是原始实例副本，不会生成修改后的派生 DICOM，也不会写回标签或像素数据。DICOM SEG、Secondary Capture、匿名化和审计仍属于后续工作。

## 推荐环境

已验证的完整医学后端使用 MSVC：

```text
Qt       F:/Qt/6.10.3/msvc2022_64
MSVC     Visual Studio 2026 + v143 14.44.35207
VTK      E:/A/GuangSuo/VTK_INSTALL/install_debug
ITK/RTK  E:/A/GuangSuo/ITK_INSTALL/install_debug
CUDA     C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.8
```

现有 VTK、ITK、RTK 是 MSVC ABI 的 Debug 二进制库。MinGW 预设只编译同一套 QML 界面和兼容占位后端，不能直接链接这些 MSVC 库。若要在 MinGW 下启用医学能力，必须使用同一 MinGW 工具链重新编译全部医学三方库。

## 快速构建

从仓库根目录执行：

```powershell
cmake --preset msvc-v143-debug --fresh
cmake --build --preset msvc-v143-debug --parallel 4
ctest --preset msvc-v143-debug --output-on-failure
```

输出不会写入源码目录：

```text
E:/A/CT_UI-build/msvc-v143-debug/artifacts/Debug/bin/CT_UI.exe
E:/A/CT_UI-build/msvc-v143-debug/artifacts/Debug/tests/CT_UI_core_tests.exe
```

只验证 QML/UI 的 MinGW 构建：

```powershell
cmake --preset mingw-ui-debug --fresh
cmake --build --preset mingw-ui-debug --parallel 4
ctest --preset mingw-ui-debug --output-on-failure
```

### Qt Creator

1. 打开仓库根目录的 `CMakeLists.txt`。
2. 选择 `CT UI - MSVC v143 Debug (Recommended)` CMake Preset。
3. 确认构建目录为 `E:/A/CT_UI-build/msvc-v143-debug`。
4. 保持配置为 `Debug`，构建并运行 `CT_UI` 目标。

如果项目改名或移动后出现 `CMakeCache.txt directory is different`，在 Qt Creator 中执行“构建 > 清除 CMake 配置”，或删除仓库外对应的旧构建目录，再重新配置。不要手工修改 `CMakeCache.txt`。

## 第三方库如何引用

Qt、VTK、ITK、RTK 和 CUDA 都位于仓库外，没有复制进项目。`CMakePresets.json` 给当前开发机传入：

```cmake
CMAKE_PREFIX_PATH=F:/Qt/6.10.3/msvc2022_64
CT_MEDICAL_SDK_ROOT=E:/A/GuangSuo
```

`cmake/MedicalDependencies.cmake` 再从医学 SDK 根目录定位：

```text
VTK_DIR = <root>/VTK_INSTALL/install_debug/lib/cmake/vtk-9.5
ITK_DIR = <root>/ITK_INSTALL/install_debug/lib/cmake/ITK-5.4
```

CMake 通过三方库提供的 `*Config.cmake` 导入 include 目录、`.lib` 和传递依赖，而不是在项目代码里手写每一个库文件的绝对路径。构建后只把程序实际链接到的医学 DLL 复制到可执行文件目录；Qt 运行库由 Qt Creator 管理，发布时应使用相同 Qt 版本的 `windeployqt`。

RTK/CUDA 当前默认不链接，因为现阶段没有投影重建调用。只有设置 `CT_ENABLE_RTK_BACKEND=ON` 时，CMake 才要求 RTK 和 CUDA 12.8。

完整变量、查找优先级和故障排查见 [构建与三方库说明](docs/BUILD_AND_DEPENDENCIES.md)。

## 启动参数

```powershell
CT_UI.exe --demo --workstation
CT_UI.exe --dicom E:/path/to/dicom --workstation
```

`--demo` 生成内存演示体数据；`--dicom` 扫描指定 DICOM 文件或目录；`--workstation` 仅用于开发和自动化验证，直接推进到工作站页面。

## 真实数据测试

常规单元测试不依赖本地医学数据。以下环境变量启用可选集成测试：

```powershell
$env:CT_UI_TEST_DICOM_DIR = "E:/path/to/one/dicom/series"
$env:CT_UI_TEST_LIDC_ROOT = "E:/A/LIDC-IDRI-0001"
$env:CT_UI_TEST_XRAY_ROOT = "E:/A/X_TEST"
ctest --preset msvc-v143-debug --output-on-failure
```

测试数据不复制进仓库。CT 序列按体数据加载；DX/CR 等投摄影像按单个 SOP Instance 或可识别的正侧位对加载，不会被误组装为三维体。

## 代码分层

```text
CT_UI/
  cmake/                    CMake 模块：编译选项、依赖发现、输出与部署
  docs/                     架构、构建和三方库说明
  qml/
    components/             可复用基础组件
    pages/                  四页主任务流
    theme/                  颜色、字号和间距令牌
    workstation/            影像工作站左右面板
  src/
    application/            工作流状态与应用编排
    dicom/                  DICOM、医学数据节点和 ITK 算法
    rendering/              QML/VTK 边界、MPR、体绘制和叠加
    main.cpp                进程入口、类型注册和依赖装配
  tests/
    CMakeLists.txt           测试目标及运行环境
    core_tests.cpp           状态机、医学数据和可选真实数据测试
  CMakeLists.txt             只负责项目级装配
  CMakePresets.json          本机可复现的 MSVC/MinGW 配置入口

../CT_UI-build/              CMake 缓存、生成代码、目标文件、DLL、EXE 和测试结果
```

仓库内不应出现 `build/`、`artifacts/`、`CMakeFiles/`、DLL、EXE、OBJ 或本地 DICOM 数据。`.qtcreator/` 和 `CMakeUserPresets.json` 也只属于当前机器，不提交 Git。

对象职责、线程边界和数据所有权见 [架构说明](docs/ARCHITECTURE.md)。页面设计和后续阶段计划见 [设计说明](DESIGN_SPEC.md)。

## 开发约定

- 中文注释用于解释架构约束、线程边界、医学坐标和“为什么这样做”，不逐行翻译代码。
- QML 不直接持有 DICOM 像素、ITK Image 或 VTK 对象。
- 新页面或 QML 组件必须加入 `cmake/SourceFiles.cmake` 的 `CT_UI_QML_FILES`。
- 新测试只放在 `tests/`，测试数据通过环境变量从仓库外注入。
- 新三方依赖统一在 `cmake/` 中声明，不在业务源文件或个人 Qt Creator 配置里硬编码。
