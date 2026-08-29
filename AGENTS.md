# AGENTS.md — AI 协作开发须知

> 给 AI 助手（以及新加入的人类开发者）的项目上下文。改动代码前先读完本文件。

## 项目速览

- **产品**：LW Vision 立维视觉 — 通用工业视觉检测平台（对标 CKVisionBuilder / OpenIVS / DLCV.AI）
- **技术栈**：C++17 + Qt 6（MinGW）+ OpenCV 5.0（项目内 MinGW 编译版）+ CMake，仅 Windows
- **代码命名空间**：`VisionInspector`（历史原因，勿改）；可执行文件/产品名：`LWVision`

## 必读文档

| 文档 | 内容 |
|------|------|
| `docs/完美化路线图.md` | 当前所处阶段与优先级，**改代码前先看这个** |
| `Windows编译指南.md` | 唯一权威编译文档（一键：build.bat） |
| `docs/竞品调研-AI视觉软件-2026-08.md` | 军哥MIP / DLCV.AI / 广州视觉芯 |
| `docs/OpenIVS专项评估.md` | 参考架构（references/OpenIVS 有其源码） |
| `docs/ckvision_manual.txt` | CKVisionBuilder 工具清单参考 |

## 硬性规则

1. **编译验证**：任何改动后必须 `build.bat release`（或 `cmake --build build/release -j`）通过才算完成；GUI 改动需本机运行确认
2. **不引入新依赖**：新增第三方库前先确认必要性并更新编译指南；优先用已有依赖（Qt/OpenCV/nlohmann/json）
3. **中文 UI**：界面文案一律中文；源码文件保持 UTF-8
4. **小步提交**：git 提交信息用中文、写清动机；不要在提交里混入无关改动
5. **路径可移植**：CMake/代码中禁止出现盘符绝对路径（D:/、C:/Users/... 等），一律相对 `${CMAKE_SOURCE_DIR}` 或环境变量
6. **参考代码的版权**：`references/` 下是外部仓库（Apache-2.0 等），借鉴其设计可以；移植代码片段必须保留原版权声明并在文件头注明来源
7. **测试资产**：检测类改动要用标准图片跑通验证；图片库放 `testdata/`（不入库大文件，入库说明文档）

## 目录结构

```
src/core   项目/插件/用户/配置/全局变量管理
src/engine 工具接口(ITool)、流程引擎(FlowEngine)、判定与数据模型   ← 架构改造主战场
src/hal    相机/PLC/伺服/IO 抽象接口与度申实现
src/ui     主界面、流程编辑器、DIY界面编辑器、图像控件
src/app    main 与 MainWindow
plugins/   40+ 工具插件（当前静态链接进主程序）
docs/      需求、路线图、调研、手册
testdata/  （待建）标准测试图片库
```
