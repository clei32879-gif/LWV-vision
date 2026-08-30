add_library(EdgeSpacing STATIC)
target_sources(EdgeSpacing PRIVATE EdgeSpacing.h EdgeSpacing.cpp)
target_link_libraries(EdgeSpacing PUBLIC VICore VIEngine VIUtils  Qt6::Core)
