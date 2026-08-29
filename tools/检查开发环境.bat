@echo off
chcp 65001 >nul
title VisionInspector 开发环境检查
color 0A

echo ============================================
echo    VisionInspector 开发环境检查
echo ============================================
echo.

echo [1/4] 检查 Visual Studio 2022...
echo --------------------------------------------
where cl.exe >nul 2>nul
if %errorlevel%==0 (
    echo   ✅ C++编译器已找到
) else (
    echo   ⚠️  当前PATH中没有cl.exe (正常, 需要在VS命令行中运行)
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC" (
    echo   ✅ VS2022 Community 已安装
    dir /b "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC" 2>nul | findstr /r "^[0-9]" >nul && echo   ✅ MSVC编译工具已安装
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC" (
    echo   ✅ VS2022 Professional 已安装
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC" (
    echo   ✅ VS2022 Enterprise 已安装
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC" (
    echo   ✅ VS2022 BuildTools 已安装
) else (
    echo   ❌ 未找到VS2022, 请确认已安装"使用C++的桌面开发"工作负载
)
echo.

echo [2/4] 检查 Qt 6...
echo --------------------------------------------
set QT_FOUND=0
for %%d in (C D E F G) do (
    if exist "%%d:\Qt" (
        echo   ✅ 在 %%d:\Qt 找到Qt安装
        for /d %%v in (%%d:\Qt\6.*) do (
            echo   ✅ Qt版本: %%~nxv
            if exist "%%v\msvc2022_64" (
                echo   ✅ MSVC 2022 64-bit 组件已安装
                echo   📁 Qt路径: %%v\msvc2022_64
                set QT_FOUND=1
            ) else if exist "%%v\msvc2019_64" (
                echo   ✅ MSVC 2019 64-bit 组件已安装
                echo   📁 Qt路径: %%v\msvc2019_64
                set QT_FOUND=1
            )
        )
    )
)
if "%QT_FOUND%"=="0" (
    echo   ❌ 未找到Qt 6, 请确认安装时选择了MSVC 2022 64-bit
)
echo.

echo [3/4] 检查 OpenCV...
echo --------------------------------------------
if exist "E:\opencv\build\OpenCVConfig.cmake" (
    echo   ✅ OpenCV 已在 E:\opencv\build 找到
    echo   📁 OpenCV路径: E:/opencv/build
    if exist "E:\opencv\build\x64\vc16\bin" (
        echo   ✅ VC16 库文件存在
    ) else if exist "E:\opencv\build\x64\vc17\bin" (
        echo   ✅ VC17 库文件存在
    )
    if exist "E:\opencv\build\include\opencv2\opencv.hpp" (
        echo   ✅ 头文件存在
    )
) else if exist "E:\opencv\opencv\build\OpenCVConfig.cmake" (
    echo   ⚠️  OpenCV多了一层目录, 路径应为 E:\opencv\build
    echo   实际在: E:\opencv\opencv\build
    echo   建议把内层opencv文件夹移到E盘根目录
) else (
    echo   ❌ 未在 E:\opencv\build 找到 OpenCVConfig.cmake
    echo   请检查解压是否正确
    dir E:\opencv 2>nul
)
echo.

echo [4/4] 检查 CMake...
echo --------------------------------------------
where cmake >nul 2>nul
if %errorlevel%==0 (
    cmake --version | findstr /r "^[0-9]" >nul && for /f "tokens=3" %%a in ('cmake --version ^| findstr /r "^[0-9]"') do echo   ✅ CMake 版本: %%a
) else (
    echo   ⚠️  PATH中没有cmake (Qt Creator自带的CMake也可以用)
)
echo.

echo ============================================
echo 检查完成!
echo ============================================
echo.
echo 如果以上都是 ✅, 就可以开始编译了:
echo   1. 打开 Qt Creator
echo   2. 文件 → 打开文件或项目
echo   3. 选择 本项目盘:\光选\CMakeLists.txt
echo.
pause
