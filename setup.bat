@echo off
echo ========================================
echo LW Vision - 项目设置脚本
echo ========================================
echo.

echo 检查环境...
echo.

REM 检查Qt
if exist "D:\Development\Qt\6.11.1\mingw_64\bin\qmake.exe" (
    echo [OK] Qt 6.11.1 已安装
) else (
    echo [警告] Qt 6.11.1 未安装
    echo 请安装Qt 6.11.1 + MinGW 13.1.0
)

REM 检查OpenCV
if exist "D:\Development\OpenCV\opencv\build\include\opencv2" (
    echo [OK] OpenCV 已安装
) else (
    echo [警告] OpenCV 未安装
    echo 请安装OpenCV 5.0.0 MSVC版本
)

REM 检查DVP2 SDK
if exist "C:\Program Files (x86)\DVP2 SDK CN" (
    echo [OK] DVP2 SDK 已安装
) else (
    echo [信息] DVP2 SDK 未安装 (可选)
)

echo.
echo ========================================
echo 配置完成！
echo ========================================
echo.
echo 要构建项目，请:
echo 1. 打开Qt Creator
echo 2. 打开 E:\guangxuan\CMakeLists.txt
echo 3. 配置构建目录
echo 4. 点击构建
echo.
pause
