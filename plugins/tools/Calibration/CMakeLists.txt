# Calibration 插件 (标定校准: 像素比例 + 真棋盘格角点标定)
add_library(Calibration STATIC)
target_sources(Calibration PRIVATE Calibration.h Calibration.cpp)
target_link_libraries(Calibration PUBLIC
    VICore
    VIEngine
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
)
