include(FetchContent)

# define external project
FetchContent_Declare(
  boxer
  GIT_REPOSITORY https://github.com/aaronmjacobs/Boxer.git
  GIT_TAG        master
)

# get properties
FetchContent_GetProperties(boxer)

# populate boxer
if(NOT boxer_POPULATED)
  FetchContent_Populate(boxer)
endif()

# gather source files (based on operating system)
set(BOXER_SOURCES "")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  list(APPEND BOXER_SOURCES "${boxer_SOURCE_DIR}/src/boxer_win.cpp")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list(APPEND BOXER_SOURCES "${boxer_SOURCE_DIR}/src/boxer_linux.cpp")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  list(APPEND BOXER_SOURCES "${boxer_SOURCE_DIR}/src/boxer_mac.mm")
else()
  message(FATAL_ERROR "Boxer does not support unknown OS!")
endif()

# add boxer target
if(NOT TARGET boxer)
  add_library(boxer STATIC ${BOXER_SOURCES})
  target_include_directories(boxer PUBLIC
    $<BUILD_INTERFACE:${boxer_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )
  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    find_library(COCOA_LIBRARY Cocoa)
    target_link_libraries(boxer PRIVATE ${COCOA_LIBRARY})
  endif()
endif()

# mark boxer as found
set(boxer_FOUND TRUE)

# re-export target with namespace
add_library(boxer::boxer ALIAS boxer)

# move boxer under folder
set_target_properties(boxer PROPERTIES FOLDER "Vendors")
