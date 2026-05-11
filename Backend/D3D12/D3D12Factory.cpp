// reference: https://alain.xyz/blog/raw-directx12
#include "D3D12Utils.h"

// instance apis
bool api::create_instance(const RHIDescriptor& desc)
{
    auto rhi = std::make_unique<D3D12RHI>();

    // record rhi flags
    rhi->rhiflags = desc.flags;

    UINT factory_flags = 0;

    // create a Debug Controller to track errors
    if (desc.flags.contains(RHIFlag::DEBUG) || desc.flags.contains(RHIFlag::VALIDATION)) {
        ID3D12Debug* dc;
        ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&dc)));
        ThrowIfFailed(dc->QueryInterface(IID_PPV_ARGS(&rhi->debug_control)));

        if (desc.flags.contains(RHIFlag::DEBUG))
            rhi->debug_control->EnableDebugLayer();

        if (desc.flags.contains(RHIFlag::VALIDATION)) {
            rhi->debug_control->SetEnableGPUBasedValidation(true);
            rhi->debug_control->SetEnableSynchronizedCommandQueueValidation(TRUE);
        }

        factory_flags |= DXGI_CREATE_FACTORY_DEBUG;

        dc->Release();
        dc = nullptr;
    }

    // create DXGI factory
    ThrowIfFailed(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&rhi->factory)));

    set_rhi(rhi.release());
    return true;
}

void api::delete_instance()
{
    auto rhi = get_rhi();

    if (rhi->debug_control) {
        rhi->debug_control->Release();
        rhi->debug_control = nullptr;
    }

    if (rhi->factory) {
        rhi->factory->Release();
        rhi->factory = nullptr;
    }
}
