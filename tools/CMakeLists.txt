# 工具类目标
add_executable(testdata_gen testdata_gen.cpp)
target_link_libraries(testdata_gen PRIVATE ${OpenCV_LIBS} Qt6::Core)

# 授权签发工具 (供应商侧专用, 不进发布包; 私钥永不入仓库)
add_executable(license_gen license_gen.cpp)
target_link_libraries(license_gen PRIVATE VICore VIEngine VIHAL VIUtils Qt6::Core)

# CNG探针 (临时诊断用)
add_executable(lic_probe lic_probe.cpp)
target_link_libraries(lic_probe PRIVATE bcrypt)
