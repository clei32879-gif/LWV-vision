# AI推理模块 (ONNX Runtime封装, 可选依赖)
if(VI_HAS_ONNXRT)
    qt_add_library(VIAI STATIC)
    target_sources(VIAI PRIVATE
        InferEngine.h
        InferEngine.cpp
    )
    target_link_libraries(VIAI PUBLIC
        VICore
        ${OpenCV_LIBS}
        Qt6::Core
        Qt6::Gui
    )
    vi_link_onnxrt(VIAI)
    target_include_directories(VIAI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endif()
