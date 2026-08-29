# 测试
cmake_minimum_required(VERSION 3.21)

find_package(Qt6 REQUIRED COMPONENTS Concurrent)
add_executable(test_modbus test_modbus.cpp)
target_link_libraries(test_modbus PRIVATE
    VIHAL
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Network
    Qt6::Concurrent
)
add_test(NAME modbus_tcp COMMAND test_modbus)

add_executable(test_engine test_engine.cpp)
target_link_libraries(test_engine PRIVATE
    VIEngine
    VIUtils
    ${OpenCV_LIBS}
    Qt6::Core
    Qt6::Concurrent
)
add_test(NAME flow_engine COMMAND test_engine)
