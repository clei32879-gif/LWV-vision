add_library(HandEyeCalibration STATIC)
target_sources(HandEyeCalibration PRIVATE HandEyeCalibration.h HandEyeCalibration.cpp)
target_link_libraries(HandEyeCalibration PUBLIC
    VICore
    VIEngine
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
)
