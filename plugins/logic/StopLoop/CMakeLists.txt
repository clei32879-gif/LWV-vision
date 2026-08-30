add_library(StopLoop STATIC)
target_sources(StopLoop PRIVATE StopLoop.h StopLoop.cpp)
target_link_libraries(StopLoop PUBLIC VICore VIEngine VIUtils Qt6::Core)

