# CircleToLine 插件 (几何测量: 圆到线距离)
add_library(CircleToLine STATIC)
target_sources(CircleToLine PRIVATE
    CircleToLine.h
    CircleToLine.cpp
)
target_link_libraries(CircleToLine PUBLIC
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
