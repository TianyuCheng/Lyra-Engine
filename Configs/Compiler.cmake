# Suppress developer warnings
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE BOOL "Suppress developer warnings")

# export compilation database
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# use position independent code for shared libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# enable C++ 17
set(CMAKE_CXX_STANDARD 17)

# For Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# For Visual Studio (export .lib for .dll)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Enable ccache
set(CMAKE_CXX_COMPILER_LAUNCHER ccache)

# Other settings
if(MSVC)
  # ignore the stupid warning from MSVC
  add_compile_options("/wd4996")

  # enable automatic multi-thread compilation
  add_compile_options("/MP")

  # ensure utf-8 encoding
  add_compile_options("/utf-8")
endif()
