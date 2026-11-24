#pragma once

#ifndef LYRA_ENGINE_RENDERING_PASSES_PASS_H
#define LYRA_ENGINE_RENDERING_PASSES_PASS_H

// dedicated modular pass headers
#include <Lyra/Rendering/Passes/ClearPass.h>
#include <Lyra/Rendering/Passes/DepthPrepass.h>
#include <Lyra/Rendering/Passes/EditorGridPass.h>
#include <Lyra/Rendering/Passes/GBufferPass.h>
#include <Lyra/Rendering/Passes/BloomPass.h>
#include <Lyra/Rendering/Passes/AutoExposurePass.h>
#include <Lyra/Rendering/Passes/DepthOfFieldPass.h>
#include <Lyra/Rendering/Passes/FXAAPass.h>
#include <Lyra/Rendering/Passes/TonemappingPass.h>
#include <Lyra/Rendering/Passes/SkyboxPass.h>
#include <Lyra/Rendering/Passes/LightCullingPass.h>
#include <Lyra/Rendering/Passes/HiZPyramidPass.h>
#include <Lyra/Rendering/Passes/ShadowMapPass.h>
#include <Lyra/Rendering/Passes/ForwardPass.h>

#endif // LYRA_ENGINE_RENDERING_PASSES_PASS_H
