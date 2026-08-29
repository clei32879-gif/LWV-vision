add_library(ColorDetection STATIC)
target_sources(ColorDetection PRIVATE ColorDetection.h ColorDetection.cpp)
target_link_libraries(ColorDetection PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
