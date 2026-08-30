add_library(WriteText STATIC)
target_sources(WriteText PRIVATE WriteText.h WriteText.cpp)
target_link_libraries(WriteText PUBLIC VICore VIEngine VIUtils Qt6::Core)
