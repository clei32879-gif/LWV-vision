add_library(QRCodeReader STATIC)
target_sources(QRCodeReader PRIVATE QRCodeReader.h QRCodeReader.cpp)
target_link_libraries(QRCodeReader PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
