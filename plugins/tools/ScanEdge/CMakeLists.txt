add_library(ScanEdge STATIC)
target_sources(ScanEdge PRIVATE ScanEdge.h ScanEdge.cpp)
target_link_libraries(ScanEdge PUBLIC VICore VIEngine VIUtils  Qt6::Core)
