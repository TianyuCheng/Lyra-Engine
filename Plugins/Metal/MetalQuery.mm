#include "MetalUtils.h"

using namespace lyra;

MetalQuerySet::MetalQuerySet()
{
    // do nothing
}

MetalQuerySet::MetalQuerySet(const GPUQuerySetDescriptor& desc)
{
    auto rhi = get_rhi();

    type = desc.type;
    count = desc.count;

    switch (desc.type) {
        case GPUQueryType::TIMESTAMP:
        {
            // check if device supports timestamp counters
            if (![rhi->device supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary]) {
                get_logger()->warn("Device does not support timestamp queries at stage boundaries");
            }

            // get timestamp counter set
            NSArray<id<MTLCounterSet>>* counter_sets = [rhi->device counterSets];
            id<MTLCounterSet> timestamp_counter_set = nil;

            for (id<MTLCounterSet> counter_set in counter_sets) {
                if ([[counter_set name] isEqualToString:MTLCommonCounterSetTimestamp]) {
                    timestamp_counter_set = counter_set;
                    break;
                }
            }

            if (!timestamp_counter_set) {
                get_logger()->error("Timestamp counter set not available on this device");
                return;
            }

            // create counter sample buffer descriptor
            MTLCounterSampleBufferDescriptor* buffer_desc = [MTLCounterSampleBufferDescriptor new];
            buffer_desc.counterSet = timestamp_counter_set;
            buffer_desc.sampleCount = desc.count;
            buffer_desc.storageMode = MTLStorageModeShared;
            buffer_desc.label = @"Timestamp Query Set";

            NSError* error = nil;
            sample_buffer = [rhi->device newCounterSampleBufferWithDescriptor:buffer_desc error:&error];
            if (error) {
                get_logger()->error("Failed to create timestamp query set: {}", [[error localizedDescription] UTF8String]);
                return;
            }
            break;
        }

        case GPUQueryType::OCCLUSION:
        {
            // for occlusion queries, Metal uses visibility result buffer
            // create a buffer to store visibility results (8 bytes per query)
            NSUInteger buffer_size = desc.count * sizeof(uint64_t);
            visibility_buffer = [rhi->device newBufferWithLength:buffer_size
                                                         options:MTLResourceStorageModeShared];
            if (!visibility_buffer) {
                get_logger()->error("Failed to create occlusion query buffer");
                return;
            }
            break;
        }

        case GPUQueryType::BLAS_PROPERTIES:
        {
            // BLAS properties queries - create buffer for acceleration structure sizes
            NSUInteger buffer_size = desc.count * sizeof(MTLAccelerationStructureSizes);
            visibility_buffer = [rhi->device newBufferWithLength:buffer_size
                                                         options:MTLResourceStorageModeShared];
            if (!visibility_buffer) {
                get_logger()->error("Failed to create BLAS properties query buffer");
                return;
            }
            break;
        }

        case GPUQueryType::PIPELINE_STATISTICS:
        {
            // pipeline statistics queries - check for counter support
            get_logger()->warn("Pipeline statistics queries are limited on Metal");
            break;
        }
    }
}

void MetalQuerySet::destroy()
{
    sample_buffer = nil;
    visibility_buffer = nil;
    count = 0;
}

bool api::create_query_set(GPUQuerySetHandle& handle, const GPUQuerySetDescriptor& desc)
{
    auto rhi = get_rhi();
    auto obj = MetalQuerySet(desc);

    // for timestamp queries, check sample_buffer
    // for occlusion queries, check visibility_buffer
    bool is_valid = false;
    switch (desc.type) {
        case GPUQueryType::TIMESTAMP:
            is_valid = (obj.sample_buffer != nil);
            break;
        case GPUQueryType::OCCLUSION:
        case GPUQueryType::BLAS_PROPERTIES:
            is_valid = (obj.visibility_buffer != nil);
            break;
        case GPUQueryType::PIPELINE_STATISTICS:
            // Pipeline statistics have limited support, allow creation
            is_valid = true;
            break;
    }

    if (!is_valid) {
        return false;
    }

    auto ind = rhi->query_sets.add(obj);
    handle = GPUQuerySetHandle(ind);
    return true;
}

void api::delete_query_set(GPUQuerySetHandle handle)
{
    get_rhi()->query_sets.remove(handle.value);
}
