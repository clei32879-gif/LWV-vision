/** @file HIKCam_dll.cpp — 海康 MVS 相机封装为 LW Vision 外接相机 DLL
 *
 *  动态加载 MvCameraControl.dll (海康 MVS 客户端安装后自带, 无需链接导入库).
 *  放置: 编译出的 HIKCam.dll → LWVision/cameras/, MvCameraControl.dll 须在 PATH 或同目录.
 *
 *  注意: MV_CC_DEVICE_INFO 结构按 MVS 4.x/5.x 头文件手工复刻 (x64):
 *    INFO_LIST { unsigned nDeviceNum; void* pDeviceInfo[256]; }
 *    DEVICE_INFO { unsigned short maj, min; union{USB/GeoV/GLIGE...3层嵌套共 1024B}; char userDef[256]; ... }
 *  为稳健起见, 枚举解析只读 stdDeviceInfo 头部的 chUserDefinedName / chModelName 区.
 */
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <string>
#include "lwcam_sdk.h"
#include "lwcam_types.h"

// ---- MVS 核心类型 (按 MvCameraControl.h x64 布局复刻) ----
#pragma pack(push, 8)
struct MV_CC_DEVICE_INFO {
    unsigned short nMajorVer;
    unsigned short nMinorVer;
    unsigned char  reserved[1024];     // union SpecialInfo 区域 (GBK 名称在其中偏移~20)
    char chUserDefinedName[256];
    char chModelName[64];
    char chSerialNumber[64];
    // 其余字段省略 — 分配时给足空间
    unsigned char tail[512];
};
struct MV_CC_DEVICE_INFO_LIST {
    unsigned int nDeviceNum;
    MV_CC_DEVICE_INFO* pDeviceInfo[256];
};
#pragma pack(pop)

// ---- 动态解析的 MVS 函数指针 ----
static HMODULE g_mvs = nullptr;
static bool g_mvsTried = false;

typedef int (*FnEnumDev)(unsigned int devType, void* list);
typedef int (*FnCreateHandle)(void** handle, void* devInfo);
typedef int (*FnDestroyHandle)(void* handle);
typedef int (*FnOpenDev)(void* handle);
typedef int (*FnCloseDev)(void* handle);
typedef int (*FnStartGrab)(void* handle);
typedef int (*FnStopGrab)(void* handle);
typedef int (*FnGetImageBuf)(void* handle, void* frameInfo, int timeoutMs);
typedef int (*FnFreeBuf)(void* handle, void* frameInfo);
typedef int (*FnSetFloat)(void* handle, const char* key, float v);
typedef int (*FnSetEnum)(void* handle, const char* key, unsigned int v);
typedef int (*FnSetInt)(void* handle, const char* key, long long v);

static FnEnumDev     pEnum     = nullptr;
static FnCreateHandle pCreate  = nullptr;
static FnDestroyHandle pDestroy= nullptr;
static FnOpenDev     pOpen     = nullptr;
static FnCloseDev    pClose    = nullptr;
static FnStartGrab   pStart    = nullptr;
static FnStopGrab    pStop     = nullptr;
static FnGetImageBuf pGetImg   = nullptr;
static FnFreeBuf     pFree     = nullptr;
static FnSetFloat    pSetF     = nullptr;
static FnSetEnum     pSetE     = nullptr;

static bool ensureMVS() {
    if (g_mvsTried) return g_mvs != nullptr;
    g_mvsTried = true;
    g_mvs = LoadLibraryA("MvCameraControl.dll");
    if (!g_mvs) return false;
    pEnum   = (FnEnumDev)GetProcAddress(g_mvs, "MV_CC_EnumDevices");
    pCreate = (FnCreateHandle)GetProcAddress(g_mvs, "MV_CC_CreateHandle");
    pDestroy= (FnDestroyHandle)GetProcAddress(g_mvs, "MV_CC_DestroyHandle");
    pOpen   = (FnOpenDev)GetProcAddress(g_mvs, "MV_CC_OpenDevice");
    pClose  = (FnCloseDev)GetProcAddress(g_mvs, "MV_CC_CloseDevice");
    pStart  = (FnStartGrab)GetProcAddress(g_mvs, "MV_CC_StartGrabbing");
    pStop   = (FnStopGrab)GetProcAddress(g_mvs, "MV_CC_StopGrabbing");
    pGetImg = (FnGetImageBuf)GetProcAddress(g_mvs, "MV_CC_GetImageBuffer");
    pFree   = (FnFreeBuf)GetProcAddress(g_mvs, "MV_CC_FreeImageBuffer");
    pSetF   = (FnSetFloat)GetProcAddress(g_mvs, "MV_CC_SetFloatValue");
    pSetE   = (FnSetEnum)GetProcAddress(g_mvs, "MV_CC_SetEnumValue");
    return pEnum && pCreate && pOpen;
}

// MV_CC_GetImageBuffer 的 frame 结构 (MV_FRAME_OUT): 头部= {pBuf, nFrameLen, 帧 info{w,h,enType,...}}
// C++ 端只读头 3 指针/整数: pBuf(8) nFrameLen(8... 实际 unsigned int + pad) — 按 x64 布局取前 24B 解析
struct MV_FRAME_OUT_HEAD {
    unsigned char* pBuf;
    unsigned int nFrameLen;
    unsigned int _pad;
    unsigned int nWidth;
    unsigned int nHeight;
    unsigned int enPixelType;
    unsigned int _pad2;
};

