/**
 * @file ExtCameraDLL.h — 外接相机 DLL 统一约定 (阶段6: 像创科那样"添加对应相机 dll")
 *
 * 约定: 任何厂商相机 SDK 封装成一个导出以下 C 接口的 DLL (C ABI, 与编译器无关),
 * 放到主程序 cameras/ 目录, 软件启动时自动扫描加载. 一个 DLL = 一个相机系列.
 *
 * 必须导出 (extern "C" __declspec(dllexport)):
 *   int  LWCam_GetInfo        (ExtCamInfo* info)                 — 返回驱动信息
 *   int  LWCam_Enumerate      (ExtCamInfo* list, int max)        — 枚举设备, 返回数量
 *   void* LWCam_Open          (const char* cameraId)             — 打开设备, 返回句柄(NULL=失败)
 *   void  LWCam_Close         (void* handle)                     — 关闭
 *   int   LWCam_Grab          (void* handle, unsigned char** buf,
 *                              int* width, int* height, int* channels,
 *                              int timeoutMs)                    — 抓一帧(BGR/灰度), 返回0=成功
 *   int   LWCam_SetParam      (void* handle, const char* key,
 *                              double value)                     — 曝光/增益/宽/高等
 *   double LWCam_GetParam     (void* handle, const char* key)
 *
 * 加载失败/字段缺失的 DLL 会被跳过并记录日志 (不影响主程序).
 */
#pragma once
#include <QString>
#include <QList>
#include <QFunctionPointer>

namespace VisionInspector {

struct ExtCamInfo {
    QString name;          // 驱动名 (如 "BaslerPylonC")
    QString vendor;        // 厂商
    QString dllPath;       // DLL 绝对路径
    int maxCameras = 0;    // 枚举到的设备数 (加载时探测)
};

/** 扫描目录下所有 dll, 尝试按约定加载; 返回成功加载的驱动名列表 */
QStringList scanExtCameraDLLs(const QString& dir);

/** 取已加载驱动的信息 */
QList<ExtCamInfo> loadedExtCameras();

/** 按驱动名取枚举设备列表 ("name:index" 形式的 cameraId) */
struct ExtCamDevice { QString driver; QString cameraId; QString model; };
QList<ExtCamDevice> enumerateExtCameras();

/** 打开外接相机 (成功返回非空句柄, 失败返回空并填 err) */
void* extCameraOpen(const QString& driver, const QString& cameraId, QString* err = nullptr);
void  extCameraClose(const QString& driver, void* handle);
int   extCameraGrab(const QString& driver, void* handle,
                    unsigned char** buf, int* w, int* h, int* ch, int timeoutMs);
bool  extCameraSetParam(const QString& driver, void* handle, const QString& key, double v);

} // namespace VisionInspector
