include(GNUInstallDirs)

set(PROJECT_NAME "lyra")
set(PROJECT_PREFIX "")

# specify the binary directory
set(LIBRARY_BIN_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
if(NOT EXISTS ${LIBRARY_BIN_DIRECTORY})
  # create the directory
  file(MAKE_DIRECTORY ${LIBRARY_BIN_DIRECTORY})
  message(STATUS "Bin Directory: ${LIBRARY_BIN_DIRECTORY}")
endif()

# define a macro for shared library
macro(lyra_shared NAME)
  set(TARGET_NAME    "${PROJECT_NAME}-${NAME}")
  set(NAMESPACE_NAME "${PROJECT_NAME}::${NAME}")

  # add library target
  add_library(${TARGET_NAME} SHARED)

  # change the dll path (to allow easier linking)
  set_target_properties(${TARGET_NAME} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${LIBRARY_BIN_DIRECTORY}/
      LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_BIN_DIRECTORY}/
      ARCHIVE_OUTPUT_DIRECTORY ${LIBRARY_BIN_DIRECTORY}/)

  # re-export target with namespace
  add_library(${NAMESPACE_NAME} ALIAS ${TARGET_NAME})

  # additional definitions
  target_compile_definitions(${TARGET_NAME} PRIVATE LYRA_BUILD_SHARED)

  # IDE target folders
  set_target_properties(${TARGET_NAME} PROPERTIES PREFIX "")
  set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "Library")

  # install shared library
  install(
      TARGETS ${TARGET_NAME}
      EXPORT ${TARGET_NAME}-targets
      DESTINATION ${CMAKE_INSTALL_LIBDIR})
endmacro()

# define a macro for interface library
macro(lyra_header NAME)
  set(TARGET_NAME    "${PROJECT_NAME}-${NAME}")
  set(NAMESPACE_NAME "${PROJECT_NAME}::${NAME}")

  # add library target
  add_library(${TARGET_NAME} INTERFACE)

  # change the dll path (to allow easier linking)
  set_target_properties(${TARGET_NAME} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${LIBRARY_BIN_DIRECTORY}/
      LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_BIN_DIRECTORY}/
      ARCHIVE_OUTPUT_DIRECTORY ${LIBRARY_BIN_DIRECTORY}/)

  # re-export target with namespace
  add_library(${NAMESPACE_NAME} ALIAS ${TARGET_NAME})

  # IDE target folders
  set_target_properties(${TARGET_NAME} PROPERTIES PREFIX "")
  set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "Library")

  # install header library
  install(
      TARGETS ${TARGET_NAME}
      EXPORT ${TARGET_NAME}-targets
      DESTINATION ${CMAKE_INSTALL_LIBDIR})

endmacro()

# define a macro for module registration (supports OBJECT, STATIC, MODULE, SHARED)
macro(lyra_module NAME TYPE)
  set(TARGET_NAME    "${PROJECT_NAME}-${NAME}")
  set(NAMESPACE_NAME "${PROJECT_NAME}::${NAME}")

  # add target based on specified library type
  add_library(${TARGET_NAME} ${TYPE})

  # re-export target with namespace
  add_library(${NAMESPACE_NAME} ALIAS ${TARGET_NAME})

  # configure properties and dependencies based on library type
  if("${TYPE}" STREQUAL "MODULE" OR "${TYPE}" STREQUAL "SHARED")
    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${LIBRARY_BIN_DIRECTORY}/
        LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_BIN_DIRECTORY}/
        ARCHIVE_OUTPUT_DIRECTORY ${LIBRARY_BIN_DIRECTORY}/
        PREFIX "")
    target_link_libraries(${TARGET_NAME} PUBLIC lyra::engine)
    target_link_libraries(${TARGET_NAME} PUBLIC lyra::commons)
  else()
    if("${TYPE}" STREQUAL "STATIC")
      set_target_properties(${TARGET_NAME} PROPERTIES
          RUNTIME_OUTPUT_DIRECTORY ${LIBRARY_BIN_DIRECTORY}/
          LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_BIN_DIRECTORY}/
          ARCHIVE_OUTPUT_DIRECTORY ${LIBRARY_BIN_DIRECTORY}/
          PREFIX "")
    endif()
    target_link_libraries(${TARGET_NAME} PUBLIC lyra::headers)
    target_link_libraries(${TARGET_NAME} PUBLIC lyra::commons)
  endif()

  # IDE target folders
  set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "Modules")
endmacro()

# define a macro for sample registration
macro(lyra_sample NAME)
  set(TARGET_NAME    "${PROJECT_NAME}-${NAME}")
  set(NAMESPACE_NAME "${PROJECT_NAME}::${NAME}")

  # add executable target
  add_executable(${TARGET_NAME})

  # change the dll path (to allow easier linking)
  set_target_properties(${TARGET_NAME} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${LIBRARY_BIN_DIRECTORY}/
      LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_BIN_DIRECTORY}/
      ARCHIVE_OUTPUT_DIRECTORY ${LIBRARY_BIN_DIRECTORY}/)

  # re-export target with namespace
  add_executable(${NAMESPACE_NAME} ALIAS ${TARGET_NAME})

  set_target_properties(${TARGET_NAME} PROPERTIES PREFIX "")
  set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "Samples")
  target_link_libraries(${TARGET_NAME} PUBLIC lyra::engine)

  # add custom target to run sample executable
  add_custom_target(
    show-${NAME}
    COMMAND ${CMAKE_COMMAND} -E echo $<TARGET_FILE_DIR:lyra-${NAME}>
    COMMAND ${CMAKE_COMMAND} -E echo $<TARGET_FILE_NAME:lyra-${NAME}>
    VERBATIM)

  # move to Targets folder
  set_target_properties(show-${NAME} PROPERTIES FOLDER "Targets")
