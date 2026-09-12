# CKVisionBuilder 手册吸收清单（对标打磨路线）

> 来源: references/ckvision_docs/CKVisionBuilder软件使用手册.txt（153KB 全本精读）
> 目的: 对照出 LWVision 工具差距，按优先级补齐。日期 2026-09-05

## 一、我们没有的工具（按价值排序）

### P0（产线刚需，短期补）
| CKVision 工具 | 说明 | 我们的落地建议 |
|---|---|---|
| 边缘凹陷 EdgeDepression | 检测边缘轮廓的凹陷/凸起（缺件、崩边类缺陷利器） | **✅已做(2026-09-05)**: 扫描线取边→鲁棒直线基线→偏离段聚合(深度/宽度/段数判定), 回归500+项全过 |
| 读取 DM 码 DataMatrix | DM 码与条码/QR 并列 | **受阻**: OpenCV5 objdetect 只含 一维码+QR (wechat_qrcode contrib 的 zxing 子集也只有 qrcode 族), 无 DM 解码。待选项: libdmtx(新依赖,需评审) / ZXing-C++ / 商用扫码库 |
| 光源控制 LightControl | 串口/IO 控制光源控制器（4通道亮度） | 新工具：串口发亮度指令，产线换型调光刚需 |
| 执行流程 ExecuteFlow | 子流程调用（流程复用/模块化） | 引擎 doExecute 支持子流程节点 + 参数传递 |
| 选择分支 SelectBranch | 多路 switch（现 ConditionBranch 只两路） | 扩展 ConditionBranch 或新工具：N 路 cases→跳转标签 |
| 提示对话框 MessageBox | 现场提示操作工 | **✅已做(2026-09-05)**: MessageBoxTool — invokeMethod投递UI线程, 模态可选/自动关闭/连续运行同框去重 |
| 代码编辑 Script | 脚本节点（JS/Lua 灵活逻辑） | 引入 QJSEngine 新工具 script 节点（无新编译依赖） |

### P1（完善工具矩阵）
| 工具 | 说明 | 建议 |
|---|---|---|
| 拟合线/拟合圆 FitLine/FitCircle | 从任意点集拟合（区别于卡尺式检测） | 复用 SubpixEdge 的鲁棒拟合库，输入点集属性 |
| 获取匹配轮廓/轮廓检测 | 轮廓级操作链 | MultiContourMatch 扩展输出轮廓点集 |
| 切换图像 SwitchImage | 多路图像源切换 | 新工具：按条件选命名图像槽（roadmap 已有此待办） |
| 切换校准/坐标变换 | 多套标定切换 | Calibration 多套存储 + 校准切换工具 |
| 状态输入/输出 | 读别的流程/全局状态位 | GetVariable/SetVariable 已覆盖大半，补 bool 位语义 |
| Modbus 读/写文本 | 报文型寄存器交互 | ModbusRead/Write 增加文本模式 |
| 数据记录/清除记录 | 工具级写检测记录 | DetectionRecorder 已有，包一层轻工具节点 |
| 定义数组/接收发送文本 | 数据结构/通讯补充 | SplitText/GenerateText 已可覆盖多数场景 |

### P2（三维测量整类目，等硬件）
CKVision 10 个 3D 工具（获取3D表面/拟合平面/检测高度/体积/向量运算/平面位姿）。依赖 3D 相机/激光轮廓仪硬件，**等有真实 3D 设备再立项**，接口可预留三维测量分类（已存在空分类）。

## 二、交互/参数设计吸收点

1. **工具属性四页签**（基本/参数/显示图形/基本设置页面）——我们已是四页签且更强（试执行即改即显）
2. **工具名称设置对话框**：批量重命名/改别名 → 可加到流程编辑器右键
3. **数据分析对话框**（2.7）：历史数据的统计分析视图 → 我们的 StatsPanel 可加"按项目统计"页
4. **显示图形页签**：每工具可配叠加图形颜色/开关 → 我们 overlays 已有，可在属性对话框加"显示选项"页
5. **十字线偏移**（CkvsRunCtrl SetCrossOffset）：图像中心十字线可偏移标定原点 → ImageViewWidget 加视图级十字线+偏移属性
6. **相机工具多品牌分立**（DSCamera/JAI/Basler/Baumer 各自工具）→ 我们走统一 ICameraDriver + GigE 通用驱动，架构更优，保持

