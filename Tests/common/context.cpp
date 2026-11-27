#include "./context.h"
#include <spdlog/spdlog.h>

using namespace lyra;

auto TestContext::get() -> TestContext&
{
    static TestContext instance;
    return instance;
}

bool TestContext::acquire(RHIBackend backend, RHIFlags flags, const Vector<GPUFeatureName>& features)
{
    // reuse active context if backend, flags, and features match
    if (rhi && current_backend == backend && current_flags == flags && current_features == features) {
        return true;
    }

    release();

    current_backend     = backend;
    current_flags       = flags;
    current_features    = features;
    feature_unsupported = false;
    missing_feature_name.clear();

    if (!loader) {
        std::filesystem::path repo_root;
        if (const char* env_root = std::getenv("LYRA_REPO_ROOT")) {
            repo_root = env_root;
        }
        if (repo_root.empty() || !std::filesystem::exists(repo_root / "Engine" / "Assets")) {
            repo_root = std::filesystem::current_path();
            while (!repo_root.empty() && !std::filesystem::exists(repo_root / "Engine" / "Assets")) {
                auto parent = repo_root.parent_path();
                if (parent == repo_root) break;
                repo_root = parent;
            }
        }
        String engine_assets_str = (repo_root / "Engine" / "Assets").lexically_normal().string();

        loader = make_own<FileLoader>(FSLoader::NATIVE);
        (void)loader->mount("sys://", engine_assets_str.c_str(), 0);
    }

    auto rhi_desc    = RHIDescriptor{};
    rhi_desc.backend = backend;
    rhi_desc.flags   = flags;
    rhi              = RHI::init(rhi_desc);
    if (!rhi) {
        release();
        return false;
    }

    auto adapter_desc = GPUAdapterDescriptor{};
    adapter           = rhi->request_adapter(adapter_desc);
    if (!adapter.valid()) {
        release();
        return false;
    }

    for (auto feature : features) {
        if (!adapter.has_feature(feature)) {
            feature_unsupported  = true;
            missing_feature_name = to_string(feature);
            release();
            return false;
        }
    }

    auto dev_desc     = GPUDeviceDescriptor{};
    dev_desc.label    = "test_device";
    dev_desc.features = features;
    device            = adapter.request_device(dev_desc);
    if (!device.valid()) {
        release();
        return false;
    }

    return true;
}

void TestContext::release()
{
    if (rhi) {
        if (device.valid()) {
            device.wait();
        }
        rhi.reset();
        device.reset();
        adapter.reset();
    }
    current_backend = static_cast<RHIBackend>(-1);
    current_flags   = 0;
    current_features.clear();
}
