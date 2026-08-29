# BlobAnalysis 插件
add_library(BlobAnalysis STATIC)
target_sources(BlobAnalysis PRIVATE
    BlobAnalysis.h
    BlobAnalysis.cpp
)
target_link_libraries(BlobAnalysis PUBLIC
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
