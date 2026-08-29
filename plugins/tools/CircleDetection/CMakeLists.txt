# CircleDetection 插件
add_library(CircleDetection STATIC)
target_sources(CircleDetection PRIVATE
    CircleDetection.h
    CircleDetection.cpp
)
target_link_libraries(CircleDetection PUBLIC
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
