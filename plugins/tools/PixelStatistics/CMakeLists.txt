add_library(PixelStatistics STATIC)
target_sources(PixelStatistics PRIVATE PixelStatistics.h PixelStatistics.cpp)
target_link_libraries(PixelStatistics PUBLIC VICore VIEngine VIUtils  Qt6::Core)
