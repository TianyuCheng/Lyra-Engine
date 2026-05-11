#include "MetalUtils.h"

using namespace lyra;

static Logger logger = create_logger("Metal", LogLevel::info);

// global Metal RHI instance
static MetalRHI* MTL_RHI = nullptr;

// logger instance
Logger get_logger()
{
    return logger;
}

// RHI instance management
void set_rhi(MetalRHI* instance)
{
    MTL_RHI = instance;
}

auto get_rhi() -> MetalRHI*
{
    return MTL_RHI;
}

// MetalRHI wait_idle implementation
void MetalRHI::wait_idle()
{
    @autoreleasepool {

        // wait for graphics queues to complete
        if (graphics_queue) {
            // create a temporary command buffer and wait for it to complete
            id<MTLCommandBuffer> cmd = [graphics_queue commandBuffer];
            [cmd commit];
            [cmd waitUntilCompleted];
        }

        // wait for compute queues to complete
        if (compute_queue && compute_queue != graphics_queue) {
            id<MTLCommandBuffer> cmd = [compute_queue commandBuffer];
            [cmd commit];
            [cmd waitUntilCompleted];
        }

        // wait for transfer queues to complete
        if (transfer_queue && transfer_queue != graphics_queue && transfer_queue != compute_queue) {
            id<MTLCommandBuffer> cmd = [transfer_queue commandBuffer];
            [cmd commit];
            [cmd waitUntilCompleted];
        }
    }
}
