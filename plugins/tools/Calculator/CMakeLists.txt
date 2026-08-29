add_library(Calculator STATIC)
target_sources(Calculator PRIVATE Calculator.h Calculator.cpp)
target_link_libraries(Calculator PUBLIC VICore VIEngine VIUtils Qt6::Core)
