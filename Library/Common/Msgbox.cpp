#include <Lyra/Common/Msgbox.h>

// vendor headers
#include <boxer/boxer.h>

using namespace lyra;

void lyra::show_error(const String& title, const String& message)
{
    boxer::show(message.c_str(), title.c_str(), boxer::Style::Error);
}

void lyra::show_warning(const String& title, const String& message)
{
    boxer::show(message.c_str(), title.c_str(), boxer::Style::Warning);
}

void lyra::show_message(const String& title, const String& message)
{
    boxer::show(message.c_str(), title.c_str(), boxer::Style::Info);
}
