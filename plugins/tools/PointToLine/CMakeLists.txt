# PointToLine 插件 (几何测量: 点到线距离)
add_library(PointToLine STATIC)
target_sources(PointToLine PRIVATE
    PointToLine.h
    PointToLine.cpp
)
target_link_libraries(PointToLine PUBLIC
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
