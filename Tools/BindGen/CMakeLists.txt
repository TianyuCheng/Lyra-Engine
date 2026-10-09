find_package(cxxopts REQUIRED)

add_executable(lyra-bindgen
    Main.cpp
    Lexer.h
    Lexer.cpp
    Parser.h
    Parser.cpp
    Generator.h
    Generator.cpp
)

target_link_libraries(lyra-bindgen PRIVATE cxxopts::cxxopts)

set_target_properties(lyra-bindgen PROPERTIES FOLDER "Tools")
