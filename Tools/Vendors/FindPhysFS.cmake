find_package(PhysFS CONFIG REQUIRED)

# re-export target with namespace
if(NOT TARGET PhysFS::PhysFS)
  if(TARGET PhysFS::PhysFS-static)
    add_library(PhysFS::PhysFS ALIAS PhysFS::PhysFS-static)
  elseif(TARGET PhysFS::PhysFS-shared)
    add_library(PhysFS::PhysFS ALIAS PhysFS::PhysFS-shared)
  endif()
endif()
