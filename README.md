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
- Slicer 风格 Shift 联动浏览：鼠标所在位面保持不动，另外两个正交位面跟随交叉点更新；
- 后台完成目录扫描、DICOM 像素解码、阈值分割和种子生长，避免阻塞 GUI；
- ITK 二值阈值分割和 Connected Threshold 种子生长，支持 6/26 邻域及体积统计；
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

## CMake 构建步骤

项目要求 CMake 3.24 或更高版本，并采用 out-of-source build。所有 CMake 缓存、自动生成代码、目标文件、DLL、EXE 和测试结果都写入仓库同级的 `CT_UI-build/`，不会污染源码目录。

### 1. 确认预设和依赖路径

在 PowerShell 中进入仓库根目录并列出预设：

```powershell
Set-Location E:/A/CT_UI
cmake --version
cmake --list-presets
```

仓库提供两个 configure/build/test preset：

| Preset | 工具链 | 医学后端 | 用途 |
| --- | --- | --- | --- |
| `msvc-v143-debug` | Visual Studio 2026、x64、v143 | `ON` | 完整 DICOM、ITK、VTK 工作站，日常开发首选 |
| `mingw-ui-debug` | Qt MinGW 13.1、x64 | `OFF` | 只验证 QML/UI 和兼容占位后端，不提供真实医学处理 |

`CMakePresets.json` 中记录的是当前开发机的 Qt 和医学 SDK 路径。换机器时应通过不提交 Git 的 `CMakeUserPresets.json` 创建一个继承预设，并覆盖 `CMAKE_PREFIX_PATH`、`Qt6_DIR`、`CT_MEDICAL_SDK_ROOT` 和 `binaryDir`；不要把个人安装路径写进 C++ 或 QML 源文件。

### 2. 首次配置完整 MSVC 医学后端

首次构建、项目移动、工具链变化或三方库路径变化后使用 `--fresh`：

```powershell
cmake --preset msvc-v143-debug --fresh
```

这一步会完成以下工作：

1. 检查 Windows、64 位和 C++17 工具链。
2. 查找 Qt 6.10 的 Core、Gui、Qml、Quick、QuickControls2、Concurrent 和 Test 模块。
3. 通过 `cmake/MedicalDependencies.cmake` 查找 VTK 9.5 与 ITK 5.4。
4. 选择真实的 `medicaldatacontroller.cpp` 和 `medicalviewportitem.cpp`。
5. 生成 Visual Studio 工程到 `E:/A/CT_UI-build/msvc-v143-debug`。

依赖和源码未变化时，后续通常只需增量构建，不必每次重新运行 `--fresh`。

### 3. 构建主程序

```powershell
cmake --build --preset msvc-v143-debug --target CT_UI --parallel 8
```

构建顺序是：先编译共享静态库 `CT_UI_core`，再生成 QML 资源、编译渲染边界并链接桌面程序 `CT_UI`。CMake 会把实际链接到的 VTK/ITK 运行时 DLL 复制到程序目录。

主要产物：

```text
E:/A/CT_UI-build/msvc-v143-debug/artifacts/Debug/lib/CT_UI_core.lib
E:/A/CT_UI-build/msvc-v143-debug/artifacts/Debug/bin/CT_UI.exe
```

### 4. 构建并运行测试

测试目标使用 `EXCLUDE_FROM_ALL`，普通主程序构建不会自动编译测试，需要显式执行：

```powershell
cmake --build --preset msvc-v143-debug --target CT_UI_core_tests --parallel 8
ctest --preset msvc-v143-debug --output-on-failure
```

测试程序输出到：

```text
E:/A/CT_UI-build/msvc-v143-debug/artifacts/Debug/tests/CT_UI_core_tests.exe
```

只运行某个 CTest 用例或显示更详细日志时可以使用：

```powershell
ctest --preset msvc-v143-debug -R core_tests --output-on-failure --verbose
```

### 5. 运行程序

从命令行启动内存演示数据：

```powershell
& E:/A/CT_UI-build/msvc-v143-debug/artifacts/Debug/bin/CT_UI.exe `
  --demo --workstation
