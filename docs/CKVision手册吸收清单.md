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
- [ ] **zxing-cpp 集成评审**（一次解决 条码+DM码+QR 三件套，MIT协议，替代 OpenCV 内置 BarcodeReader）→ 建议列入下一批，需用户点头（新依赖规矩）
- [ ] anomalib→ONNX→AI节点（无监督缺陷检测，纯OK图训练）
- [ ] P1 余项：拟合线/拟合圆（从点集拟合，区别于卡尺式） / 多套标定切换
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
