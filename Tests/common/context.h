#ifndef LYRA_TESTLIB_HELPER_CONTEXT_H
#define LYRA_TESTLIB_HELPER_CONTEXT_H

#include "./render.h"

struct TestContext
{
    static auto get() -> TestContext&;

    bool acquire(RHIBackend backend, RHIFlags flags, const Vector<GPUFeatureName>& features);
    void release();

    OwnedResource<RHI>     rhi;
    GPUAdapter             adapter;
    GPUDevice              device;
    Own<FileLoader>        loader;

    RHIBackend             current_backend = static_cast<RHIBackend>(-1);
    RHIFlags               current_flags   = 0;
    Vector<GPUFeatureName> current_features;

    bool   feature_unsupported = false;
    String missing_feature_name;
};

#endif // LYRA_TESTLIB_HELPER_CONTEXT_H