```

直接打开 DICOM 文件或目录：

```powershell
& E:/A/CT_UI-build/msvc-v143-debug/artifacts/Debug/bin/CT_UI.exe `
  --dicom "E:/path/to/dicom" --workstation
```

从 Qt Creator 外单独复制程序到新目录时，还需使用同版本 Qt 的 `windeployqt` 部署 Qt Quick 模块和平台插件；医学 DLL 已由构建规则复制，但 Qt 运行环境不会由手工复制一个 EXE 自动获得。

### 6. MinGW UI 兼容构建

MinGW 预设用于验证同一套 QML 和 C++ 接口能否编译，不会链接 MSVC ABI 的 VTK/ITK 库：

```powershell
cmake --preset mingw-ui-debug --fresh
cmake --build --preset mingw-ui-debug --target CT_UI --parallel 8
cmake --build --preset mingw-ui-debug --target CT_UI_core_tests --parallel 8
ctest --preset mingw-ui-debug --output-on-failure
```

该预设自动选择 `medicaldatacontroller_stub.cpp` 和 `medicalviewportitem_stub.cpp`。需要在 MinGW 下运行真实医学后端时，必须先用相同 MinGW 工具链重新构建 Qt 匹配的 VTK、ITK 和 RTK，不能直接链接现有 MSVC `.lib`。

### 7. Qt Creator 构建

1. 打开仓库根目录的 `CMakeLists.txt`。
2. 选择 `CT UI - MSVC v143 Debug (Recommended)` CMake Preset。
3. 确认架构为 x64、工具集为 v143、配置为 Debug。
4. 确认构建目录为 `E:/A/CT_UI-build/msvc-v143-debug`。
5. 日常开发构建并运行 `CT_UI`；需要回归测试时再单独构建 `CT_UI_core_tests`。

Debug 默认设置 `CT_UI_ENABLE_QML_CACHEGEN=OFF`，减少修改 QML 后产生的 C++ 编译量。制作发布构建时可以启用该选项，但必须同时准备与工具链和配置完全匹配的 Release 版 VTK/ITK 安装树。

