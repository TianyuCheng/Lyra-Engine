find_package(nfd CONFIG REQUIRED)

# re-export target with namespace if needed
if(TARGET nfd AND NOT TARGET nfd::nfd)
  add_library(nfd::nfd ALIAS nfd)
endif()
