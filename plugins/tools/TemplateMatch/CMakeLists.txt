# TemplateMatch 插件
add_library(TemplateMatch STATIC)
target_sources(TemplateMatch PRIVATE
    TemplateMatch.h
    TemplateMatch.cpp
)
target_link_libraries(TemplateMatch PUBLIC
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
