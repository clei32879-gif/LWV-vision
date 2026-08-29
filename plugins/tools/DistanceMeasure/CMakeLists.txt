add_library(DistanceMeasure STATIC)
target_sources(DistanceMeasure PRIVATE DistanceMeasure.h DistanceMeasure.cpp)
target_link_libraries(DistanceMeasure PUBLIC VICore VIEngine VIUtils Qt6::Core)
