# 快速找圆 (EdgeDrawing整图搜索)
add_library(EdgeCircleFind STATIC)
target_sources(EdgeCircleFind PRIVATE
    EdgeCircleFind.h
    EdgeCircleFind.cpp
)
target_link_libraries(EdgeCircleFind PUBLIC
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
