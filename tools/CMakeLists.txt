# 工具类目标
add_executable(testdata_gen testdata_gen.cpp)
target_link_libraries(testdata_gen PRIVATE ${OpenCV_LIBS} Qt6::Core)
