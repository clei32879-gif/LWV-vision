#include "UpdateView.h"
#include "../../../src/engine/ToolRegistry.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif

namespace VisionInspector {

PropertyDefList UpdateView::propertyDefs() const {
    return {
        PropertyDef::enumProp("viewIndex", "目标视图", {"视图1", "视图2", "视图3", "视图4"}, 0),
        PropertyDef::enumProp("imageSource", "图像来源", {"当前图像", "输入图像"}, 0),
    };
}

bool UpdateView::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    CvImagePtr input = getInputImage(context);
    if (!input || input->empty()) {
        // 即使没有图像也标记成功，视图保持不变
        setResultData("viewUpdated", false);
        setStatus(ToolStatus::OK);
        return true;
    }

    int viewIdx = propertyValue("viewIndex").toInt();
    // 将当前图像存入context，供MainWindow读取显示
    context.setData("updateView_index", viewIdx);
    context.setData("updateView_image", QVariant::fromValue((void*)new cv::Mat(*input)));

    setResultData("viewIndex", viewIdx);
    setResultData("viewUpdated", true);
    setStatus(ToolStatus::OK);
    return true;
#else
    setResultData("error", "需要OpenCV库"); setStatus(ToolStatus::NG); return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(UpdateView, "更新视图", VisionInspector::ToolCategory::Special)
