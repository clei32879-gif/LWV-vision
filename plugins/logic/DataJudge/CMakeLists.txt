# DataJudge 插件
add_library(DataJudge STATIC)
target_sources(DataJudge PRIVATE
    DataJudge.h
    DataJudge.cpp
)
target_link_libraries(DataJudge PUBLIC
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
