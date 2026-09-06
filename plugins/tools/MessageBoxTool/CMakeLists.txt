add_library(MessageBoxTool STATIC)
target_sources(MessageBoxTool PRIVATE MessageBoxTool.h MessageBoxTool.cpp)
target_link_libraries(MessageBoxTool PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Widgets)
