#include "MetalUtils.h"

using namespace lyra;

MetalQuerySet::MetalQuerySet()
{
    // do nothing
}

MetalQuerySet::MetalQuerySet(const GPUQuerySetDescriptor& desc)
{
    auto rhi = get_rhi();

    type  = desc.type;
    count = desc.count;

    switch (desc.type) {
        case GPUQueryType::TIMESTAMP:
        {
            // check if device supports timestamp counters
            if (![rhi->device supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary]) {
                get_logger()->warn("Device does not support timestamp queries at stage boundaries");
            }

            // get timestamp counter set
            NSArray<id<MTLCounterSet>>* counter_sets          = [rhi->device counterSets];
            id<MTLCounterSet>           timestamp_counter_set = nil;

            for (id<MTLCounterSet> counter_set in counter_sets) {
                if ([[counter_set name] isEqualToString:MTLCommonCounterSetTimestamp]) {
                    timestamp_counter_set = counter_set;
                    break;
                }
            }

            if (!timestamp_counter_set) {
                get_logger()->error("Timestamp counter set not available on this device");
                throw GPUValidationError("Timestamp counter set not available on this device");
            }

            // create counter sample buffer descriptor
            MTLCounterSampleBufferDescriptor* buffer_desc = [MTLCounterSampleBufferDescriptor new];
            buffer_desc.counterSet                        = timestamp_counter_set;
            buffer_desc.sampleCount                       = desc.count;
            buffer_desc.storageMode                       = MTLStorageModeShared;
            buffer_desc.label                             = @"Timestamp Query Set";

            NSError* error = nil;
            sample_buffer  = [rhi->device newCounterSampleBufferWithDescriptor:buffer_desc error:&error];
            if (!sample_buffer || error) {
                NSString* error_str = error ? [error localizedDescription] : @"unknown error";
                get_logger()->error("Failed to create timestamp query set: {}", [error_str UTF8String]);
                throw GPUInternalError([NSString stringWithFormat:@"Failed to create timestamp query set: %@", error_str].UTF8String);
            }
            break;
        }

        case GPUQueryType::OCCLUSION:
        {
            // for occlusion queries, Metal uses visibility result buffer
            // create a buffer to store visibility results (8 bytes per query)
            NSUInteger buffer_size = desc.count * sizeof(uint64_t);
            visibility_buffer      = [rhi->device newBufferWithLength:buffer_size
                                                         options:MTLResourceStorageModeShared];
            if (!visibility_buffer) {
                get_logger()->error("Failed to create occlusion query buffer");
                throw GPUOutOfMemoryError("Failed to create occlusion query buffer");
            }
            break;
        }

        case GPUQueryType::BLAS_PROPERTIES:
        {
            // BLAS properties queries - create buffer for acceleration structure sizes
            NSUInteger buffer_size = desc.count * sizeof(MTLAccelerationStructureSizes);
            visibility_buffer      = [rhi->device newBufferWithLength:buffer_size
                                                         options:MTLResourceStorageModeShared];
            if (!visibility_buffer) {
                get_logger()->error("Failed to create BLAS properties query buffer");
                throw GPUOutOfMemoryError("Failed to create BLAS properties query buffer");
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
    sample_buffer     = nil;
    visibility_buffer = nil;
    count             = 0;
}
