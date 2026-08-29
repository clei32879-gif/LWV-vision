add_library(ColorConvert STATIC)
target_sources(ColorConvert PRIVATE ColorConvert.h ColorConvert.cpp)
target_link_libraries(ColorConvert PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
