# ConditionBranch 插件
add_library(ConditionBranch STATIC)
target_sources(ConditionBranch PRIVATE
    ConditionBranch.h
    ConditionBranch.cpp
)
target_link_libraries(ConditionBranch PUBLIC
    VICore
    VIEngine
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
