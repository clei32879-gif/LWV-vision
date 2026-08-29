# BrightnessCheck 插件
add_library(BrightnessCheck STATIC)
target_sources(BrightnessCheck PRIVATE
    BrightnessCheck.h
    BrightnessCheck.cpp
)
target_link_libraries(BrightnessCheck PUBLIC
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
