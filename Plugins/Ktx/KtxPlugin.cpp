#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Plugin/AMS/AMSAPI.h>

#include "KtxAsset.h"

using namespace lyra;

LYRA_EXPORT auto prepare() -> void
{
    get_logger()->set_level(parse_log_level_from_env("LYRA_KTX_VERBOSITY"));
}

LYRA_EXPORT auto cleanup() -> void
{
    // do nothing
}

LYRA_EXPORT auto create() -> AssetHandlerAPI
{
    return KtxAsset::handler();
}