## 三、其他资料吸收记录

- **软件整体画面.png**（CkvsRunCtrl demo）：右侧"获取数据"面板（工具名+数据ID手动查询结果值）是调试利器 → 待做：数据栏/RuntimeUI 加结果查询小面板
- **相机触发设置.png**（度申 DvpCam）：触发来源(软/硬) + "软件触发"按钮 + ROI裁剪(左上宽高) + 十字线显示 → **软触发按钮已做**（相机参数对话框，2026-09-05）；ROI偏移裁剪与十字线显示待做（偏移寄存器地址需按相机型号确认）
- **筛选相机驱动服务.txt**：JAI GigE 过滤驱动（FilterDriver）安装进网卡属性→服务→从磁盘安装 inf，可显著降低高帧率丢帧。已补入 工控机部署指南.md
- **异常情况处理方案.txt**：加密狗/相机/部署 20+ 现场坑（已在阶段5吸收）
- **CkvsRunCtrl开发手册.txt**：运行控件 API 面（LoadProject/Execute/GetResultDouble/十字线偏移/OnNotifyTool 消息）——印证我们 RuntimeUI + DetectionRecorder 的方向；"通知控件"消息机制值得做成工具（NotifyTool→触发 DIY 界面动作）
- **涂胶/焊缝跟踪/3D拼接**：专用工艺工具（胶宽/断胶判定、焊缝偏差、3D拼接），属垂直场景，等对应产线需求再立项
- **Web工具使用.txt**：CKVision 的 Web 远程监视 → 对应阶段6"远程日志与升级"，等真机阶段
- **布局界面.png/演示流程.png/位置补正.png**：界面布局与我们主界面（工具箱|流程|图像|数据栏）同构，位置补正交互与我们的补正工具一致

## 四、执行状态

- [x] 相机管理：工位启用/禁用（标配N台可只启用部分）+ 相机参数面板（曝光/增益/分辨率/触发/像素格式）—— 2026-09-05
- [x] P0 工具：边缘凹陷 + 提示对话框 —— 2026-09-06
- [x] **P0 全部清零（2026-09-07）**：光源控制 LightControl / 执行流程 ExecuteFlow(子流程) / 选择分支 SelectBranch+BranchEnd / 脚本节点 ScriptNode(QJSEngine) —— 引擎测试10/11/12全过
- [x] P1 首项：切换图像 SwitchImage —— 2026-09-07
- [x] **zxing-cpp 已集成**（2026-09-07）: BarcodeReader 双引擎, DM码同步支持
- [x] anomalib 管线已通（2026-09-07/08 两轮实训）; 适用边界已明确(非周期纹理+固定对齐), 待产线OK图
- [x] 切换图像 SwitchImage 已做(2026-09-07); 余: 拟合线/拟合圆(从点集拟合) / 多套标定切换
- [ ] P2：3D 测量类目（等硬件）

## 五、外部调研（2026-09-06，GitHub 实测检索 + 抖音@鹿鹿生-AI创业日记 主页实际打开采集）

