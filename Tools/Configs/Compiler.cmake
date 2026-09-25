# Suppress developer warnings
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE BOOL "Suppress developer warnings")

# export compilation database
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# use position independent code for shared libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# enable C++ 20
set(CMAKE_CXX_STANDARD 20)

# For Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# For Visual Studio (export .lib for .dll)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Default symbol visibility for shared libraries (GCC/Clang)
set(CMAKE_CXX_VISIBILITY_PRESET default)
set(CMAKE_VISIBILITY_INLINES_HIDDEN OFF)

# Enable ccache
set(CMAKE_CXX_COMPILER_LAUNCHER ccache)

if(MSVC)
  # ignore the stupid warning from MSVC
  add_compile_options("/wd4996")

  # enable automatic multi-thread compilation
  add_compile_options("/MP")

  # ensure utf-8 encoding
  add_compile_options("/utf-8")
endif()
