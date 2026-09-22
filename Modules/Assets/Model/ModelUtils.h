#pragma once

#include <Lyra/Common/Logger.h>
#include <Lyra/Common/String.h>

using namespace lyra;

namespace lyra::model
{
    inline Logger get_logger()
    {
        static Logger logger = create_logger("Model", LogLevel::trace);
        return logger;
    }
} // namespace lyra::model
