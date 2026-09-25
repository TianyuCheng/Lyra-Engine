find_package(cxxopts REQUIRED)

add_executable(lyra-reflect
    Main.cpp
    Lexer.h
    Lexer.cpp
    Parser.h
    Parser.cpp
    Generator.h
    Generator.cpp
)

target_link_libraries(lyra-reflect PRIVATE cxxopts::cxxopts)

set_target_properties(lyra-reflect PROPERTIES FOLDER "Tools")
