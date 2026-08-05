# 架构与代码分层

## 1. 总体结构

```text
QML 表现层
  Main / Pages / Workstation / Components
                  |
                  | Qt 属性、信号、Q_INVOKABLE
                  v
C++ 应用层                         C++ 医学数据层
  WorkflowController                MedicalDataController
  四页任务流、访问守卫               DICOM、元数据、Volume、ITK 分割
                  |                         |
                  +------------+------------+
                               |
                               v
                         VTK 渲染适配层
                         MedicalViewportItem
                         MPR、投影、3D、叠加
                               |
                               v
                    外部 Qt / VTK / ITK / RTK / CUDA
```

QML 是视图，不负责保存大块医学像素、运行分割算法或创建 VTK 对象。C++ 控制器是状态和数据的唯一来源，渲染适配层把只读数据快照转换为 VTK 管线。

## 2. 目录职责

### `qml/`

- `Main.qml`：全局窗口、状态栏、页面容器和流程导航；
- `pages/`：患者确认、联锁检查、扫描范围、影像工作站四个任务页；
- `workstation/`：数据集列表和上下文工具面板；
- `components/`：按钮、状态标签、视口和序列选择器；
- `theme/`：颜色、字号、间距和尺寸令牌。

页面只调用控制器公开的属性、信号和命令。不能在 QML 中递归扫描目录、解析 DICOM 或复制原始文件。

### `src/application/`

`WorkflowController` 是四页任务流的唯一状态源，维护当前步骤、已到达的最大步骤和锁定状态。QML 只能访问已经完成或当前步骤，异常锁定时禁止继续推进。

后续接入患者身份核对、设备联锁和曝光状态时，应继续由应用层组合这些领域状态，而不是让不同页面各自维护一份布尔变量。

### `src/dicom/`

`MedicalDataController` 当前承担医学数据场景层：

- 递归发现和分组 DICOM；
- 读取患者、检查、序列和方向标签；
- 区分 CT 体数据与 DX/CR 投影数据；
- 管理多个 `LoadedVolumeNode`；
- 调用 ITK 阈值和 Connected Threshold 分割；
- 管理窗宽窗位、分割和种子状态；
- 导出原始 DICOM 实例副本。

这个目录名称沿用当前实现，但职责已经超过纯 DICOM IO。后续算法和持久化继续增长时，建议按行为拆为 `medical/io`、`medical/scene`、`medical/algorithms`，并保持 `MedicalDataController` 只做编排。本轮不搬动已验证实现，避免无功能收益的大规模路径变更。

### `src/rendering/`

`MedicalViewportItem` 是 QML 与 VTK 的唯一边界。它接收控制器快照与轻量显示属性，负责：

- Axial、Coronal、Sagittal 切片；
- X 线投影及正侧位切换；
- GPU 体绘制和 MIP；
- 分割叠加和三维表面；
- 窗宽窗位、裁剪、旋转、翻转和体素拾取。

QML 不应直接获得 `vtkRenderer`、`vtkImageData` 或 `vtkVolume` 指针。

### `tests/`

测试代码与正式程序目标分离。`core_tests.cpp` 验证应用状态和数据算法；真实 DICOM 测试由环境变量启用，不把患者数据或大文件提交到 Git。将来渲染回归测试应新增独立目标，避免让核心测试依赖可见窗口和 GPU。

## 3. 数据所有权

### `VolumeSnapshot`

包含尺寸、spacing、origin、DICOM LPS 方向矩阵和 `short` 像素。控制器生成新的快照后，通过 `std::shared_ptr<const VolumeSnapshot>` 给渲染层读取。发布后视为不可变。

### `MaskSnapshot`

与源 Volume 共享几何定义，像素值 `0/1` 表示背景和前景。分割参数变化时创建新快照，不允许渲染线程原地修改控制器数据。

### `LoadedVolumeNode`

表示工作区中的一个数据节点，保存 Volume、可选成对投影、Mask、元数据、源文件和显示状态。当前支持多个节点驻留，但渲染视口一次消费一个活动节点；还没有实现多个 Volume 同时融合或配准。

