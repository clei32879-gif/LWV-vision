/** @file DSCam_dll.cpp — 度申 DVP2 相机封装为 LW Vision 外接相机 DLL
 *
 *  动态加载 DVPCamera64.dll (度申 DVP2 SDK 运行库, 装机后自带).
 *  API 按 DVP2 头文件 (dvp.h) 的 C 接口: dvpInit/dvpUnInit, dvpEnumDevices,
 *  dvpOpenDeviceById, dvpCloseDevice, dvpCapture, dvpSet... 
 */
#include "lwcam_sdk.h"
#include <windows.h>
#include <cstring>
#include <cstdio>

// ---- DVP2 C 类型 (按 dvp.h x64 复刻) ----
typedef void* dvpHandle;
struct dvpDeviceId {
    char GUID[64];
    char FriendlyName[256];
};
struct dvpDeviceList {
    unsigned int nCount;
    dvpDeviceId devices[16];
};
struct dvpFrame {
    unsigned char* pImageData;
    unsigned int uBytes;
    unsigned int iWidth;
    unsigned int iHeight;
    unsigned int bFormat;
    double fExposure;
    unsigned long long timestamp;
    unsigned char _tail[128];
};

// ---- 动态解析 ----
static HMODULE g_dvp = nullptr;
static bool g_tried = false;

typedef int (*FnInit)();
typedef int (*FnUnInit)();
typedef int (*FnEnum)(dvpDeviceList*);
typedef int (*FnOpenById)(dvpDeviceId*, dvpHandle*);
typedef int (*FnClose)(dvpHandle);
typedef int (*FnCapture)(dvpHandle, dvpFrame*);
typedef int (*FnSetFloatDvp)(dvpHandle, const char*, double);
typedef int (*FnSetBoolDvp)(dvpHandle, const char*, bool);

static FnInit pInit = nullptr;
static FnUnInit pUnInit = nullptr;
static FnEnum pEnum = nullptr;
static FnOpenById pOpen = nullptr;
static FnClose pClose = nullptr;
static FnCapture pCap = nullptr;
static FnSetFloatDvp pSetF = nullptr;
static FnSetBoolDvp pSetB = nullptr;

static bool ensureDVP() {
    if (g_tried) return g_dvp != nullptr;
    g_tried = true;
    g_dvp = LoadLibraryA("DVPCamera64.dll");
    if (!g_dvp) return false;
    pInit   = (FnInit)GetProcAddress(g_dvp, "dvpInit");
    pUnInit = (FnUnInit)GetProcAddress(g_dvp, "dvpUnInit");
    pEnum   = (FnEnum)GetProcAddress(g_dvp, "dvpEnumDevices");
    pOpen   = (FnOpenById)GetProcAddress(g_dvp, "dvpOpenDeviceById");
    pClose  = (FnClose)GetProcAddress(g_dvp, "dvpCloseDevice");
    pCap    = (FnCapture)GetProcAddress(g_dvp, "dvpCapture");
    pSetF   = (FnSetFloatDvp)GetProcAddress(g_dvp, "dvpSetFloatValue");
    pSetB   = (FnSetBoolDvp)GetProcAddress(g_dvp, "dvpSetBoolValue");
    return pInit && pEnum && pOpen && pCap;
}

struct DSHandle {
    dvpHandle dev = nullptr;
    unsigned char* lastBuf = nullptr;
    unsigned char* rgb = nullptr;
    int w = 0, h = 0;
};

extern "C" {

__declspec(dllexport)
int LWCam_GetInfo(LWCamDriverInfo* info) {
    if (!info) return -1;
    strcpy_s(info->name, "DSCam");
    strcpy_s(info->vendor, "度申科技");
    return 0;
}

__declspec(dllexport)
int LWCam_Enumerate(LWCamDeviceInfo* list, int max) {
    if (!ensureDVP() || !pEnum) return 0;
    dvpDeviceList dl{};
    if (pEnum(&dl) != 0) return 0;
    int n = (int)dl.nCount;
    if (n > max) n = max;
    for (int i = 0; i < n; ++i) {
        strcpy_s(list[i].id, dl.devices[i].GUID);
        strcpy_s(list[i].model, dl.devices[i].FriendlyName);
    }
    return n;
}

__declspec(dllexport)
void* LWCam_Open(const char* cameraId) {
    if (!ensureDVP()) return nullptr;
    static bool inited = false;
    if (!inited && pInit) { pInit(); inited = true; }
    dvpDeviceId id{};
    strncpy_s(id.GUID, cameraId, sizeof(id.GUID) - 1);
    auto* h = new DSHandle();
    if (pOpen(&id, &h->dev) != 0) { delete h; return nullptr; }
    // 连续采集模式
    if (pSetB) pSetB(h->dev, "trigger switch", false);
    return h;
}

__declspec(dllexport)
void LWCam_Close(void* handle) {
    auto* h = (DSHandle*)handle;
    if (!h) return;
    if (h->dev && pClose) pClose(h->dev);
    delete[] h->lastBuf;
    delete[] h->rgb;
    delete h;
}

__declspec(dllexport)
int LWCam_Grab(void* handle, unsigned char** buf, int* width, int* height,
               int* channels, int timeoutMs) {
    auto* h = (DSHandle*)handle;
    if (!h || !h->dev || !pCap) return -1;
    dvpFrame frame{};
    // dvpCapture 阻塞取帧 (超时由库内部管理)
    if (pCap(h->dev, &frame) != 0 || !frame.pImageData) return -2;
    const int w = (int)frame.iWidth, hh = (int)frame.iHeight;
    const bool mono = (frame.bFormat == 0);   // MONO 格式约定
    if (!h->rgb || h->w != w || h->h != hh) {
        delete[] h->rgb;
        h->rgb = new unsigned char[(size_t)w * hh * 3];
        h->w = w; h->h = hh;
    }
    if (mono) {
        for (int i = 0; i < w * hh; ++i)
            h->rgb[i*3] = h->rgb[i*3+1] = h->rgb[i*3+2] = frame.pImageData[i];
    } else {
        memcpy(h->rgb, frame.pImageData, (size_t)w * hh * 3);
    }
    *buf = h->rgb;
    *width = w; *height = hh; *channels = 3;
    (void)timeoutMs;
    return 0;
}

__declspec(dllexport)
int LWCam_SetParam(void* handle, const char* key, double value) {
    auto* h = (DSHandle*)handle;
    if (!h || !h->dev || !pSetF) return -1;
    if (!strcmp(key, "exposure")) return pSetF(h->dev, "exposure", value);
    if (!strcmp(key, "gain"))     return pSetF(h->dev, "gain", value);
    return 0;
}

__declspec(dllexport)
double LWCam_GetParam(void* handle, const char* key) {
    (void)handle; (void)key;
    return -1;
}

} // extern "C"

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
