add_library(PositionCorrection STATIC)
target_sources(PositionCorrection PRIVATE PositionCorrection.h PositionCorrection.cpp)
target_link_libraries(PositionCorrection PUBLIC VICore VIEngine VIUtils Qt6::Core)