### GitHub 实测检索结果（REST API 亲测，非想象）
| 项目 | 星数 | 对我们的价值 |
|---|---|---|
| open-edge-platform/**anomalib** | 6119★ | **无监督缺陷检测首选**——纯OK图即可训练（PaDiM/PatchCore/EfficientAD），导出ONNX接我们的AI节点即可，正是路线图"阶段4提升方向"的现成轮子 |
| xiahaifeng1995/PaDiM-Anomaly-Detection-Localization | 488★ | PaDiM 参考实现（算法对照用） |
| zxing-cpp/**zxing-cpp** | 2005★ | **DM码/条码识别解法**：C++17、MIT、CMake 一把梭，比 libdmtx(366★, LGPL) 更适合——一并解决条码+DM+QR 全家桶，识别率比 OpenCV 内置强 |
| dmtx/libdmtx | 366★ | DM码专用但 LGPL + C API 老旧，zxing-cpp 覆盖面更广，优先 zxing-cpp |
| share2code99/wood_defect_detection_yolo11 | 2★ | 木业检测 YOLO11 实现（活节/死节/裂纹/虫眼分类），可做训练脚本参照 |
| vnk8071/anomaly-detection-in-industry | 110★ | anomalib 全流程 UI 管线参照 |

### 抖音@鹿鹿生-AI创业日记 主页采集（浏览器实际打开逐条读取）
视频矩阵覆盖视觉创业全链路：机器视觉接单(批量检测/螺丝分类/缺陷检测免费寄样) → 相机网络配置 → **光源控制器接线(16★高热)** → 测量精度提升(7) → 自动化软硬件分工 → 传统CV vs AI对比 → 视觉定位+深度学习方案(14) → **线阵相机教程(149★爆款)** → 液体异物检测(5) → CODESYS系列(25)。
**未找到木业检测专视频**（主页当前可见内容以接单科普为主）——用户提到的"木业检测"视频可能在其他账号或已删除，如能提供链接可专项分析。
**对本项目的印证**（实践口径）：
1. 接单型视觉创业的标配能力 = 定位+缺陷检测+PLC联动——与现有 84 工具+真机契约地址完全对口
2. "光源控制器接线"高热 → 印证 P0 光源控制工具的产线需求优先级
3. "传统CV vs AI"话题 → 双路线节点（传统工具+AI节点）是行业共识，我们架构已具备
4. 线阵相机是高热教程题材 → ICameraDriver 架构可扩展线阵驱动（待硬件到位）

## 六、anomalib 无监督缺陷检测实验（2026-09-07，实践验证全记录）

**管线已打通**（tools/train_anomalib.py）：
- anomalib 2.6.0（用户机已装）+ Padim/Patchcore/EfficientAd 多模型可切换
- **关键坑（实测发现）**：anomalib 2.x 导出 ONNX 会把 post_processor（归一化+clip）固化进去 → OK/NG 分数全变 1.0 不可用。解法：导出前临时摘掉 post_processor，ONNX 输出原始 pred_score + anomaly_map，阈值存 metadata.json 供 C++ 端使用
- EfficientAd 需 batch=1；Folder datamodule 用 val_split_mode="synthetic" 保证 memory bank 见全部 OK 图

**实验结论（诚实记录：合成螺纹图上不可分）**：
- 24 张合成螺纹 OK 图训练 Padim，OK 分数 382~760、NG 分数 528~740，**重叠不可分**（对齐/收紧裁剪/Patchcore/EfficientAd 均试过）
- 根因：**合成螺纹是周期性纹理，每张图相位不同 → 无监督模型把"相位差"当异常**，这是无监督方法在周期纹理上的经典盲区（与对齐无关——圆对齐后纹理相位仍随机）
- 行业对策（军哥方法论印证）：周期纹理类(螺纹/齿类)用**传统算法**（我们的 ThreadInspection 极坐标展开就是正确路线）；无监督方案适用于**非周期纹理+固定对齐**场景（木业板材/布匹/PCB/玻璃）
- **行动项**：管线保留，等真实木业图到位（非周期木纹+固定传送带对齐）直接 `python tools/train_anomalib.py --model EfficientAd --align --data <木业图目录>` 即可训练部署

## 七、anomalib 真实数据实训（2026-09-08，E:\图片素材 真实缺陷分类图）

**数据**：用户提供的真实螺钉头顶视图——短图/长图共 6 类缺陷（开裂77/头麻26/发黑30/头疤24/未点胶6）+ 良品 87 张，2448×2048 工业相机 BMP。
**训练**：Padim 512px，120 张良品入库。
**结果（诚实记录）**：OK 分数带 294~329 / NG 292~353，全图与 16×16 patch 级均**不可分**；对齐良好（OK 图间差分仅 2~3 灰度），排除位置漂移。
**根因**：开裂/发黑与正常头部纹理在 resnet18 浅层特征空间本身相近，全图/patch 分数无响应；且同类缺陷外观差异大（min/max 跨度大），无监督"记忆正常"模式抓不住这种"与正常相似但语义为缺陷"的目标。
**实践结论**：该数据走**有监督分类**路线正确——阶段4 自训的"缺陷分类.onnx"（263张7类，Top-1 79.7%）已经验证成功。无监督(Padim/Patchcore)适用前提是"缺陷与正常在纹理特征上可分"（划痕/污点/异物类），不适用"形状相似但语义缺陷"（开裂/发黑类）。
**管线资产保留**：tools/train_anomalib.py 对真实数据直接可用（含中文路径 np.fromfile+imdecode 解码），未来划痕/污点/木业类缺陷场景即插即训。

## 八、相机多类型适配架构（2026-09-08，面阵→全线扩展）

**目标**（用户明确要求）：适配不限于品牌，还包括 高像素/高分辨率/2D/3D/线扫/红外短波 各品类。

**已落地（接口层）**：
- `CameraInfo::SensorKind` 六品类：Area(面阵2D) / LineScan(线阵) / Stereo3D(双目结构光) / Laser3D(激光轮廓仪) / Thermal(红外热成像) / SWIR(短波红外)
- `CameraInfo.sensorInfo` 传感器描述字段（如"线阵8K"/"640×512 LWIR"）
- `CameraParams` 扩展：线阵（lineRate 行频 / scanLineCount / encoderSource 编码器触发）、3D（zRange 量程 / profileCount）、红外（emissivity 发射率 / temperatureDisplay）
- `ICameraDriver` 新增虚接口（默认实现=面阵行为，各驱动按需覆写）：sensorKind() / setLineRate() / setEncoderTrigger() / grabHeightMap() / readCenterTemperature()
- 相机参数对话框：线阵相机自动出现"行频"行（写 setLineRate 即时生效）；连接状态标注品类（[线阵]/[3D双目]/[激光3D]/[红外]/[短波红外]）

**驱动实现路径**（按优先级，等硬件到位）：
| 品类 | 推荐接入方式 | 参考实现 |
|---|---|---|
| 多品牌GigE/USB3 | **Aravis 0.8.31**（LGPL, 1272★, GenICam通用）| E:\图片素材\Aravis源码.zip 已下载解压 |
| 高像素面阵 | GigE 10GigE/25GigE 或 CoaXPress；现有 GigE 驱动寄存器协议通用，**大图带宽需巨型帧+过滤驱动** | 现有 GigECamera + JAI FilterDriver 文档 |
| 线阵 | GenICam 行频/编码器寄存器（接口已预留 setLineRate/setEncoderTrigger） | Aravis arvcamera.c |
| 激光3D | 厂商SDK（SICK/Gocator等）+ grabHeightMap 接口对接 | ICameraDriver::grabHeightMap |
| 红外/短波 | 厂商SDK（FLIR/Seek）或 GigE Vision 红外机型 | readCenterTemperature 接口 |

**协议资料**：GenICam SFNC（特征命名标准）确保各品类寄存器语义统一——线阵行频=LineScanLineRate、编码器=LineScanEncoderSource 等。

## 八·补、木材数据集实训（2026-09-08，公开数据集下载+四轮实验）

**数据获取（自己动手下载）**：Zenodo 记录 15679291 —— 东北林业大学《水性漆木材表面缺陷》（Nature Scientific Data 2025 正式发表，CC BY 4.0），335 张产线实拍水性漆木板（划痕/裂纹/气泡/孔洞，YOLO标注），下载 `OriginalDataset.zip`(18MB) 存 `E:\图片素材\木材缺陷数据集`。

**四轮实验**（Padim/Patchcore × 弱污染/纯净协议）：
| 轮次 | 训练集 | 结果 |
|---|---|---|
| 1 | 202张低污染(<5%) | AUROC 0.42（反向）|
| 2 | 59张纯净(<0.5%) | AUROC 0.485 |
| 3 | Patchcore 59纯净 | AUROC 0.36 |
| 4 | — | 全部不可分 |

**根因（对比成功案例的反思）**：该数据集每张图木纹走向/密度差异极大（无监督需要"正常纹理单一"），且拍摄包含多角度多光源；anomalib 官方 MVTec 协议里每类的好图来自**同一工位同一视角**。这不是算法问题，是**数据采集一致性**问题——正如真实螺钉数据的教训：**无监督检测对拍摄一致性要求极高，产线固定工位+固定光源是前置条件**。
**结论**：管线与脚本全部就绪且经过 4 轮实战检验（含数据划分/训练/导出/验证工具链），真实落地必须满足：①固定工位拍摄 ②同一光照 ③OK 图 50+ 张。用户产线满足这三个条件后，`python tools/train_anomalib.py --model Patchcore --align --data <目录>` 一步出模型。

## 九、外接相机DLL体系（2026-09-09，像创科那样"添加对应相机dll"）

**可行性研究结论（实测定案）**：
- CKVision 各品牌相机 DLL (CvsDSCamTool/CvsBaslerTool 等) 导出的是 **MSVC C++ 类 mangled 符号** (CreateTool/InitLibrary 工厂 + CvsVisionTool 虚类), MinGW 无法按 C++ ABI 直接调用 — **不能直接复用创科相机 DLL 本体**。
- 但本机已验证 **Basler PylonC_v8_0.dll 是纯 C 接口 (279 导出, 含 Enumerate/Open/Grab/Feature 全套)** — 厂商 SDK 的 C 接口 DLL 动态加载完全可行。

**已落地：统一外接相机 DLL 约定（`tools/camera_dll_sdk/lwcam_sdk.h`）**：
- 7 个 C 导出函数: LWCam_GetInfo / Enumerate / Open / Close / Grab / SetParam / GetParam
- 软件启动自动扫描 `cameras/` 目录加载；关键导出缺失自动跳过不影响主程序
- 厂商 SDK 封装成薄 DLL 即接入 (内部可用任意 SDK/语言约定, 静态链接避免运行库依赖)
- **示范 DLL**: SimCam (模拟相机, 移动条纹活画面, ctypes 端到端验证: 加载→枚举→开→抓两帧差异→关 全过)
- SDK 头文件+示例源码: `tools/camera_dll_sdk/` (sample_camera_dll.cpp, 30 行核心代码接入任意 SDK)

## 十、逐张截图对标（2026-09-09，45张资料PNG逐张过）

对照用户重命名的 45 张 CKVision 工具界面截图逐张核对，结论：
- **斑点分析**: 参数已全对标（阈值+自动阈值/检测类型黑白色/连通性四八/限定面积/椭圆+外接矩形特征），本批补齐 ROI 环形选项
- **检测顶点**: 参数已全对标（极性/检测位置/梯度阈值/滤波半宽/扫描宽度）；ROI 收敛为 无/矩形/菱形（创科原图圆形/圆环灰显）
- **检测边缘**: 参数已全对标（极性/边缘位置/梯度阈值/滤波半宽+曲线剖面预览）；ROI 同上收敛；曲线剖面预览是我们可加的显示增强(待做)
- **卡尺测量**: 补齐"扫描宽度"(多平行线平均剖面降噪)；创科的"上限/宽度/下限"判定即我们的数据判定页(更通用)
- **检测圆形**: 参数已对标（圆环ROI/半径/厚度/角度/范围/卡尺数量/容忍误差）
- **预先处理**: 我们的图像补正(亮度均匀化/对比度/白平衡)已覆盖
- **显示图形页签**（每工具可配 OK/NG 颜色与图形开关）: 记入待做 — 属性对话框加"显示选项"页
