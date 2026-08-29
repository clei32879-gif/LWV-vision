add_library(BarcodeReader STATIC)
target_sources(BarcodeReader PRIVATE BarcodeReader.h BarcodeReader.cpp)
target_link_libraries(BarcodeReader PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
