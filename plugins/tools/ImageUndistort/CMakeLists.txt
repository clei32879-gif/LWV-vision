add_library(ImageUndistort STATIC)
target_sources(ImageUndistort PRIVATE ImageUndistort.h ImageUndistort.cpp)
target_link_libraries(ImageUndistort PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
