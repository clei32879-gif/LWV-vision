# VisionInspector 开发环境搭建指南 (Windows)

> **目标**: 在Windows电脑上搭建开发环境，编译运行VisionInspector

---

## 第一步：安装开发工具 (约30分钟)

### 1.1 安装 Visual Studio 2022 (免费社区版)

1. 打开浏览器，访问: https://visualstudio.microsoft.com/zh-hans/vs/community/
2. 下载 **Community 2022** 版本
3. 运行安装程序，在"工作负载"中勾选:
   - ✅ **使用C++的桌面开发**
   - ✅ **MSVC v143 - VS 2022 C++ x64/x86生成工具**
   - ✅ **Windows 11 SDK** (或Windows 10 SDK)
4. 点击安装，等待完成（约15-20GB）

### 1.2 安装 Qt 6 (开源免费版)

1. 打开浏览器，访问: https://www.qt.io/download-open-source
2. 下载 **Qt Online Installer**
3. 运行安装程序，登录/注册Qt账号
4. 在组件选择界面，选择:
   - ✅ **Qt 6.8.0** (或最新版本)
   - 在Qt 6.8.0下勾选: **MSVC 2022 64-bit**
   - ✅ **Qt Creator** (IDE, 已包含)
   - ✅ **CMake** (Qt自带的CMake, 也可以单独装)
5. 安装路径建议: `C:\Qt` (不要有空格和中文)
6. 等待安装完成（约5-10GB）

### 1.3 安装 OpenCV

**方法A: 下载预编译包 (推荐, 最简单)**

1. 访问: https://github.com/opencv/opencv/releases
2. 找到 **4.13.0** 版本，下载 `opencv-4.13.0-windows.exe` (约185MB)
3. 运行它，解压到 `E:\opencv` (路径不要有中文和空格)
4. 最终结构:
   ```
   E:\opencv\
     └── build\
         ├── bin\          (DLL文件)
         ├── include\      (头文件)
         ├── x64\vc16\     (VS2022用的库文件)
         │   ├── bin\      (opencv_world4xxxd.dll等)
         │   └── lib\      (opencv_world4xxxd.lib等)
         └── x64\vc17\     (VS2022也可以用vc16)
   ```

### 1.4 安装 CMake (如果没有用Qt自带的)

1. 访问: https://cmake.org/download/
2. 下载 Windows x64 Installer
3. 安装时勾选 "Add CMake to system PATH"

---

## 第二步：获取项目代码

### 方法A: 从小盘直接打开 (推荐)

1. 把小盘移动硬盘插到Windows电脑上
2. 小盘盘符可能是 `E:\` 或 `F:\` 等
3. 项目路径: `小盘:\光选\`

### 方法B: 从GitHub克隆 (如果你创建了Git仓库)

```bash
git clone <你的仓库地址>
```

---

## 第三步：编译项目 (两种方式任选)

### 方式1: 用 Qt Creator 编译 (推荐, 最简单)

1. 打开 **Qt Creator**
2. 点击 **打开项目**
3. 选择 `小盘:\光选\CMakeLists.txt`
4. Qt Creator会自动检测Qt和CMake
5. 需要配置 OpenCV 路径:
   - 在左侧"项目"标签页中
   - 找到 **CMake** 配置
   - 添加变量: `OpenCV_DIR` = `E:/opencv/build`
6. 点击 **运行** 按钮 (绿色三角形)
7. 等待编译完成，软件会自动启动

### 方式2: 用命令行编译

```batch
# 打开 "x64 Native Tools Command Prompt for VS 2022"
# 然后执行:

cd /d 小盘:\光选

# 创建构建目录
mkdir build
cd build

# 配置项目 (注意替换Qt路径)
cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64" ^
    -DOpenCV_DIR="E:/opencv/build" ^
    ..

# 编译 (Release模式)
cmake --build . --config Release

