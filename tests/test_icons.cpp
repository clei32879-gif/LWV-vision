/**
 * @file test_icons.cpp
 * @brief 图标加载回归测试: 遍历全部已注册工具与分类,
 *        验证素材图标(IconHelper)能加载非空图标, 缺失时回退代码绘制。
 *
 * 素材来源: 设计稿"图标素材"文件夹(白底JPG转透明PNG, 经 icons.qrc 编译进 VIUI)。
 * 任何一个工具/分类出现空图标, 说明素材映射或资源编译有问题, 测试失败。
 */

#include "../src/ui/IconHelper.h"
#include "../src/engine/ToolRegistry.h"
#include <QGuiApplication>
#include <QIcon>
#include <QStringList>
#include <cstdio>

using namespace VisionInspector;

static int g_fail = 0;

static void check(const char* what, const QIcon& icon, const QString& name) {
    if (icon.isNull()) {
        std::printf("[FAIL] %s 空图标: %s\n", what, name.toUtf8().constData());
        ++g_fail;
    } else {
        std::printf("[OK]   %s: %s\n", what, name.toUtf8().constData());
    }
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    // 1. 应用图标
    check("appIcon", IconHelper::appIcon(64), "应用图标");

    // 2. 全部工具图标 (先按注册清单, 再按类型名)
    const auto metas = ToolRegistry::instance().allMetaData();
    std::printf("已注册工具数: %lld\n", static_cast<long long>(metas.size()));
    for (const auto& meta : metas) {
        check("toolIcon", IconHelper::toolIcon(meta.typeName, meta.category, 16), meta.typeName + " / " + meta.displayName);
    }

    // 3. 全部分类图标 (素材或代码绘制兜底, 必须非空)
    const QStringList catNames = {
        "Camera", "ImageProcess", "Calibration", "Detection", "Geometry",
        "Communication", "Logic", "System", "ThreeD", "Special"
    };
    const ToolCategory cats[] = {
        ToolCategory::Camera, ToolCategory::ImageProcess, ToolCategory::Calibration,
        ToolCategory::Detection, ToolCategory::Geometry, ToolCategory::Communication,
        ToolCategory::Logic, ToolCategory::System, ToolCategory::ThreeD, ToolCategory::Special
    };
    for (int i = 0; i < catNames.size(); ++i) {
        check("categoryIcon", IconHelper::categoryIcon(cats[i], 18), "分类-" + catNames[i]);
    }

    if (g_fail == 0) {
        std::printf("\n图标测试全部通过 (失败: 0)\n");
        return 0;
    }
    std::printf("\n图标测试失败: %d 项\n", g_fail);
    return 1;
}
