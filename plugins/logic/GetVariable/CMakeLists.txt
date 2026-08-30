add_library(GetVariable STATIC)
target_sources(GetVariable PRIVATE GetVariable.h GetVariable.cpp)
target_link_libraries(GetVariable PUBLIC VICore VIEngine VIUtils Qt6::Core)