如果项目移动后出现 `CMakeCache.txt directory is different`，请运行 `cmake --preset msvc-v143-debug --fresh`，或在 Qt Creator 中执行“构建 > 清除 CMake 配置”。不要手工编辑或移动旧 `CMakeCache.txt`。

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
cmake --build --preset msvc-v143-debug --target CT_UI_core_tests --parallel 4
ctest --preset msvc-v143-debug --output-on-failure
```

测试数据不复制进仓库。CT 序列按体数据加载；DX/CR 等投摄影像按单个 SOP Instance 或可识别的正侧位对加载，不会被误组装为三维体。

## 源码目录与分层

### 完整目录结构

```text
CT_UI/
├─ CMakeLists.txt                 项目入口，只负责目标和模块的总装配
├─ CMakePresets.json              MSVC/MinGW configure、build、test 预设
├─ DESIGN_SPEC.md                 页面设计与后续阶段规划
├─ README.md                      项目入口文档
├─ cmake/
│  ├─ CompilerOptions.cmake       编译警告、UTF-8、链接选项和产物目录
│  ├─ MedicalDependencies.cmake   VTK、ITK、RTK、CUDA 查找与后端开关
│  ├─ RuntimeDeployment.cmake     复制目标实际依赖的医学运行时 DLL
│  └─ SourceFiles.cmake           C++ 后端分组和 QML 文件清单
├─ docs/
│  ├─ ARCHITECTURE.md             对象职责、线程边界和数据所有权
│  └─ BUILD_AND_DEPENDENCIES.md   依赖发现、部署和构建故障排查
├─ qml/
│  ├─ Main.qml                    应用窗口、页面切换和全局对象接入
│  ├─ theme/
│  │  └─ Theme.qml                颜色、字号、圆角和间距令牌
│  ├─ components/
│  │  ├─ ActionButton.qml         工具栏通用按钮
│  │  ├─ StatusPill.qml           状态标签
│  │  ├─ SeriesSelectionDialog.qml DICOM 序列选择对话框
│  │  └─ ViewportPane.qml         视口外壳、鼠标键盘交互、切片滑块
│  ├─ pages/
│  │  ├─ PatientPage.qml          患者确认
│  │  ├─ SafetyPage.qml           联锁/安全检查
│  │  ├─ ScanRangePage.qml        扫描范围设置
│  │  └─ WorkstationPage.qml      CT/X-ray 影像工作站总布局
│  └─ workstation/
│     ├─ DataPanel.qml            数据、导入导出和可见性控制
│     ├─ InspectorPanel.qml       窗宽窗位、分割和显示参数
│     └─ MarkupsTreePanel.qml     标点与测量对象树
├─ src/
│  ├─ main.cpp                    进程入口、QML 类型注册、控制器装配
│  ├─ application/
│  │  └─ workflowcontroller.*     四阶段工作流状态机和页面访问约束
│  ├─ annotation/
│  │  └─ annotationcontroller.*   QML 标注 API、数据集场景绑定和通知
│  ├─ markups/
│  │  ├─ markupsnode.*            Point List、Line、Angle、Curve 数据节点
│  │  ├─ markupsscene.*           活动工具、节点生命周期和编辑操作
│  │  ├─ markupsmetrics.*         长度、角度和曲线采样/度量
│  │  └─ markupspicker.*          体素、世界、切片和显示坐标变换
│  ├─ dicom/
│  │  ├─ dicompresentation.h      投影显示方向和灰度呈现辅助逻辑
│  │  ├─ medicaldatacontroller.h  医学数据、Volume、分割的稳定公共 API
│  │  ├─ medicaldatacontroller.cpp 真实 DICOM/ITK 医学后端
│  │  └─ medicaldatacontroller_stub.cpp MinGW UI 兼容后端
│  └─ rendering/
│     ├─ medicalviewportitem.h    QML 可见的统一视口接口
│     ├─ medicalviewportitem.cpp  VTK MPR、体绘制、标注叠加和拾取
│     └─ medicalviewportitem_stub.cpp 无 VTK 时的兼容绘制实现
└─ tests/
   ├─ CMakeLists.txt              测试目标、链接依赖和 CTest 环境
   └─ core_tests.cpp              工作流、DICOM、分割、坐标和标注测试

../CT_UI-build/                   仓库外的全部构建缓存与产物
```

### 分层职责

| 层 | 目录/目标 | 职责 | 不应承担的职责 |
| --- | --- | --- | --- |
| 启动与装配层 | `src/main.cpp`、根 `CMakeLists.txt` | 创建控制器、注册 QML 类型、选择后端、组装目标 | DICOM 解析、渲染算法或业务状态实现 |
| 表现与交互层 | `qml/` | 页面布局、工具选择、鼠标键盘事件、属性绑定和状态展示 | 持有 ITK Image、VTK 对象或直接解析 DICOM |
| 应用流程层 | `src/application/` | 页面工作流和应用级状态约束 | 图像算法与渲染细节 |
| 标注适配层 | `src/annotation/` | 把 QML 操作映射到当前数据集的 Markups 场景 | VTK Actor 创建和医学像素解码 |
| 标注领域层 | `src/markups/` | 工具语义、节点、控制点、度量、拾取和坐标变换 | QML 页面和渲染线程对象 |
| 医学数据层 | `src/dicom/` | DICOM 扫描/解码、Volume 快照、ITK 分割、数据集生命周期 | QML 布局和直接操作 VTK 渲染器 |
| 渲染边界层 | `src/rendering/` | `QQuickVTKItem`、VTK 管线、相机、MPR/3D 和标注 Actor | 长期持有可变 ITK 对象或修改应用工作流 |
| 验证层 | `tests/` | 对共享核心库和可选真实数据执行自动化验证 | 被正式程序反向依赖 |

主要依赖方向如下，箭头表示调用或数据依赖：

```text
QML pages/components
  ├─> WorkflowController
  ├─> MedicalDataController ──> ITK/GDCM
  ├─> AnnotationController ──> MarkupsScene ──> Node/Metrics/Picker
  └─> MedicalViewportItem ──> VTK
             │
             ├─读取 VolumeSnapshot
             └─读取 AnnotationController 的只读渲染快照
