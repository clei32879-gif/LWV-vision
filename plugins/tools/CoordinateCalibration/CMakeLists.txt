add_library(CoordinateCalibration STATIC)
target_sources(CoordinateCalibration PRIVATE CoordinateCalibration.h CoordinateCalibration.cpp)
target_link_libraries(CoordinateCalibration PUBLIC
    VICore
    VIEngine
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
)
