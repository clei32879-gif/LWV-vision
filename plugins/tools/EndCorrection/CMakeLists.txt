add_library(EndCorrection STATIC)
target_sources(EndCorrection PRIVATE EndCorrection.h EndCorrection.cpp)
target_link_libraries(EndCorrection PUBLIC VICore VIEngine VIUtils Qt6::Core)
