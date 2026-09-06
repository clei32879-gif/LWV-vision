# CKVisionBuilder 手册吸收清单（对标打磨路线）

> 来源: references/ckvision_docs/CKVisionBuilder软件使用手册.txt（153KB 全本精读）
> 目的: 对照出 LWVision 工具差距，按优先级补齐。日期 2026-09-05

## 一、我们没有的工具（按价值排序）

### P0（产线刚需，短期补）
| CKVision 工具 | 说明 | 我们的落地建议 |
|---|---|---|
| 边缘凹陷 EdgeDepression | 检测边缘轮廓的凹陷/凸起（缺件、崩边类缺陷利器） | 新工具：轮廓提取→与拟合直线/圆弧比对→凹陷深度+面积判定 |
| 读取 DM 码 DataMatrix | DM 码与条码/QR 并列 | QRCodeReader 扩展或新工具：`cv::DataMatrixDetector`(OpenCV objdetect) |
| 光源控制 LightControl | 串口/IO 控制光源控制器（4通道亮度） | 新工具：串口发亮度指令，产线换型调光刚需 |
| 执行流程 ExecuteFlow | 子流程调用（流程复用/模块化） | 引擎 doExecute 支持子流程节点 + 参数传递 |
| 选择分支 SelectBranch | 多路 switch（现 ConditionBranch 只两路） | 扩展 ConditionBranch 或新工具：N 路 cases→跳转标签 |
| 提示对话框 MessageBox | 现场提示操作工 | 新工具：QMessageBox(需UI线程投递)，含自动关闭超时 |
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
- [ ] P0 工具补齐（边缘凹陷/DM码/光源控制/执行流程/选择分支/脚本节点）
- [ ] P1 工具完善（拟合线圆/切换图像/多套标定）
