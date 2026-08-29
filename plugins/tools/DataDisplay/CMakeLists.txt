add_library(DataDisplay STATIC)
target_sources(DataDisplay PRIVATE DataDisplay.h DataDisplay.cpp)
target_link_libraries(DataDisplay PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
