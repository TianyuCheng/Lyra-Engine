#pragma once

#ifndef LYRA_EDITOR_COMMON_COLORS_H
#define LYRA_EDITOR_COMMON_COLORS_H

#include <Lyra/Utilities/Math.h>

#define LYRA_COLOR_TRACE    lyra::Vector4(0.6f, 0.6f, 0.6f, 1.0f)     // light gray
#define LYRA_COLOR_DEBUG    lyra::Vector4(0.3f, 0.7f, 1.0f, 1.0f)     // cyan-ish
#define LYRA_COLOR_INFO     lyra::Vector4(0.2f, 0.9f, 0.2f, 1.0f)     // green
#define LYRA_COLOR_WARN     lyra::Vector4(1.0f, 0.8f, 0.2f, 1.0f)     // yellow-orange
#define LYRA_COLOR_ERROR    lyra::Vector4(1.0f, 0.25f, 0.25f, 1.0f)   // red
#define LYRA_COLOR_CRITICAL lyra::Vector4(1.0f, 0.0f, 0.8f, 1.0f)     // magenta
#define LYRA_COLOR_DISABLED lyra::Vector4(0.95f, 0.96f, 0.00f, 1.00f) // disabled

#define LYRA_COLOR_FOLDER   lyra::Vector4(0.3f, 0.7f, 1.0f, 1.0f) // folder color

#endif // LYRA_LYRA_EDITOR_COLORS_H
