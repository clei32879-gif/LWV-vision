# YOLOv8检测工具 (检测+实例分割, ONNX Runtime)
if(VI_HAS_ONNXRT)
    add_library(YOLOv8Detect STATIC)
    target_sources(YOLOv8Detect PRIVATE
        YOLOv8Detect.h
        YOLOv8Detect.cpp
    )
    target_link_libraries(YOLOv8Detect PUBLIC
        VICore
        VIEngine
        VIHAL
        VIUtils
        ${OpenCV_LIBS}
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
    )
    vi_link_onnxrt(YOLOv8Detect)
endif()
