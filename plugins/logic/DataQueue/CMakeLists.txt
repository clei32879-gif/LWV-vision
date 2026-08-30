add_library(DataQueue STATIC)
target_sources(DataQueue PRIVATE DataQueue.h DataQueue.cpp)
target_link_libraries(DataQueue PUBLIC VICore VIEngine VIUtils Qt6::Core)

