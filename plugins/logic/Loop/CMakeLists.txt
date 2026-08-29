add_library(Loop STATIC)
target_sources(Loop PRIVATE Loop.h Loop.cpp)
target_link_libraries(Loop PUBLIC VICore VIEngine VIUtils Qt6::Core)
