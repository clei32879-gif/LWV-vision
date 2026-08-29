add_library(ContourCompare STATIC)
target_sources(ContourCompare PRIVATE ContourCompare.h ContourCompare.cpp)
target_link_libraries(ContourCompare PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
