#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cxxopts.hpp>

#include "Lexer.h"
#include "Parser.h"
#include "Generator.h"

namespace fs = std::filesystem;
using namespace lyra::reflect;

static std::string read_file(const fs::path& path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        return "";
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static bool write_if_changed(const fs::path& path, const std::string& content)
{
    if (fs::exists(path)) {
        std::string existing = read_file(path);
        if (existing == content) {
            // Unchanged, preserve timestamp
            return true;
        }
    }

    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path());
    }

    std::ofstream out(path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out) {
        std::cerr << "lyra-reflect: error: could not open output file: " << path << std::endl;
        return false;
    }

    out << content;
    return true;
}

int main(int argc, char* argv[])
{
    cxxopts::Options options("lyra-bindgen", "Lyra Engine C++ Binding Generator");

    options.add_options()
        ("m,module", "Module name", cxxopts::value<std::string>()->default_value("Engine"))
        ("p,prefix", "Include directory prefix (e.g. Lyra/Scene)", cxxopts::value<std::string>()->default_value(""))
        ("I,include-dir", "Public include directory for staging", cxxopts::value<std::string>()->default_value(""))
        ("o,output", "Output .gen.h header file", cxxopts::value<std::string>())
        ("i,headers", "Input header files to process", cxxopts::value<std::vector<std::string>>())
        ("h,help", "Print usage");

    options.parse_positional({"headers"});

    auto result = options.parse(argc, argv);

    if (result.count("help") || !result.count("output")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    std::string module_name = result["module"].as<std::string>();
    std::string prefix      = result.count("prefix") ? result["prefix"].as<std::string>() : "";
    std::string inc_dir     = result.count("include-dir") ? result["include-dir"].as<std::string>() : "";
    fs::path    output_path = result["output"].as<std::string>();

    std::vector<std::string> header_paths;
    if (result.count("headers")) {
        header_paths = result["headers"].as<std::vector<std::string>>();
    }

    ModuleReflection module_data;
    module_data.module_name = module_name;

    for (const auto& h_str : header_paths) {
        fs::path p(h_str);
        if (!fs::exists(p)) {
            std::cerr << "lyra-bindgen: warning: input header does not exist: " << p << std::endl;
            continue;
        }

        std::string content = read_file(p);

        // If the file is .hxx and an include-dir is provided, stage it as a public .h file
        if (p.extension() == ".hxx" && !inc_dir.empty()) {
            fs::path staged_path = fs::path(inc_dir);
            if (!prefix.empty()) {
                staged_path /= prefix;
            }
            staged_path /= (p.stem().string() + ".h");

            if (!write_if_changed(staged_path, content)) {
                std::cerr << "lyra-bindgen: error: could not write staged header: " << staged_path << std::endl;
                return 1;
            }
        }

        if (!Parser::fast_check(content)) {
            // Fast skip if no lyra annotations present
            continue;
        }

        Lexer  lexer(content, p.filename().string());
        Parser parser(lexer);

        size_t prev_comps = module_data.components.size();
        size_t prev_sys   = module_data.systems.size();

        parser.parse(module_data);

        // If any component or system was parsed from this file, record it in includes
        if (module_data.components.size() > prev_comps || module_data.systems.size() > prev_sys) {
            module_data.included_headers.push_back(p.filename().string());
        }
    }

    std::string generated_code = Generator::generate(module_data);

    if (!write_if_changed(output_path, generated_code)) {
        return 1;
    }

    return 0;
}
