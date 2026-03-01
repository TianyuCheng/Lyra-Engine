lyra_sample(editor)
find_package(cxxopts REQUIRED)
target_sources(lyra-editor PRIVATE main.cpp
    renderer.h
    renderer.cpp
)

# add custom target to run sample executable
add_custom_target(
  editor
  COMMAND $<TARGET_FILE:lyra-editor> --project $ENV{LYRA_DEFAULT_PROJECT}
  COMMENT "Run Lyra::Editor"
  DEPENDS lyra-editor
  VERBATIM)

# move to Targets folder
set_target_properties(editor PROPERTIES FOLDER "Targets")
