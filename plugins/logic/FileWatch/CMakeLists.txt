add_library(FileWatch STATIC)
target_sources(FileWatch PRIVATE FileWatch.h FileWatch.cpp)
target_link_libraries(FileWatch PUBLIC VICore VIEngine VIUtils Qt6::Core)
