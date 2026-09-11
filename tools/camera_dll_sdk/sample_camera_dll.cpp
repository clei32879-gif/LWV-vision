/** @file sample_camera_dll.cpp — 示范: 厂商相机 SDK 封装成 LW Vision 外接相机 DLL
 *
 *  本示例不依赖任何厂商 SDK, 用内置生成图案模拟相机 (方便验证整条加载链路).
 *  替换 grab 实现里的生成逻辑为厂商 SDK 抓帧即可接入真实相机.
 *
 *  编译 (MSVC 或 MinGW):
 *    cl /LD sample_camera_dll.cpp /Fe:SimCam.dll
 *    g++ -shared -o SimCam.dll sample_camera_dll.cpp -DLWCAM_EXPORTS
 *  部署: SimCam.dll → LWVision/cameras/
 */
#define LWCAM_EXPORTS
#include "lwcam_sdk.h"
#include <cstring>

struct SimHandle {
    int width = 640, height = 480;
    unsigned char* buf = nullptr;
    int seq = 0;
};

static const int kChannels = 3;

static void ensureBuf(SimHandle* h) {
    if (!h->buf) h->buf = new unsigned char[h->width * h->height * kChannels];
}

extern "C" {

LWCAM_API int LWCam_GetInfo(LWCamDriverInfo* info) {
    if (!info) return -1;
    strcpy_s(info->name, "SimCam");
    strcpy_s(info->vendor, "LWVision 示例");
    return 0;
}

LWCAM_API int LWCam_Enumerate(LWCamDeviceInfo* list, int max) {
    if (!list || max < 1) return 0;
    strcpy_s(list[0].id, "SIM-0");
    strcpy_s(list[0].model, "模拟相机(SimCam)");
    return 1;
}

LWCAM_API void* LWCam_Open(const char* cameraId) {
    auto* h = new SimHandle();
    ensureBuf(h);
    // 生成可辨识图案: 灰度渐变
    for (int y = 0; y < h->height; ++y)
        for (int x = 0; x < h->width; ++x) {
            int v = (x * 255 / h->width + y) / 2;
            v = v * 3 / 4;
            unsigned char* px = h->buf + (y * h->width + x) * kChannels;
            px[0] = px[1] = px[2] = (unsigned char)v;
        }
    return h;
}

LWCAM_API void LWCam_Close(void* handle) {
    auto* h = (SimHandle*)handle;
    if (!h) return;
    delete[] h->buf;
    delete h;
}

LWCAM_API int LWCam_Grab(void* handle, unsigned char** buf, int* width, int* height,
                         int* channels, int timeoutMs) {
    auto* h = (SimHandle*)handle;
    if (!h) return -1;
    ensureBuf(h);
    // 移动条纹: 每帧平移, 证明画面是活的
    ++h->seq;
    for (int y = 0; y < h->height; y += 8) {
        int off = (h->seq * 4) % 16;
        for (int x = 0; x < h->width; ++x) {
            unsigned char* px = h->buf + (y * h->width + x) * kChannels;
            px[0] = px[1] = px[2] = (unsigned char)(((x + off) & 16) ? 200 : 60);
        }
    }
    *buf = h->buf;
    *width = h->width; *height = h->height; *channels = kChannels;
    (void)timeoutMs;
    return 0;
}

LWCAM_API int LWCam_SetParam(void* handle, const char* key, double value) {
    auto* h = (SimHandle*)handle;
    if (!h) return -1;
    if (!strcmp(key, "width"))  { h->width = (int)value;  ensureBuf(h); return 0; }
    if (!strcmp(key, "height")) { h->height = (int)value; ensureBuf(h); return 0; }
    return 0;   // 曝光/增益模拟相机不模拟
}

LWCAM_API double LWCam_GetParam(void* handle, const char* key) {
    auto* h = (SimHandle*)handle;
    if (!h) return -1;
    if (!strcmp(key, "width")) return h->width;
    if (!strcmp(key, "height")) return h->height;
    return -1;
}

} // extern "C"

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
