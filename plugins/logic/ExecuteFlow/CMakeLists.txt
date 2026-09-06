add_library(ExecuteFlow STATIC)
target_sources(ExecuteFlow PRIVATE ExecuteFlow.h ExecuteFlow.cpp)
target_link_libraries(ExecuteFlow PUBLIC VICore VIEngine VIUtils Qt6::Core)