struct HIKHandle {
    void* cam;
    unsigned char* lastBuf = nullptr;
    unsigned char* rgb = nullptr;
    int w = 0, h = 0, ch = 1;
    void* frame = nullptr;   // MV_FRAME_OUT
};

extern "C" {

__declspec(dllexport)
int LWCam_GetInfo(LWCamDriverInfo* info) {
    if (!info) return -1;
    strcpy_s(info->name, "HIKMVS");
    strcpy_s(info->vendor, "海康机器人");
    return 0;
}

__declspec(dllexport)
int LWCam_Enumerate(LWCamDeviceInfo* list, int max) {
    if (!ensureMVS() || !pEnum) return 0;
    // GigE(1) + USB(4) 两次枚举合并
    MvDeviceInfoList gigList{}; MvDeviceInfoList usbList{};
    int total = 0;
    if (pEnum(1, &gigList) == 0) {
        for (unsigned i = 0; i < gigList.nDeviceNum && total < max; ++i) {
            strcpy_s(list[total].id, ("HIK-GIGE-" + std::to_string(i)).c_str());
            strcpy_s(list[total].model, "海康GigE相机");
            ++total;
        }
    }
    if (pEnum(4, &usbList) == 0) {
        for (unsigned i = 0; i < usbList.nDeviceNum && total < max; ++i) {
            strcpy_s(list[total].id, ("HIK-USB-" + std::to_string(i)).c_str());
            strcpy_s(list[total].model, "海康USB相机");
            ++total;
        }
    }
    return total;
}

__declspec(dllexport)
void* LWCam_Open(const char* cameraId) {
    if (!ensureMVS() || !pEnum || !pCreate) return nullptr;
    MvDeviceInfoList list{};
    const bool usb = strncmp(cameraId, "HIK-USB", 7) == 0;
    if (pEnum(usb ? 4 : 1, &list) != 0) return nullptr;
    const char* dash = strrchr(cameraId, '-');
    int idx = dash ? atoi(dash + 1) : 0;
    if (idx >= (int)list.nDeviceNum) return nullptr;
    void* di = list.pDeviceInfo[idx];
    if (!di) return nullptr;
    void* handle = nullptr;
    if (pCreate(&handle, di) != 0 || !handle) return nullptr;
    if (pOpen(handle) != 0) { pDestroy(handle); return nullptr; }
    if (pStart) pStart(handle);
    auto* h = new HIKHandle();
    h->cam = handle;
    return h;
}

__declspec(dllexport)
void LWCam_Close(void* handle) {
    auto* h = (HIKHandle*)handle;
    if (!h) return;
    if (h->cam) {
        if (pStop) pStop(h->cam);
        if (pClose) pClose(h->cam);
        if (pDestroy) pDestroy(h->cam);
    }
    delete[] h->lastBuf;
    delete[] h->rgb;
    delete h;
}

__declspec(dllexport)
int LWCam_Grab(void* handle, unsigned char** buf, int* width, int* height,
               int* channels, int timeoutMs) {
    auto* h = (HIKHandle*)handle;
    if (!h || !h->cam || !pGetImg || !pFree) return -1;
    MvFrameOut frame{};   // MV_FRAME_OUT 复刻结构
    if (pGetImg(h->cam, &frame, timeoutMs) != 0) return -2;
    unsigned char* pbuf = frame.pBuf;
    int w = (int)frame.nWidth;
    int hh = (int)frame.nHeight;
    if (!pbuf || w <= 0 || hh <= 0) return -3;
    // Mono8 直出; 其他格式由调用方按需处理 (常见产线配置 Mono8)
    if (!h->lastBuf || h->w != w || h->h != hh) {
        delete[] h->lastBuf;
        h->lastBuf = new unsigned char[w * hh];
        h->w = w; h->h = hh;
    }
    memcpy(h->lastBuf, pbuf, (size_t)w * hh);
    pFree(h->cam, &frame);
    // 单通道灰度输出
    if (!h->rgb) h->rgb = new unsigned char[(size_t)w * hh * 3];
    // 保持灰度→3通道, 便于下游统一处理
    for (int i = 0; i < w * hh; ++i) {
        h->rgb[i*3] = h->rgb[i*3+1] = h->rgb[i*3+2] = h->lastBuf[i];
    }
    h->ch = 3;
    *buf = h->rgb;
    *width = w; *height = hh; *channels = 3;
    return 0;
}

__declspec(dllexport)
int LWCam_SetParam(void* handle, const char* key, double value) {
    auto* h = (HIKHandle*)handle;
    if (!h || !h->cam) return -1;
    if (!strcmp(key, "exposure")) {
        if (!pSetF) return -1;
        return pSetF(h->cam, "ExposureTime", (float)value);
    }
    if (!strcmp(key, "gain")) {
        if (!pSetF) return -1;
        return pSetF(h->cam, "Gain", (float)value);
    }
    if (!strcmp(key, "width")) {
        if (!pSetE) return -1;
        return 0; // 宽高建议用 MV_CC_SetIntValue("Width") — 预留
    }
    if (!strcmp(key, "triggerMode")) {
        if (!pSetE) return -1;
        return pSetE(h->cam, "TriggerMode", value > 0 ? 1 : 0);
    }
    return 0;
}

__declspec(dllexport)
double LWCam_GetParam(void* handle, const char* key) {
    (void)handle; (void)key;
    return -1;
}

} // extern "C"

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
