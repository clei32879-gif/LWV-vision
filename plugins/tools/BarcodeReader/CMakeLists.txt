add_library(BarcodeReader STATIC)
target_sources(BarcodeReader PRIVATE BarcodeReader.h BarcodeReader.cpp)
target_link_libraries(BarcodeReader PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)

# zxing-cpp 引擎 (可选, MIT): 存在即链接并定义宏
if(VI_HAS_ZXING)
    target_link_libraries(BarcodeReader PUBLIC ZXing)
    target_compile_definitions(BarcodeReader PRIVATE VI_HAS_ZXING)
endif()