# 运行
cd bin\Release
VisionInspector.exe
```

---

## 第四步：验证运行

如果一切正常，你会看到:

```
┌──────────────────────────────────────────────────────────────┐
│ 文件(F)  设置(S)  操作(O)  视图(V)  帮助(H)                    │
├──────────────────────────────────────────────────────────────┤
│ [新建] [打开] [保存] | [执行] [运行] [停止] | [切换用户]        │
├──────────┬──────────────────────────────────┬────────────────┤
│          │                                  │                │
│ 工具箱   │                                  │ 流程           │
│          │        VisionInspector           │                │
│ 相机工具 │         视觉筛选软件              │ (流程列表)     │
│ 图像处理 │                                  │                │
│ 检测识别 │      等待加载图像...              │                │
│ 几何测量 │                                  │                │
│ ...      │                                  │                │
│          │                                  │                │
├──────────┴──────────────────────────────────┴────────────────┤
│ 数据: 检测项目 | 上限 | 下限 | OK数 | NG数 | OK率 | NG率 |状态│
│      总数:0 | 良品:0 | 不良品:0 | 重测:0 | 良率:0%            │
├──────────────────────────────────────────────────────────────┤
│ 日志: [hh:mm:ss] VisionInspector 启动完成                     │
├──────────────────────────────────────────────────────────────┤
│ 就绪                                       用户:管理员  未连接│
└──────────────────────────────────────────────────────────────┘
```

---

## 常见问题

### Q: 找不到Qt模块?
**A:** 确认 `CMAKE_PREFIX_PATH` 指向正确的Qt安装路径，
如 `C:/Qt/6.8.0/msvc2022_64`

### Q: 找不到OpenCV?
**A:** 确认 `OpenCV_DIR` 指向 `E:/opencv/build`，
该目录下应该有 `OpenCVConfig.cmake` 文件

### Q: 编译时报错 "无法打开输入文件 opencv_world4xxxd.lib"?
**A:** 需要把OpenCV的DLL目录加入PATH:
   - 右键"此电脑" → 属性 → 高级系统设置 → 环境变量
   - 在Path中添加: `E:\opencv\build\x64\vc16\bin`
   - 重启电脑

### Q: 程序启动后闪退?
**A:** 检查是否缺少Qt的DLL:
   - 可以在Qt Creator中运行(它会自动配置PATH)
   - 或者手动运行 `C:\Qt\6.8.0\msvc2022_64\bin\windeployqt.exe VisionInspector.exe`

---

## 项目文件结构说明

```
光选\
├── CMakeLists.txt              ← 顶层构建文件
├── src\                        ← 源代码
│   ├── app\                    ← 主程序
│   │   ├── main.cpp            ← 程序入口
│   │   ├── MainWindow.h/cpp    ← 主窗口
│   │   └── AppSettings.h/cpp   ← 应用设置
│   ├── core\                   ← 核心框架
│   │   ├── ProjectManager      ← 项目管理
│   │   ├── UserManager         ← 用户权限
│   │   ├── GlobalVariables     ← 全局变量
│   │   └── ConfigManager       ← 配置管理
│   ├── engine\                 ← 工具引擎
│   │   ├── ITool.h             ← 工具基类接口(核心!)
│   │   ├── ToolContext         ← 工具执行上下文
│   │   ├── ToolRegistry        ← 工具注册表
│   │   ├── FlowEngine          ← 流程执行引擎
│   │   └── InspectionResult    ← 检测结果/统计
│   ├── hal\                    ← 硬件抽象层
│   │   ├── ICameraDriver       ← 相机接口
│   │   ├── IPLCDriver          ← PLC接口
│   │   ├── IServoDriver        ← 伺服接口
│   │   └── HardwareManager     ← 硬件管理器
│   ├── ui\                     ← 界面组件
│   │   ├── DisplayArea         ← 显示区域
│   │   ├── FlowEditor          ← 流程编辑器
│   │   ├── Toolbox             ← 工具箱
│   │   ├── DataPanel           ← 数据栏
│   │   └── LogPanel            ← 日志栏
│   └── utils\                  ← 工具类
│       ├── Common.h            ← 通用定义
│       ├── Logger              ← 日志系统
│       └── JsonHelper          ← JSON辅助
├── plugins\                    ← 插件目录
│   ├── cameras\                ← 相机插件
│   ├── tools\                  ← 检测工具插件
│   ├── logic\                  ← 逻辑工具插件
│   ├── comm\                   ← 通讯工具插件
│   └── plc\                    ← PLC驱动插件
├── docs\                       ← 文档
│   ├── 开发计划.md
│   ├── ckvision_manual.txt     ← CKVision手册参考
│   └── Windows开发环境搭建.md  ← 本文档
├── 完整需求与技术方案.md
└── 需求梳理与框架.md
```

---

## 下一步

环境搭好后，按 `F5` 或点击运行按钮，如果能看到主界面窗口，就说明环境搭建成功！

然后我们就可以开始往里面填充具体功能了:
1. 相机采集 (接度申相机)
2. BLOB分析 / 边缘检测等算法
3. PLC通讯 (Modbus对接信捷XD5)
4. 流程编辑器
5. ...
