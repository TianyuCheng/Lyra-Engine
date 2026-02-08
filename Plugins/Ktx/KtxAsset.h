#pragma once

#include <Lyra/Common/Logger.h>
#include <Lyra/Assets/AMSAPI.h>

using namespace lyra;

Logger get_logger();

struct KtxAsset
{
    static lyra::AssetHandlerAPI handler();
};
