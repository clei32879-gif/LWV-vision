add_library(SystemTime STATIC)
target_sources(SystemTime PRIVATE SystemTime.h SystemTime.cpp)
target_link_libraries(SystemTime PUBLIC VICore VIEngine VIUtils Qt6::Core)
