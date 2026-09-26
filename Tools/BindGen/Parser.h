#pragma once

#ifndef LYRA_REFLECT_PARSER_H
#define LYRA_REFLECT_PARSER_H

#include "Lexer.h"
#include "Model.h"

namespace lyra::reflect
{
    class Parser
    {
    public:
        explicit Parser(Lexer& lexer);

        /**
         * @brief Quick check if source contains any lyra attribute markers.
         */
        static bool fast_check(std::string_view source);

        /**
         * @brief Parses annotated structs and systems into the module reflection model.
         */
        bool parse(ModuleReflection& out_module);

    private:
        auto consume(TokenType expected, const std::string& error_msg = "") -> Token;
        bool match(TokenType type);
        bool check(TokenType type);

        auto parse_attribute_list() -> std::vector<Attribute>;
        void parse_attribute(std::vector<Attribute>& out_attrs, const std::string& prefix_scope = "");

        void parse_struct(const std::vector<Attribute>& attrs, ModuleReflection& out_module);
        void parse_field(ComponentMeta& comp);

        void parse_function(const std::vector<Attribute>& attrs, ModuleReflection& out_module);
        auto parse_parameter() -> ParamMeta;

        void synchronize_to_statement();

        Lexer& lexer;
        Token  current;
    };

} // namespace lyra::reflect

#endif // LYRA_REFLECT_PARSER_H
