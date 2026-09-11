/** @file test_bracket_correction.cpp
 *  @brief 括号式位置补正回归 (CKVision式): 位置补正→…→结束补正 区间内工具自动跟随, 免勾选
 */
#include "../../src/engine/FlowEngine.h"
#include "../../src/engine/ToolRegistry.h"
#include <QCoreApplication>
#include <opencv2/core.hpp>
#include <cstdio>

using namespace VisionInspector;
static int fails = 0;
#define CHECK(c, m) do { if(!(c)) { std::printf("  [FAIL] %s\n", m); ++fails; } } while(0)

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    auto& reg = ToolRegistry::instance();
    FlowEngine engine;

    // 流程: 位置补正(手动链接到 定位.matchX/Y=30,40) → 边缘检测(区间内) → 结束补正
    Flow* f = new Flow(&engine);
    f->setName("括号补正");
    ITool* pc = reg.createTool("PositionCorrection");
    CHECK(pc != nullptr, "位置补正注册");
    pc->setProperty("sourceMode", 1);            // 手动链接模式 (确定性)
    pc->setProperty("originXLink", "定位.matchX");
    pc->setProperty("originYLink", "定位.matchY");
    ITool* ed = reg.createTool("EdgeDetection");
    CHECK(ed != nullptr, "边缘检测注册");
    ed->setProperty("roiCenterX", 100.0);
    ed->setProperty("roiCenterY", 50.0);
    ed->setProperty("roiWidth", 60.0);
    ed->setProperty("roiHeight", 60.0);
    ITool* ec = reg.createTool("EndCorrection");
    CHECK(ec != nullptr, "结束补正注册");
    f->addTool(pc);
    f->addTool(ed);
    f->addTool(ec);
    engine.addFlow(f);

    // ---- 分步验证: 位置补正单工具括号置位 → 结束补正闭合 ----
    ToolContext ctxPC;
    ctxPC.setData("定位.matchX", 30.0);
    ctxPC.setData("定位.matchY", 40.0);
    CHECK(pc->execute(ctxPC), "位置补正单工具执行OK");
    CHECK(ctxPC.getBool("__auto_correction", false) == true, "括号张开(自动跟随置位)");
    CHECK(ctxPC.getDouble("coord_originX", -1) == 30.0, "补正原点X=30");
    CHECK(ctxPC.getDouble("coord_originY", -1) == 40.0, "补正原点Y=40");

    CHECK(ec->execute(ctxPC), "结束补正执行OK");
    CHECK(ctxPC.getBool("__auto_correction", true) == false, "括号闭合(自动跟随复位)");

    // ---- 整流程执行: 不崩溃, 区间内自动跟随标志生命周期正确 ----
    ToolContext ctx;
    ctx.setData("定位.matchX", 30.0);
    ctx.setData("定位.matchY", 40.0);
    ctx.setCurrentImage(std::make_shared<CvImage>(cv::Mat::zeros(480, 640, CV_8UC1)));
    engine.executeSubFlow(f, ctx);
    CHECK(ctx.getBool("__auto_correction", true) == false, "整流程后括号闭合一致");

    engine.shutdownAndWait(2000);
    std::printf("\n括号式补正: %s (失败%d)\n", fails ? "存在失败" : "全部通过", fails);
    return fails ? 1 : 0;
}
