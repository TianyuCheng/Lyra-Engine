#pragma once

#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/String.h>

using namespace lyra;

namespace lyra::model
{
    inline Logger get_logger()
    {
        static Logger logger = create_logger("Model", LogLevel::trace);
        return logger;
    }
} // namespace lyra::model
