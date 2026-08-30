# LineIntersect 插件 (几何测量: 两线交点)
add_library(LineIntersect STATIC)
target_sources(LineIntersect PRIVATE
    LineIntersect.h
    LineIntersect.cpp
)
target_link_libraries(LineIntersect PUBLIC
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
