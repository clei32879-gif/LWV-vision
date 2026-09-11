/** @file lwcam_sdk.h — LW Vision 外接相机 DLL 开发约定 (厂商/集成商按此实现即可被软件加载)
 *
 *  约定: 纯 C 接口 (extern "C"), 一个 DLL = 一个相机系列.
 *  将编译出的 DLL 放入主程序 cameras/ 目录, 启动时自动加载.
 *  必需导出: LWCam_Enumerate / LWCam_Open / LWCam_Grab / LWCam_Close
 *  可选导出: LWCam_GetInfo / LWCam_SetParam / LWCam_GetParam
 *  内部可用任意厂商 SDK (Basler PylonC / 海康 MVS / 度申 DVP2 / HIK / ...).
 *  编译建议: 任意编译器 (C ABI 与编译器无关), 静态链接厂商库避免运行库依赖.
 */
#pragma once
#include <windows.h>

#ifdef LWCAM_EXPORTS
#define LWCAM_API extern "C" __declspec(dllexport)
#else
#define LWCAM_API extern "C" __declspec(dllimport)
#endif

#pragma pack(push, 8)
struct LWCamDriverInfo { char name[64]; char vendor[64]; };
struct LWCamDeviceInfo { char id[128]; char model[128]; };
#pragma pack(pop)

extern "C" {

/** 驱动信息 (可选): name=驱动名(=dll名可省), vendor=厂商 */
LWCAM_API int LWCam_GetInfo(LWCamDriverInfo* info);

/** 枚举设备, 返回数量 (list 容量 max) */
LWCAM_API int LWCam_Enumerate(LWCamDeviceInfo* list, int max);

/** 打开设备 (cameraId = LWCam_Enumerate 里的 id), 返回句柄, NULL=失败 */
LWCAM_API void* LWCam_Open(const char* cameraId);

LWCAM_API void LWCam_Close(void* handle);

/** 抓一帧: 输出 BGR 或灰度; 返回 0=成功, 非0=失败/超时.
 *  buf 指向驱动内部缓冲 (下次抓取前有效), 调用方不释放. */
LWCAM_API int LWCam_Grab(void* handle, unsigned char** buf,
                         int* width, int* height, int* channels, int timeoutMs);

/** 设置参数: key 例 "exposure"(μs) / "gain"(dB) / "width" / "height" /
 *            "triggerMode"(0连续1触发) / "pixelFormat" / "lineRate"(线阵Hz) */
LWCAM_API int LWCam_SetParam(void* handle, const char* key, double value);

LWCAM_API double LWCam_GetParam(void* handle, const char* key);

} // extern "C"
