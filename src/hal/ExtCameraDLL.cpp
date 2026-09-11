/** @file ExtCameraDLL.cpp — 外接相机 DLL 动态加载器 (QLibrary, 约定见头文件) */
#include "ExtCameraDLL.h"
#include "../utils/Logger.h"
#include <QDir>
#include <QLibrary>
#include <QCoreApplication>

namespace VisionInspector {

// ---- C 接口签名 (与 DLL 约定一致, 纯POD) ----
struct ExtCamCInfo { char name[64]; char vendor[64]; };

using FnEnumerate = int (*)(ExtCamCInfo*, int);
using FnOpen      = void* (*)(const char*);
using FnClose     = void (*)(void*);
using FnGrab      = int (*)(void*, unsigned char**, int*, int*, int*, int);
using FnSetParam  = int (*)(void*, const char*, double);
using FnGetParam  = double (*)(void*, const char*);

struct LoadedDriver {
    QString name, vendor, path;
    FnEnumerate enumerate = nullptr;
    FnOpen open = nullptr;
    FnClose close = nullptr;
    FnGrab grab = nullptr;
    FnSetParam setParam = nullptr;
    FnGetParam getParam = nullptr;
};
static QList<LoadedDriver> g_drivers;

static bool loadOne(const QString& path) {
    QLibrary lib(path);
    if (!lib.load()) return false;
    auto get = [&](const char* fn) { return reinterpret_cast<QFunctionPointer>(lib.resolve(fn)); };
    auto enumerate = reinterpret_cast<FnEnumerate>(get("LWCam_Enumerate"));
    auto open      = reinterpret_cast<FnOpen>(get("LWCam_Open"));
    auto grab      = reinterpret_cast<FnGrab>(get("LWCam_Grab"));
    auto close     = reinterpret_cast<FnClose>(get("LWCam_Close"));
    if (!enumerate || !open || !grab || !close) return false;   // 关键导出缺失 → 跳过

    LoadedDriver d;
    d.path = path;
    d.enumerate = enumerate; d.open = open; d.close = close; d.grab = grab;
    d.setParam = reinterpret_cast<FnSetParam>(get("LWCam_SetParam"));
    d.getParam = reinterpret_cast<FnGetParam>(get("LWCam_GetParam"));
    ExtCamCInfo info{};
    using FnGetInfoC = int (*)(ExtCamCInfo*);
    auto getInfo = reinterpret_cast<FnGetInfoC>(get("LWCam_GetInfo"));
    if (getInfo) { ExtCamCInfo ci{}; if (getInfo(&ci) == 0) { d.name = ci.name; d.vendor = ci.vendor; } }
    if (d.name.isEmpty()) d.name = QFileInfo(path).baseName();

    for (const auto& exist : g_drivers)
        if (exist.name == d.name) return false;   // 重名跳过
    g_drivers.append(d);
    VI_LOG_INFO(QString("外接相机DLL已加载: %1 (%2)").arg(d.name, path));
    return true;
}

QStringList scanExtCameraDLLs(const QString& dir) {
    QStringList loaded;
    QDir d(dir);
    if (!d.exists()) return loaded;
    for (const QFileInfo& fi : d.entryInfoList({"*.dll"}, QDir::Files)) {
        if (loadOne(fi.absoluteFilePath())) loaded << fi.baseName();
    }
    if (!loaded.isEmpty())
        VI_LOG_INFO(QString("外接相机驱动: %1 个 (%2)").arg(loaded.size()).arg(loaded.join(", ")));
    return loaded;
}

QList<ExtCamInfo> loadedExtCameras() {
    QList<ExtCamInfo> out;
    for (const auto& d : g_drivers) {
        ExtCamInfo i; i.name = d.name; i.vendor = d.vendor; i.dllPath = d.path;
        out.append(i);
    }
    return out;
}

static LoadedDriver* findDriver(const QString& name) {
    for (auto& d : g_drivers) if (d.name == name) return &d;
    return nullptr;
}

QList<ExtCamDevice> enumerateExtCameras() {
    QList<ExtCamDevice> out;
    for (const auto& d : g_drivers) {
        ExtCamCInfo infos[16];
        int n = d.enumerate(infos, 16);
        for (int i = 0; i < n; ++i)
            out.append({ d.name, QString("%1:%2").arg(d.name).arg(i),
                         QString::fromLocal8Bit(infos[i].name) });
    }
    return out;
}

void* extCameraOpen(const QString& driver, const QString& cameraId, QString* err) {
    auto* d = findDriver(driver);
    if (!d) { if (err) *err = QStringLiteral("驱动未加载: %1").arg(driver); return nullptr; }
    const QString id = cameraId.mid(cameraId.indexOf(':') + 1);   // 去 "driver:" 前缀
    void* h = d->open(id.toLocal8Bit().constData());
    if (!h && err) *err = QStringLiteral("打开失败");
    return h;
}

void extCameraClose(const QString& driver, void* handle) {
    if (auto* d = findDriver(driver)) d->close(handle);
}

int extCameraGrab(const QString& driver, void* handle,
                  unsigned char** buf, int* w, int* h, int* ch, int timeoutMs) {
    auto* d = findDriver(driver);
    if (!d) return -1;
    return d->grab(handle, buf, w, h, ch, timeoutMs);
}

bool extCameraSetParam(const QString& driver, void* handle, const QString& key, double v) {
    auto* d = findDriver(driver);
    if (!d || !d->setParam) return false;
    return d->setParam(handle, key.toUtf8().constData(), v) == 0;
}

} // namespace VisionInspector
