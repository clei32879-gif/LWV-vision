add_library(CaptureImage STATIC)
target_sources(CaptureImage PRIVATE CaptureImage.h CaptureImage.cpp)
target_link_libraries(CaptureImage PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
