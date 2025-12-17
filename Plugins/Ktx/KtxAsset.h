#pragma once

#include <Lyra/Common/Logger.h>
#include <Lyra/Plugin/AMS/AMSAPI.h>

using namespace lyra;

Logger get_logger();

struct KtxAsset
{
    static lyra::AssetHandlerAPI handler();
};