endmacro()

# define a macro for toolkit
macro(lyra_toolkit NAME)
  set(TARGET_NAME    "${PROJECT_NAME}-${NAME}")
  set(NAMESPACE_NAME "${PROJECT_NAME}::${NAME}")

  # add executable target
  add_executable(${TARGET_NAME})

  # change the dll path (to allow easier linking)
  set_target_properties(${TARGET_NAME} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${LIBRARY_BIN_DIRECTORY}/
      LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_BIN_DIRECTORY}/
      ARCHIVE_OUTPUT_DIRECTORY ${LIBRARY_BIN_DIRECTORY}/)

  # re-export target with namespace
  add_executable(${NAMESPACE_NAME} ALIAS ${TARGET_NAME})

  set_target_properties(${TARGET_NAME} PROPERTIES PREFIX "")
  set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "Toolkit")
  target_link_libraries(${TARGET_NAME} PUBLIC lyra::engine)

  # add custom target to run sample executable
  add_custom_target(
    show-${NAME}
    COMMAND ${CMAKE_COMMAND} -E echo $<TARGET_FILE_DIR:lyra-${NAME}>
    COMMAND ${CMAKE_COMMAND} -E echo $<TARGET_FILE_NAME:lyra-${NAME}>
    VERBATIM)

  # move to Targets folder
  set_target_properties(show-${NAME} PROPERTIES FOLDER "Targets")
endmacro()

# define a function to bind headers for a target using lyra-bindgen
function(lyra_bindgen TARGET_NAME)
  cmake_parse_arguments(BINDGEN "" "MODULE;PREFIX;OUTPUT;INCLUDE_DIR" "HEADERS" ${ARGN})

  if(NOT BINDGEN_MODULE)
    set(BINDGEN_MODULE "${TARGET_NAME}")
  endif()

  if(NOT BINDGEN_PREFIX)
    set(BINDGEN_PREFIX "Lyra")
  endif()

  if(NOT BINDGEN_INCLUDE_DIR)
    set(BINDGEN_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/Include")
  endif()

  if(NOT BINDGEN_OUTPUT)
    set(BINDGEN_OUTPUT "${BINDGEN_INCLUDE_DIR}/Lyra/Scripting/${BINDGEN_MODULE}.gen.h")
  endif()

  set(ABSOLUTE_HEADERS "")
  set(STAGED_OUTPUTS "")
  foreach(HEADER ${BINDGEN_HEADERS})
    if(NOT IS_ABSOLUTE "${HEADER}")
      set(HEADER_ABS "${CMAKE_CURRENT_SOURCE_DIR}/${HEADER}")
    else()
      set(HEADER_ABS "${HEADER}")
    endif()
    list(APPEND ABSOLUTE_HEADERS "${HEADER_ABS}")

    get_filename_component(HEADER_EXT "${HEADER}" EXT)
    get_filename_component(HEADER_NAME_WE "${HEADER}" NAME_WE)
    if("${HEADER_EXT}" STREQUAL ".hxx")
      list(APPEND STAGED_OUTPUTS "${BINDGEN_INCLUDE_DIR}/${BINDGEN_PREFIX}/${HEADER_NAME_WE}.h")
    endif()
  endforeach()

  get_filename_component(OUT_DIR "${BINDGEN_OUTPUT}" DIRECTORY)

  add_custom_command(
    OUTPUT "${BINDGEN_OUTPUT}" ${STAGED_OUTPUTS}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BINDGEN_INCLUDE_DIR}/${BINDGEN_PREFIX}"
    COMMAND lyra-bindgen -m "${BINDGEN_MODULE}" -p "${BINDGEN_PREFIX}" -I "${BINDGEN_INCLUDE_DIR}" -o "${BINDGEN_OUTPUT}" ${ABSOLUTE_HEADERS}
    DEPENDS lyra-bindgen ${ABSOLUTE_HEADERS}
    COMMENT "Running lyra-bindgen for ${TARGET_NAME} -> ${BINDGEN_OUTPUT}"
    VERBATIM
  )

  target_sources(${TARGET_NAME} PRIVATE "${BINDGEN_OUTPUT}" ${STAGED_OUTPUTS})
  target_include_directories(${TARGET_NAME} PUBLIC "${BINDGEN_INCLUDE_DIR}")
  target_include_directories(${TARGET_NAME} PRIVATE "${OUT_DIR}")
endfunction()

function(lyra_reflect TARGET_NAME)
  lyra_bindgen(${TARGET_NAME} ${ARGN})
endfunction()

