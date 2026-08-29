# LineDetection 插件
add_library(LineDetection STATIC)
target_sources(LineDetection PRIVATE
    LineDetection.h
    LineDetection.cpp
)
target_link_libraries(LineDetection PUBLIC
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
