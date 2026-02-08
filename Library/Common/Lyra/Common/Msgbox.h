#pragma once

#ifndef LYRA_LIBRARY_COMMON_MSGBOX_H
#define LYRA_LIBRARY_COMMON_MSGBOX_H

// library headers
#include <Lyra/Common/String.h>

namespace lyra
{
    void show_error(const String& title, const String& message);
    void show_warning(const String& title, const String& message);
    void show_message(const String& title, const String& message);

} // end of namespace lyra

#endif // LYRA_LIBRARY_COMMON_MSGBOX_H
