# CircleToCircle 插件 (几何测量: 圆到圆距离/同心度)
add_library(CircleToCircle STATIC)
target_sources(CircleToCircle PRIVATE
    CircleToCircle.h
    CircleToCircle.cpp
)
target_link_libraries(CircleToCircle PUBLIC
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
