/**
 * @file ToolRegistry.cpp
 * @brief 工具注册表实现
 */

#include "ToolRegistry.h"
#include <QDebug>
#include <QMap>

namespace VisionInspector {

namespace {
// 内置工具中文说明表: 注册时填充 ToolMetaData.description.
// 每条说明均已逐一对照插件 execute() 真实行为核实; 新增工具时在此补一行.
QString builtinToolDescription(const QString& typeName) {
    static const QMap<QString, QString> kDescriptions = {
        // ---- 相机/图像源 ----
        { "CaptureImage",       "从相机抓帧、加载单文件或目录图像列表作为当前图像；用于流程取图源头" },
        { "UpdateView",         "将当前或输入图像写入指定视图槽位并记录索引；用于界面多视图展示" },
        { "SaveImage",          "将当前图像按PNG/JPG/BMP保存到指定目录并可加时间后缀；用于留存图像" },
        // ---- 检测识别 ----
        { "AIDetection",        "加载ONNX模型做YOLO检测或分类推理并按规则判OK/NG；用于AI缺陷与目标检测" },
        { "YOLOv8Detect",       "内置ONNX Runtime运行YOLOv8检测或分割模型输出目标框；用于深度学习检测" },
        { "BarcodeReader",      "用OpenCV条码检测器识别一维条码并输出文本/类型/位置；用于产线扫码读取" },
        { "QRCodeReader",       "用OpenCV检测解码二维码并输出文本与中心坐标；用于二维码内容读取" },
        { "OCR",                "分割字符后与教导模板做归一化匹配输出文本；适合固定印刷体喷码读取" },
        { "BlobAnalysis",       "二值化后按连通域统计斑点数量面积质心，可输出外接矩形椭圆；用于目标计数检测" },
        { "BlobClassify",       "对连通斑点按面积圆度长宽比孔数特征过滤并统计数量；用于形状分类判定" },
        { "BrightnessCheck",    "统计ROI内灰度均值与标准差并按上下限判定；用于光照曝光异常检查" },
        { "CircleDetection",    "沿圆周多卡尺提取亚像素边缘并鲁棒拟合圆心半径；用于圆类工件定位测量" },
        { "ColorDetection",     "按RGB/HSV/LAB阈值提取色块并统计数量面积；用于颜色有无检查" },
        { "ContourCompare",     "提取模板与目标轮廓按形状相似度比对判定；用于轮廓一致性检查" },
        { "ContourMatch",       "用Canny轮廓与Hu矩在图中匹配模板并输出多目标位置；用于形状定位" },
        { "EdgeCircleFind",     "用EdgeDrawing整图搜圆并支持Hough回退定位；常作流程首个工具引导补正" },
        { "EdgeDetection",      "在ROI内按灰度梯度剖面检测边缘点并按位置策略选取；用于找边定位" },
        { "EdgeSpacing",        "沿卡尺扫描线检测全部边缘并计算相邻间距；用于齿距间隙测量" },
        { "LineDetection",      "沿旋转ROI布置卡尺提取亚像素边缘并鲁棒拟合直线；用于直线定位测量" },
        { "MultiContourMatch",  "按面积区间与尺寸一致性过滤统计ROI内轮廓数量；用于多目标计数检查" },
        { "PixelStatistics",    "统计ROI灰度均值极值及阈值区间像素数与占比并判定；用于亮度分布检查" },
        { "ScanEdge",           "沿卡尺及多条平行扫描线检测全部亚像素边缘点；用于边缘位置批量提取" },
        { "ThreadInspection",   "将螺纹环极坐标展开检测牙数、缺牙、烂牙、毛刺与斜牙；用于螺纹外观专检" },
        { "VertexDetection",    "按水平垂直梯度乘积在ROI内搜索顶点并按策略选取；用于角点位置检测" },
        { "WidthDetection",     "对ROI二值化后按扫描行或列测量宽度；用于料宽与缺料宽窄检查" },
        // ---- 几何测量 ----
        { "AngleMeasure",       "根据顶点与两边端点计算夹角及各边方位角；用于角度类几何测量" },
        { "Caliper",            "沿旋转卡尺扫描线检测亚像素边缘，输出边缘位置或边缘对间距；用于尺寸测量" },
        { "CircleToCircle",     "计算两圆圆心距、边缘间隙并判定外离相交等关系；用于圆间距同心度测量" },
        { "CircleToLine",       "计算圆心到直线的垂直距离、有向距离与垂足；用于圆线位置测量" },
        { "DistanceMeasure",    "计算两点像素距离并按标定比例换算毫米；用于长度几何测量" },
        { "ExtendPoint",        "从起点沿指定方向角延伸给定长度求终点；用于构造辅助测量点" },
        { "LineIntersect",      "计算两条直线的交点坐标与夹角并判定平行；用于角点定位" },
        { "LineToLine",         "计算两条直线间的垂直距离与夹角；用于平行度间距测量" },
        { "PointToLine",        "计算点到直线的垂直距离、有向距离与垂足；用于位置偏差测量" },
        { "RotatePoint",        "将点绕指定旋转中心旋转给定角度求新坐标；用于旋转后特征点推算" },
        // ---- 定位补正/标定 ----
        { "Calibration",        "按两点、已知比例、棋盘格或多图法求像素毫米比例及内参畸变；用于测量标定" },
        { "CoordinateCalibration", "用至少3点仿射或4点透视拟合图像到机械坐标变换；用于像素坐标映射" },
        { "CoordSystem",        "由原点加角度或方向点定义坐标系参数写入上下文；供后续ROI跟随补正" },
        { "EndCorrection",      "清除位置补正建立的坐标系恢复原始图像坐标；与位置补正配对使用" },
        { "GrayscaleMatch",     "带角度缩放搜索的灰度模板匹配，金字塔加速支持多目标；用于精密定位" },
        { "HandEyeCalibration", "用AX=XB方程求解相机与机器人末端手眼矩阵并评估残差；用于机器人引导" },
        { "ImageCorrection",    "对图像做亮度均匀化、对比度增强或白平衡；用于光照不均预处理" },
        { "ImageUndistort",     "用相机标定结果或手动内参对图像去畸变重映射；用于畸变校正预处理" },
        { "PositionCorrection", "根据匹配的原点角度建立补正坐标系供后续ROI跟随；用于工件偏移旋转补偿" },
        { "ShapeMatch",         "提取目标轮廓按形状相似度匹配模板并搜索角度缩放；用于形状定位识别" },
        { "TemplateMatch",      "带角度缩放搜索的灰度模板匹配，支持多目标去重输出姿态；用于目标定位" },
        // ---- 图像处理 ----
        { "ColorConvert",       "在灰度、HSV、LAB、BGR间转换图像颜色空间；用于颜色检测预处理" },
        { "CropTransform",      "按ROI裁剪图像后执行镜像、旋转、缩放或平移；用于图像区域方向调整" },
        { "ImageCompare",       "将图像与模板像素差分并按面积统计缺陷区域；用于异物缺料对比检测" },
        { "ImageFilter",        "提供均值、中值、高斯、双边四种滤波去噪；用于检测前图像预处理" },
        { "ImageMerge",         "将最多四张图像水平垂直或网格拼接并融合重叠区；用于多相机视野合并" },
        { "ImageOperation",     "对两张图像逐像素做加减差分与或异或等运算；用于背景差分等场景" },
        { "ImageProcess",       "提供灰度转换、二值化、反色、亮度对比度调整；用于基础图像预处理" },
        { "Morphology",         "提供腐蚀膨胀开闭、梯度、顶帽黑帽七种形态学处理；用于二值图修饰" },
        { "Threshold",          "提供固定、OTSU自动及自适应阈值二值化；用于目标与背景分割" },
        // ---- 逻辑控制 ----
        { "Calculator",         "解析四则运算表达式求值并输出变量，可引用3个数据链接；用于简单数值计算" },
        { "CalculateVariable",  "解析含幂运算的四则表达式求值并写入变量，可引用3个数据链接；用于结果计算" },
        { "CompareText",        "按等于、包含、开头结尾或正则比较两段文本；用于报文与结果校验" },
        { "ConditionBranch",    "按表达式对上游工具数据做多条件比较判定并输出pass；用于流程分支" },
        { "DataDisplay",        "在图像上叠加显示文本、数据值及OK/NG状态；用于结果可视化" },
        { "DataJudge",          "按大于小于或范围对数据键值判定OK/NG并累计OK率；用于数值判定" },
        { "DataQueue",          "维护跨执行的命名先进先出队列，支持入队出队窥视清空；用于缓存批量数据" },
        { "Delay",              "按设定毫秒数延时且保持界面事件处理；用于等待设备动作或节拍控制" },
        { "GenerateText",       "用模板占位符替换上下文或全局变量生成文本；用于拼装报文标签" },
        { "GetVariable",        "读取跨流程共享的全局变量值并输出；配合设置变量实现流程间传值" },
        { "Loop",               "与循环结束配对标记循环起点，支持递增递减无限模式；用于重复执行流程段" },
        { "LoopEnd",            "与循环配对判定索引是否走完，未完则回跳循环起点；用于控制循环终止" },
        { "SetVariable",        "按类型将常量或数据链接值写入流程上下文或全局变量；用于流程间传值" },
        { "SplitText",          "按逗号等分隔符拆分文本并输出各段或指定段；用于解析通讯报文" },
        { "StopLoop",           "在循环体内按无条件、流程NG或数据满足条件提前退出循环；用于异常中断" },
        { "SystemTime",         "输出当前系统时间戳或统计流程耗时与执行间隔；用于记录与节拍测量" },
        // ---- 文件通讯 ----
        { "EthernetTool",       "以TCP客户端或服务器模式执行连接断开发送接收；用于与上位机Socket通讯" },
        { "FileWatch",          "检查信号文件存在、删除文件或按天数清理旧文件；用于上位机文件握手" },
        { "ModbusComm",         "通过已配置的PLC驱动读写保持寄存器线圈等四类数据；用于流程级PLC通讯" },
        { "ModbusRead",         "以内置Modbus TCP或RTU主站读取PLC寄存器与线圈；用于采集PLC状态" },
        { "ModbusWrite",        "以内置Modbus主站写PLC保持寄存器或线圈，支持多值与引用；用于写OK/NG信号" },
        { "PlaySound",          "播放系统提示音或指定wav文件；用于NG报警声音提醒" },
        { "PlcLink",            "按预定义寄存器映射执行启停、产量读写、拍照剔除等操作；用于设备联动" },
        { "SerialPort",         "执行串口打开关闭及ASCII或HEX收发；用于与串口设备指令交互" },
        { "SerialPortTool",     "配置COM口参数并打开或关闭串口，不含数据收发；用于建立串口连接" },
        { "TcpData",            "每次执行自动连接、收发后断开的TCP原子通讯；用于NG上报与一次性握手" },
        { "WriteText",          "把文本内容写入指定文件，支持覆盖追加及日期后缀；用于保存结果数据" },
    };
    return kDescriptions.value(typeName);
}
} // anonymous namespace

ToolRegistry& ToolRegistry::instance() {
    static ToolRegistry instance;
    return instance;
}

bool ToolRegistry::registerTool(const QString& typeName, const ToolMetaData& meta) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_registry.contains(typeName)) {
        qWarning() << "工具类型已存在, 跳过注册:" << typeName;
        return false;
    }

    ToolMetaData m = meta;
    if (m.description.isEmpty())
        m.description = builtinToolDescription(typeName);
    m_registry[typeName] = m;
    return true;
}

bool ToolRegistry::unregisterTool(const QString& typeName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_registry.remove(typeName) > 0;
}

ITool* ToolRegistry::createTool(const QString& typeName) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_registry.find(typeName);
    if (it == m_registry.end()) {
        qWarning() << "工具类型未注册:" << typeName;
        return nullptr;
    }

    if (!it.value().createFunc) {
        qWarning() << "工具创建函数为空:" << typeName;
        return nullptr;
    }

    return it.value().createFunc();
}

ToolMetaData ToolRegistry::metaData(const QString& typeName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_registry.value(typeName);
}

QList<ToolMetaData> ToolRegistry::allMetaData() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_registry.values();
}

QList<ToolMetaData> ToolRegistry::metaDataByCategory(ToolCategory category) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    QList<ToolMetaData> result;
    for (const auto& meta : m_registry) {
        if (meta.category == category)
            result.append(meta);
    }
    return result;
}

bool ToolRegistry::isRegistered(const QString& typeName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_registry.contains(typeName);
}

int ToolRegistry::count() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_registry.size();
}

void ToolRegistry::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_registry.clear();
}

} // namespace VisionInspector
