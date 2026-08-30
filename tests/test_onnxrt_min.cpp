/**
 * @file test_onnxrt_min.cpp
 * @brief ONNX Runtime 最小链接验证: 创建环境 + 加载模型 + 读取输入形状
 * (无模型时仅验证环境创建, 退出0)
 */

#include <onnxruntime_cxx_api.h>
#include <cstdio>
#include <string>

static std::wstring toWide(const char* s) {
    std::wstring w;
    while (*s) w += (wchar_t)(unsigned char)*s++;
    return w;
}

int main(int argc, char* argv[]) {
    try {
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "LWVision");
        std::printf("[OK] ORT环境创建成功\n");
        if (argc > 1) {
            Ort::SessionOptions opts;
            opts.SetIntraOpNumThreads(4);
            const std::wstring wpath = toWide(argv[1]);
            Ort::Session session(env, wpath.c_str(), opts);
            Ort::AllocatorWithDefaultOptions alloc;
            const size_t nIn = session.GetInputCount();
            std::printf("[OK] 模型加载成功, 输入数=%zu\n", nIn);
            for (size_t i = 0; i < nIn; ++i) {
                auto name = session.GetInputNameAllocated(i, alloc);
                auto shape = session.GetInputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
                std::printf("  输入[%zu] %s 形状=[", i, name.get());
                for (auto d : shape) std::printf("%lld ", (long long)d);
                std::printf("]\n");
            }
            const size_t nOut = session.GetOutputCount();
            for (size_t i = 0; i < nOut; ++i) {
                auto name = session.GetOutputNameAllocated(i, alloc);
                auto shape = session.GetOutputTypeInfo(i).GetTensorTypeAndShapeInfo().GetShape();
                std::printf("  输出[%zu] %s 形状=[", i, name.get());
                for (auto d : shape) std::printf("%lld ", (long long)d);
                std::printf("]\n");
            }
        }
        return 0;
    } catch (const Ort::Exception& e) {
        std::printf("[FAIL] ORT异常: %s\n", e.what());
        return 1;
    }
}
