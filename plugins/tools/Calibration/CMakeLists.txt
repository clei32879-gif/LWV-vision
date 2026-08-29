add_library(Calibration STATIC)
target_sources(Calibration PRIVATE Calibration.h Calibration.cpp)
target_link_libraries(Calibration PUBLIC VICore VIEngine VIUtils Qt6::Core)
