@echo off
REM ============================================================
REM LW Vision 立维视觉 - 一键编译脚本 (Windows)
REM 自动探测 Qt 安装位置，无需手动改任何路径
REM 用法: 双击运行，或命令行执行 build.bat [release]
REM ============================================================
setlocal enabledelayedexpansion
chcp 65001 >nul
cd /d "%~dp0"

set "BUILD_TYPE=Debug"
if /i "%~1"=="release" set "BUILD_TYPE=Release"

REM ---------- 1. 探测 Qt (在常见盘符下找 6.x\mingw_64) ----------
if defined QT_ROOT_DIR if exist "%QT_ROOT_DIR%\bin\qmake.exe" goto :qt_found

set "QT_ROOT_DIR="
for %%D in (C D E F) do (
    if not defined QT_ROOT_DIR (
        for /d %%Q in ("%%D:\Qt\6.*") do (
            if exist "%%Q\mingw_64\bin\qmake.exe" set "QT_ROOT_DIR=%%Q\mingw_64"
        )
    )
)
if not defined QT_ROOT_DIR (
    echo [错误] 未找到 Qt 6.x MinGW 版。请安装 Qt 6.2+ 并勾选 MinGW 组件,
    echo        或设置环境变量 QT_ROOT_DIR 指向 Qt 的 mingw_64 目录。
    pause & exit /b 1
)
:qt_found
echo [OK] Qt:   %QT_ROOT_DIR%

REM ---------- 2. 探测 MinGW 工具链 ----------
set "MINGW_BIN=%QT_ROOT_DIR:\mingw_64=\Tools%"
set "MINGW_BIN=!MINGW_BIN!\..\Tools"
REM Qt 官方安装布局: C:\Qt\6.x.x\mingw_64 和 C:\Qt\Tools\mingw13xx_64
for /d %%T in ("!QT_ROOT_DIR!\..\..\Tools\mingw*_64") do set "MINGW_BIN=%%T\bin"
if not exist "!MINGW_BIN!\mingw32-make.exe" (
    where mingw32-make >nul 2>&1 && set "MINGW_BIN=" || (
        echo [错误] 未找到 mingw32-make, 请随 Qt 安装 MinGW 工具链
        pause & exit /b 1
    )
)
if defined MINGW_BIN echo [OK] GCC:  !MINGW_BIN!
set "PATH=!QT_ROOT_DIR!\bin;!MINGW_BIN!;%PATH%"

REM ---------- 3. CMake 配置 + 编译 ----------
set "PRESET=default"
if "%BUILD_TYPE%"=="Release" set "PRESET=release"

echo [..] CMake 配置 (%BUILD_TYPE%) ...
cmake --preset %PRESET% || (echo [失败] CMake 配置出错 & pause & exit /b 1)

echo [..] 编译中, 使用 %NUMBER_OF_PROCESSORS% 线程 ...
cmake --build build/%PRESET% -j %NUMBER_OF_PROCESSORS% || (echo [失败] 编译出错 & pause & exit /b 1)

echo.
echo ============================================================
echo  编译成功: build\%PRESET%\bin\LWVision.exe
echo ============================================================
pause
