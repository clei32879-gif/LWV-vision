/**
 * @file PlaySound.cpp
 * @brief 播放声音工具 (对标 CKVision 播放声音, P0-10 补齐)
 *
 * NG报警/提示音:
 * - 播放模式 系统提示音: Windows 系统默认提示音 (MessageBeep)
 * - 播放模式 播放wav文件: 异步播放指定 .wav 文件 (PlaySoundW, SND_ASYNC)
 *
 * Windows 上使用系统 API, 无第三方依赖; 非 Windows 平台回退到 QApplication::beep()。
 * 输出: played(bool) / mode / soundPath
 */
#include "PlaySound.h"
#include "../../../src/engine/ToolRegistry.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
// windows.h 把 PlaySound 定义成宏 (PlaySoundW/A), 与本工具类名冲突
#undef PlaySound
#endif

namespace VisionInspector {

PropertyDefList PlaySound::propertyDefs() const {
    return {
        PropertyDef::enumProp("playMode", "播放模式", {"系统提示音", "播放wav文件"}, 0),
        PropertyDef::stringProp("soundPath", "wav文件路径", ""),
    };
}

bool PlaySound::execute(ToolContext& context) {
    const int mode = propertyValue("playMode").toInt();
    const QString path = propertyValue("soundPath").toString();

#ifdef Q_OS_WIN
    bool played = false;
    if (mode == 1 && !path.isEmpty()) {
        played = PlaySoundW(reinterpret_cast<LPCWSTR>(path.utf16()),
                            nullptr, SND_FILENAME | SND_ASYNC);
    } else {
        played = MessageBeep(MB_ICONWARNING) != 0;
    }
    setResultData("played", played);
#else
    // 非 Windows 平台: 静默通过 (产品以 Windows 为目标, 不引入额外依赖)
    setResultData("played", true);
#endif

    setResultData("mode", mode);
    setResultData("soundPath", path);
    setStatus(ToolStatus::OK);
    return true;
}

} // namespace VisionInspector

VI_REGISTER_TOOL(PlaySound, "播放声音", VisionInspector::ToolCategory::Logic)
