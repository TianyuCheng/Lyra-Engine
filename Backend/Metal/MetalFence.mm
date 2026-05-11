#include "MetalUtils.h"
using namespace lyra;

MetalFence::MetalFence()
{
    // do nothing
}

MetalFence::MetalFence(bool signaled)
{
    auto rhi = get_rhi();
    event    = [rhi->device newSharedEvent];
    target   = signaled ? 1 : 0;
}

void MetalFence::wait(uint64_t timeout)
{
    if (!event) return;

    @autoreleasepool {
        // CPU wait for GPU signal using dispatch semaphore
        dispatch_semaphore_t    semaphore = dispatch_semaphore_create(0);
        MTLSharedEventListener* listener  = [[MTLSharedEventListener alloc] initWithDispatchQueue:dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)];
        [event notifyListener:listener
                      atValue:target
                        block:^(id<MTLSharedEvent>, uint64_t) {
                          dispatch_semaphore_signal(semaphore);
                        }];
        dispatch_semaphore_wait(semaphore, timeout == UINT64_MAX ? DISPATCH_TIME_FOREVER : dispatch_time(DISPATCH_TIME_NOW, timeout));
    }
}

void MetalFence::signal(uint64_t value)
{
    target = value;
}

void MetalFence::signal(id<MTLCommandBuffer> cmdbuf, uint64_t value)
{
    [cmdbuf encodeSignalEvent:event value:value];
    target = value;
}

void MetalFence::destroy()
{
    @autoreleasepool {
        event = nil;
    }
}
