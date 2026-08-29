add_library(UpdateView STATIC)
target_sources(UpdateView PRIVATE UpdateView.h UpdateView.cpp)
target_link_libraries(UpdateView PUBLIC VICore VIEngine VIUtils ${OpenCV_LIBS} Qt6::Core Qt6::Gui)
