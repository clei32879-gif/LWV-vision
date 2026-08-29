#include "Morphology.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif

namespace VisionInspector {

PropertyDefList Morphology::propertyDefs() const {
    return {
        PropertyDef::enumProp("operation", "操作类型", {"腐蚀", "膨胀", "开运算", "闭运算", "梯度", "顶帽", "黑帽"}, 0),
        PropertyDef::enumProp("kernelShape", "核形状", {"矩形", "十字形", "椭圆"}, 0),
        PropertyDef::intProp("kernelW", "核宽度", 3, 1, 50),
        PropertyDef::intProp("kernelH", "核高度", 3, 1, 50),
        PropertyDef::intProp("iterations", "迭代次数", 1, 1, 20),
    };
}

bool Morphology::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) { setStatus(ToolStatus::NG); setResultData("error", "输入图像为空"); return false; }
    int op = propertyValue("operation").toInt();
    int shape = propertyValue("kernelShape").toInt();
    int kw = propertyValue("kernelW").toInt(), kh = propertyValue("kernelH").toInt();
    int iter = propertyValue("iterations").toInt();
    cv::MorphShapes mShape = (shape == 1) ? cv::MORPH_CROSS : (shape == 2) ? cv::MORPH_ELLIPSE : cv::MORPH_RECT;
    cv::Mat kernel = cv::getStructuringElement(mShape, cv::Size(kw, kh));
    int morphOp;
    switch (op) { case 0: morphOp = cv::MORPH_ERODE; break; case 1: morphOp = cv::MORPH_DILATE; break;
    case 2: morphOp = cv::MORPH_OPEN; break; case 3: morphOp = cv::MORPH_CLOSE; break;
    case 4: morphOp = cv::MORPH_GRADIENT; break; case 5: morphOp = cv::MORPH_TOPHAT; break;
    case 6: morphOp = cv::MORPH_BLACKHAT; break; default: morphOp = cv::MORPH_ERODE; break; }
    cv::Mat result;
    cv::morphologyEx(*input, result, morphOp, kernel, cv::Point(-1,-1), iter);
    setOutputImage(context, std::make_shared<CvImage>(result.clone()));
    setStatus(ToolStatus::OK); return true;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}


VI_REGISTER_TOOL(Morphology, "形态学操作", VisionInspector::ToolCategory::ImageProcess)
} // namespace VisionInspector