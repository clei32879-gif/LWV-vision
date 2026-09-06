# 核心库 - 项目管理、插件管理、权限管理、配置管理
# 这是一个静态库, 被主程序和所有插件依赖

qt_add_library(VICore STATIC)

target_sources(VICore PRIVATE
    # 项目管理
    ProjectManager.h
    ProjectManager.cpp

    # 内置设备模板 (阶段6)
    DeviceTemplates.h
    DeviceTemplates.cpp

    # 插件管理
    PluginManager.h
    PluginManager.cpp

    # 用户权限管理
    UserManager.h
    UserManager.cpp

    # 全局变量
    GlobalVariables.h
    GlobalVariables.cpp

    # 配置管理
    ConfigManager.h
    ConfigManager.cpp
)

target_link_libraries(VICore PUBLIC
    Qt6::Core
    Qt6::Widgets
    nlohmann_json::nlohmann_json  # JSON库 (后续通过FetchContent引入)
)

# 公开头文件路径 (供其他模块引用)
target_include_directories(VICore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
