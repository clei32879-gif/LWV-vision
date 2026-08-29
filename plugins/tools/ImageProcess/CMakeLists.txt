# ImageProcess 插件
add_library(ImageProcess STATIC)
target_sources(ImageProcess PRIVATE
    ImageProcess.h
    ImageProcess.cpp
)
target_link_libraries(ImageProcess PUBLIC
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
