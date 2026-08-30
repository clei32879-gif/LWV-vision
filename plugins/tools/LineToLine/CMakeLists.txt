add_library(LineToLine STATIC)
target_sources(LineToLine PRIVATE LineToLine.h LineToLine.cpp)
target_link_libraries(LineToLine PUBLIC VICore VIEngine VIUtils Qt6::Core Qt6::Gui)
