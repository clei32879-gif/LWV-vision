# AI检测工具 (ONNX推理, 阶段4)
if(VI_HAS_ONNXRT)
    add_library(AIDetection STATIC)
    target_sources(AIDetection PRIVATE
        AIDetection.h
        AIDetection.cpp
    )
    target_link_libraries(AIDetection PUBLIC
        VICore
        VIEngine
        VIHAL
        VIUtils
        VIAI
        ${OpenCV_LIBS}
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
    )
    vi_link_onnxrt(AIDetection)
endif()
