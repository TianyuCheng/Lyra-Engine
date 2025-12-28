#include "MetalUtils.h"

using namespace lyra;

// Metal instance creation (minimal for Metal - mainly device enumeration)
bool api::create_instance(const RHIDescriptor& desc)
{
    auto rhi      = new MetalRHI();
    rhi->rhiflags = desc.flags;
    set_rhi(rhi);

    get_logger()->info("Metal RHI instance created");
    return true;
}

void api::delete_instance()
{
    auto rhi = get_rhi();
    if (rhi) {
        delete rhi;
        set_rhi(nullptr);
    }
    get_logger()->info("Metal RHI instance deleted");
}
