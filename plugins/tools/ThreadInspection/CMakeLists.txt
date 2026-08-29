# 螺纹检测专项工具 (阶段3拳头功能)
add_library(ThreadInspection STATIC)
target_sources(ThreadInspection PRIVATE
    ThreadInspection.h
    ThreadInspection.cpp
)
target_link_libraries(ThreadInspection PUBLIC
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