```

QML 只通过 Qt 属性、信号和 `Q_INVOKABLE` 接口访问 C++。GUI 线程发布不可变像素/标注快照；VTK 对象只在 `QQuickVTKItem` 的渲染上下文中创建和访问。`CT_UI_core` 不依赖 QML 或 `QQuickVTKItem`，因此测试程序可以复用数据与标注逻辑而不启动完整窗口。

### CMake 目标与源码分组

`cmake/SourceFiles.cmake` 把文件分成后端无关部分和可替换后端，根 `CMakeLists.txt` 根据 `CT_ENABLE_MEDICAL_BACKEND` 选择实现：

| 源码集合/目标 | 内容 |
| --- | --- |
| `CT_UI_CORE_COMMON_SOURCES` | application、annotation、markups、医学控制器公共头文件 |
| `CT_UI_MEDICAL_CORE_SOURCES` | 真实 `medicaldatacontroller.cpp` |
| `CT_UI_COMPATIBILITY_CORE_SOURCES` | 兼容 `medicaldatacontroller_stub.cpp` |
| `CT_UI_RENDERING_COMMON_SOURCES` | 稳定的 `medicalviewportitem.h` |
| `CT_UI_MEDICAL_RENDERING_SOURCES` | VTK `medicalviewportitem.cpp` |
| `CT_UI_COMPATIBILITY_RENDERING_SOURCES` | 无 VTK 的 `medicalviewportitem_stub.cpp` |
| `CT_UI_QML_FILES` | 所有由 `qt_add_qml_module()` 打包的 QML 文件 |
| `CT_UI_core` | 主程序和测试共享的静态核心库；医学模式下链接 ITK |
| `CT_UI` | `main.cpp`、QML、渲染实现和 `CT_UI_core`；医学模式下链接 VTK |
| `CT_UI_core_tests` | 链接 `CT_UI_core` 与 Qt Test 的独立测试程序 |

新增文件时遵循以下规则：

1. 新的领域/控制器 C++ 文件加入对应的 `CT_UI_*_SOURCES` 集合。
2. 新 QML 页面或组件必须加入 `CT_UI_QML_FILES`，否则不会被打包进程序。
3. 真实后端与 stub 必须保持相同公共 API，使两个 preset 都能编译。
4. 新测试放入 `tests/`；本地 DICOM 数据通过环境变量注入，不复制进仓库。
5. 新三方依赖只在 `cmake/` 中声明，不在业务源码中写绝对库路径。

### 目录整洁约束

仓库内不应出现 `build/`、`artifacts/`、`CMakeFiles/`、`Testing/`、DLL、EXE、LIB、OBJ、压缩包或本地 DICOM 数据。`.qtcreator/`、`.qtc_clangd/` 和 `CMakeUserPresets.json` 只属于当前机器，已由 `.gitignore` 排除。

对象职责、线程边界和数据所有权见 [架构说明](docs/ARCHITECTURE.md)。更完整的依赖查找与构建故障排查见 [构建与三方库说明](docs/BUILD_AND_DEPENDENCIES.md)，页面设计和后续阶段计划见 [设计说明](DESIGN_SPEC.md)。

## 开发约定

- 中文注释用于解释架构约束、线程边界、医学坐标和“为什么这样做”，不逐行翻译代码。
- QML 不直接持有 DICOM 像素、ITK Image 或 VTK 对象。
- QML 只调用异步导入和分割接口；同步接口保留给命令行和自动化测试。
- 新页面或 QML 组件必须加入 `cmake/SourceFiles.cmake` 的 `CT_UI_QML_FILES`。
- 新测试只放在 `tests/`，测试数据通过环境变量从仓库外注入。
- 新三方依赖统一在 `cmake/` 中声明，不在业务源文件或个人 Qt Creator 配置里硬编码。
