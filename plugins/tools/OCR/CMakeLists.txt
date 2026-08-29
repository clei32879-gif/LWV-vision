add_library(OCR STATIC)
target_sources(OCR PRIVATE OCR.h OCR.cpp)
target_link_libraries(OCR PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
