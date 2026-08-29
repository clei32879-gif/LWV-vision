add_library(WidthDetection STATIC)
target_sources(WidthDetection PRIVATE WidthDetection.h WidthDetection.cpp)
target_link_libraries(WidthDetection PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
