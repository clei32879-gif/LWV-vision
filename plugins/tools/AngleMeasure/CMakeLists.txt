add_library(AngleMeasure STATIC)
target_sources(AngleMeasure PRIVATE AngleMeasure.h AngleMeasure.cpp)
target_link_libraries(AngleMeasure PUBLIC VICore VIEngine VIUtils Qt6::Core)
