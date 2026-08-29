add_library(ImageCorrection STATIC)
target_sources(ImageCorrection PRIVATE ImageCorrection.h ImageCorrection.cpp)
target_link_libraries(ImageCorrection PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
