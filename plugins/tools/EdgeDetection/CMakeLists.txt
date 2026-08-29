# EdgeDetection 插件
add_library(EdgeDetection STATIC)
target_sources(EdgeDetection PRIVATE
    EdgeDetection.h
    EdgeDetection.cpp
)
target_link_libraries(EdgeDetection PUBLIC
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
