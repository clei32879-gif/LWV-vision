# OpenIVS 专项评估报告

> 评估日期：2026-08-29 ｜ 源码：github.com/dl-cv/OpenIVS（已克隆到 `references/OpenIVS`，不入库）
> 结论先行：**架构参考价值 > 代码复用价值**。它验证了我们路线图的方向，并提供了几处可直接落地 borrowed 设计。

## 一、它是什么

深度视觉（DLCV）开源的工业视觉框架（Apache-2.0，240★），**C#/.NET 技术栈**，约 7.9 万行 C# + 16 个 XAML。定位是"配合 DLCV AI 平台 + 海康相机 + Halcon + Modbus PLC 快速搭建检测项目"。

**"半成品"体现在**：主程序 OpenIVSWPF 只有约 2400 行（单窗口 MVP 级别），没有流程编辑器 UI；推理依赖 DLCV 自家闭源 SDK（`.dvt/.dvst` 私有模型格式）；组件多而散（几十个 Demo/Test 工程），是"模块仓库 + 参考实现"，不是完整产品。

## 二、架构精华（值得抄的 5 个设计）

### 1. 双层图架构：设备编排图 与 算法流水线 分离 ⭐最重要

- **Sequence 图**（`DlcvCsharpApi/sequence/`）：设备级编排——等相机图、软触发、跑流程、解析产品、更新显示、Modbus 读写、存 JSON
- **Flow 图**（`DlcvCsharpApi/flow/` + `模块、流程与模型推理标准文档.md`）：算法流水线——预处理→模型推理→结果处理→模板匹配，甚至可以把整条流程存成"流程模型"文件（`.dvst/.dvso`）像加载模型一样加载

**对我们的意义**：这正是解决我们"节点连线不真正传数据、线性执行、检测逻辑和设备逻辑混在一起"的答案。CKVision 把所有工具塞一张图；OpenIVS 把"视觉算法链"和"设备时序"分开，各管各的。

### 2. 异步节点执行模型（`ISequenceNode`）

```csharp
Task<object> ExecuteAsync(node, ctx, executor);  // 节点异步执行
Task ArmAsync(...);  Task DisarmAsync();         // 运行前"武装"/解除
```

每个节点是 Task 异步的，配合 `WaitAllNode`/`WaitAnyNode` 实现图内分叉汇聚（fork-join），`RunFlowNode` 支持嵌套流程。我们的 FlowEngine 是同步线性执行——**阶段1 引擎改造直接按这个模型设计**（C++ 侧用 std::future/QThreadPool 实现同等语义）。

### 3. Arm/Disarm 武装模式

运行循环开始前先对全图"Arm"（相机准备、Modbus 连接预热等），停止时 Disarm。工业检测软件处理触发-就绪状态的干净做法。

### 4. 标准 JSON 结果 Schema（C#/C++ 双语言统一）

字段 snake_case（`category_id`、`box`、`angle`、`region`、`mask_rle`、模板匹配明细），类别/框/角度/区域/折线一套语义通用，且每次请求带耗时信息（供界面显示与性能定位）。**我们的 AI 推理节点和工具间数据传递建议直接对齐这套语义**，将来用户从竞品迁移零成本理解。

### 5. 集中式结果叠加绘制（`ResultOverlayDrawer.cs`）

所有节点的结果统一交给一个 Drawer 在图上画框/掩膜/文本，而不是每个工具各画各的。我们的 overlays 机制应该收敛成同样的一层。

## 三、对我们直接有用的代码/资产

| 资产 | 用法 | 价值 |
|------|------|------|
| `dlcv_infer_cpp_qt_demo/ImageViewerWidget`（645 行 C++/Qt） | 和我们 MultiViewWidget 对比学习（缩放/平移/叠加实现） | ★★★ 可部分移植（Apache-2.0 允许商用，保留版权声明） |
| `dlcv_infer_cpp_dll` 的 C++ API 头文件 | 我们 ONNX 推理节点的接口形状参考（Model/InferBatch/时间信息） | ★★ |
| `CameraManager/CameraManager.cs` | 将来接海康 MVS 时的参数设置参考 | ★★ |
| `ModbusApi/ModbusApi.cs` | Modbus 封装模式参考（我们用 libmodbus 更彻底） | ★ |
| `VirboxTest` + `hasp_*.ini` | 商业交付阶段的加密狗选型参考（他们用 Virbox） | ★（阶段6） |
| 文档工程实践 | 他们维护 AGENTS.md/CLAUDE.md 给 AI 协作开发用、有双语言统一的标准文档 | ★ 值得效仿，我们也建一份 |

## 四、明确不能用的

- **C# 代码本体**——技术栈不同（我们是 C++/Qt），只能看思路
- **`.dvt/.dvst` 模型格式与推理 SDK**——DLCV 私有闭源，我们的 AI 路线走 ONNX 开放格式
- **Halcon 绑定**——商业付费库，与我们开源路线冲突

## 五、落地动作（已并入完美化路线图）

1. 阶段1 引擎改造：按"Sequence 设备编排图 + Flow 算法流水线"双层设计，节点异步执行 + Arm/Disarm + WaitAll/WaitAny
2. 阶段4 AI 节点：JSON 结果 schema 对齐 OpenIVS 语义；接口形状参考 `dlcv_infer_cpp_dll`
3. 图像控件：移植/借鉴 ImageViewerWidget 的实现到 MultiViewWidget 升级（阶段2）
4. 本仓库建 AGENTS.md，延续其 AI 协作开发实践
