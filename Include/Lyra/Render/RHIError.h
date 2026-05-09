#pragma once

#ifndef LYRA_LYRA_RENDER_RHIERROR_H
#define LYRA_LYRA_RENDER_RHIERROR_H

#include <stdexcept>

namespace lyra
{
    struct GPUError : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    struct GPUInternalError : public GPUError
    {
        using GPUError::GPUError;
    };

    struct GPUOutOfMemoryError : public GPUError
    {
        using GPUError::GPUError;
    };

    struct GPUPipelineError : public GPUError
    {
        using GPUError::GPUError;
    };

    struct GPUDeviceLostInfo : public GPUError
    {
        using GPUError::GPUError;
    };

    struct GPUUncapturedErrorEvent : public GPUError
    {
        using GPUError::GPUError;
    };

    struct GPUValidationError : public GPUError
    {
        using GPUError::GPUError;
    };

    struct GPUCompilationInfo : public GPUError
    {
        using GPUError::GPUError;
    };

    struct GPUCompilationMessage : public GPUError
    {
        using GPUError::GPUError;
    };

} // namespace lyra

#endif // LYRA_LYRA_RENDER_RHIERROR_H
