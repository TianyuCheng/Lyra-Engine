#include "MetalUtils.h"
using namespace lyra;

void MetalCommandPool::init(id<MTLCommandQueue> queue)
{
    @autoreleasepool {
        command_queue = queue;
    }
}

void MetalCommandPool::destroy()
{
    @autoreleasepool {
        command_queue = nil;
    }
}

id<MTLCommandBuffer> MetalCommandPool::allocate()
{
    @autoreleasepool {
        return command_queue ? [command_queue commandBuffer] : nil;
    }
}
