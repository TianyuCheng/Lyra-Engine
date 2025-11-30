#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Plugin/AMS/AMSAPI.h>

#include "KtxAsset.h"

using namespace lyra;

LYRA_EXPORT auto prepare() -> void
{
    // do nothing
}

LYRA_EXPORT auto cleanup() -> void
{
    // do nothing
}

LYRA_EXPORT auto create() -> AssetHandlerAPI
{
    return KtxAsset::handler();
}
