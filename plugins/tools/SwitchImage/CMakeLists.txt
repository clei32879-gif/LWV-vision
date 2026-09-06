add_library(SwitchImage STATIC)
target_sources(SwitchImage PRIVATE SwitchImage.h SwitchImage.cpp)
target_link_libraries(SwitchImage PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core)