```text
MedicalDataController
  m_volumeNodes[]
    LoadedVolumeNode 0  CT volume + mask + metadata
    LoadedVolumeNode 1  DX image/pair + metadata
    LoadedVolumeNode 2  another CT volume + metadata
             |
             +--> selectedVolumeIndex --> immutable snapshots --> viewports
```

移除或切换节点必须由控制器完成，QML 只传节点索引和用户命令。

## 4. 线程边界

```text
GUI 线程
  QML 事件、控制器属性、Volume 节点状态
       |
       +-- importDicomAsync --> 后台扫描任务
       |                         目录遍历、DICOM 候选发现
       |                         结果排队回 GUI 线程发布
       |
       +-- immutable snapshot --> Qt Quick / VTK 渲染线程
                                  创建和更新 VTK 对象
```

- `QQuickVTKItem::setGraphicsApi()` 必须在 `QGuiApplication` 创建前执行；
- GUI 线程只更新 Qt 属性和只读快照引用；
- VTK 对象只在 `QQuickVTKItem` 的渲染线程中创建和访问；
- 后台扫描任务不能直接修改 QML 可见属性，应回到 GUI 线程发布结果；
- `m_snapshotMutex` 只保护快照交换，不应用来包围耗时 DICOM/ITK 操作。

这些规则用于避免渲染上下文冲突、悬空指针和界面长时间冻结。

## 5. 医学坐标与显示状态

体数据几何保留 DICOM LPS 方向矩阵。切片方向、体素拾取和距离估算必须使用 dimensions、spacing、origin 和 direction，不能仅按屏幕像素推断物理位置。

窗宽窗位属于活动数据节点的显示状态。二维切片映射灰度，三维体绘制则将窗宽窗位与预设共同转换为颜色和不透明度传递函数。分割显隐独立于原始 Volume 显隐，避免取消叠加时误删原始三维体。

DX/CR 是 DICOM 封装的二维投摄影像；CT 是按空间位置组成的三维切片序列。二者文件格式相同，但采集几何和可视化语义不同，因此工作站布局不同。

## 6. MSVC 与 MinGW 两种实现

公共头文件和 QML API 在两种构建中保持一致：

```text
MSVC + CT_ENABLE_MEDICAL_BACKEND=ON
  medicaldatacontroller.cpp
  medicalviewportitem.cpp
  VTK / ITK / DICOM

MinGW + CT_ENABLE_MEDICAL_BACKEND=OFF
  medicaldatacontroller_stub.cpp
  medicalviewportitem_stub.cpp
  仅 UI 兼容验证
```

选择发生在 `cmake/SourceFiles.cmake` 和根 `CMakeLists.txt`，不是运行时动态切换。兼容桩必须维持与真实实现相同的 QML 属性和命令，以便 UI 不分叉。

## 7. 新功能应放在哪里

| 功能 | 所属位置 |
| --- | --- |
| 页面布局、工具分组、空状态 | `qml/pages`、`qml/workstation` |
| 可复用按钮、标签、对话框 | `qml/components` |
| 工作流守卫、联锁组合、危险操作确认状态 | `src/application` |
| DICOM 读写、SEG、匿名化 | 后续 `src/medical/io`，当前暂放 `src/dicom` |
| 分割、重采样、配准 | 后续 `src/medical/algorithms`，当前暂放 `src/dicom` |
| Volume/Mask 场景节点 | 后续 `src/medical/scene`，当前暂放 `src/dicom` |
| MPR、体绘制、MIP、GPU 显示 | `src/rendering` |
| 投影重建、RTK/CUDA 管线 | 后续独立 `src/reconstruction` |
| 单元和真实数据集成测试 | `tests` |

## 8. 当前边界与后续拆分条件

当前阶段优先保证 UI 框架、DICOM 加载和 VTK/ITK 基线稳定。满足以下任一条件时再拆分 `MedicalDataController`：

- 新增 DICOM 派生对象写出或匿名化；
- 分割算法超过三类，且需要参数模型、撤销或异步队列；
- 多 Volume 需要同时融合、配准或独立显示属性；
- RTK 重建进入主流程；
- 单个实现文件继续增长并阻碍独立测试。

拆分时先固定控制器的 QML API 和快照契约，再逐个迁移内部服务，避免 UI 与医学内核同时重写。
