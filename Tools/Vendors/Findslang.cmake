# Reference: https://github.com/microsoft/vcpkg/issues/38937

# Set the Slang installation paths
set(SHADER_SLANG_LIB_PATH "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib")
set(SHADER_SLANG_BIN_PATH "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
set(SHADER_SLANG_INC_PATH "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")

# Handle platform-specific library prefixes and extensions
if (WIN32)
    set(SHADER_SLANG_LIB_EXTENSION "lib")
    set(SHADER_SLANG_BIN_EXTENSION "dll")
    set(SHADER_SLANG_LIB_PREFIX "")
    set(SHADER_SLANG_DLL_COPY_PREFIX "${SHADER_SLANG_BIN_PATH}/")
elseif (APPLE)
    set(SHADER_SLANG_LIB_EXTENSION "dylib")
    set(SHADER_SLANG_BIN_EXTENSION "dylib")
    set(SHADER_SLANG_LIB_PREFIX "lib")
    set(SHADER_SLANG_DLL_COPY_PREFIX "${SHADER_SLANG_LIB_PATH}/")
else ()
    set(SHADER_SLANG_LIB_EXTENSION "so")
    set(SHADER_SLANG_BIN_EXTENSION "so")
    set(SHADER_SLANG_LIB_PREFIX "lib")
    set(SHADER_SLANG_DLL_COPY_PREFIX "${SHADER_SLANG_LIB_PATH}/")
endif ()

# Define a function to find and import a shared library
function(find_and_import_library target_name lib_name)
    find_library(${target_name}_LIBRARY
            NAMES "${SHADER_SLANG_LIB_PREFIX}${lib_name}.${SHADER_SLANG_LIB_EXTENSION}"
            HINTS ${SHADER_SLANG_LIB_PATH}
    )

    add_library(slang::${lib_name} SHARED IMPORTED)

    # Windows requires IMPORTED_IMPLIB, while other platforms use IMPORTED_LOCATION
    if (WIN32)
        set_target_properties(slang::${lib_name} PROPERTIES IMPORTED_IMPLIB ${${target_name}_LIBRARY})
        set_target_properties(slang::${lib_name} PROPERTIES IMPORTED_LOCATION "${SHADER_SLANG_BIN_PATH}/${lib_name}.${SHADER_SLANG_BIN_EXTENSION}")
    else()
        set_target_properties(slang::${lib_name} PROPERTIES IMPORTED_LOCATION ${${target_name}_LIBRARY})
    endif()
endfunction()

# Find and import Slang-related libraries
find_and_import_library(SHADER_SLANG slang)
find_and_import_library(SHADER_SLANG_RT slang-rt)
find_and_import_library(SHADER_SLANG_GFX gfx)

# Store the list of Slang libraries
set(SHADER_SLANG_LIBS slang::slang slang::slang-rt slang::gfx)

# Handle DLL copying logic (Windows copies .dll, other platforms copy .so/.dylib)
if (WIN32)
    file(GLOB SHADER_SLANG_DLLS "${SHADER_SLANG_DLL_COPY_PREFIX}slang*.${SHADER_SLANG_BIN_EXTENSION}")
    set(SHADER_SLANG_GFX_DLL "${SHADER_SLANG_DLL_COPY_PREFIX}gfx.${SHADER_SLANG_BIN_EXTENSION}")
else()
    file(GLOB SHADER_SLANG_DLLS "${SHADER_SLANG_DLL_COPY_PREFIX}${SHADER_SLANG_LIB_PREFIX}slang*.${SHADER_SLANG_BIN_EXTENSION}")
    set(SHADER_SLANG_GFX_DLL "${SHADER_SLANG_DLL_COPY_PREFIX}${SHADER_SLANG_LIB_PREFIX}gfx.${SHADER_SLANG_BIN_EXTENSION}")
endif()

# Copy the shared libraries to the target executable directory after build
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${SHADER_SLANG_DLLS} ${SHADER_SLANG_GFX_DLL} "$<TARGET_FILE_DIR:${TARGET_NAME}>"
)

# Link dependencies
target_link_libraries(${TARGET_NAME} PRIVATE ${SHADER_SLANG_LIBS})
target_include_directories(${TARGET_NAME} PUBLIC ${SHADER_SLANG_INC_PATH})

# Move to Vendors directory
set_target_properties(slang::gfx PROPERTIES FOLDER "Vendors")
set_target_properties(slang::slang PROPERTIES FOLDER "Vendors")
set_target_properties(slang::slang-rt PROPERTIES FOLDER "Vendors")
