#pragma once

#ifndef LYRA_REFLECT_LEXER_H
#define LYRA_REFLECT_LEXER_H

#include <string>
#include <string_view>
#include <vector>

namespace lyra::reflect
{
    enum struct TokenType
    {
        Eof,
        Identifier,
        StringLiteral,
        NumberLiteral,

        // Brackets & delimiters
        LBracket,        // [
        RBracket,        // ]
        DoubleLBracket,  // [[
        DoubleRBracket,  // ]]
        LParen,          // (
        RParen,          // )
        LBrace,          // {
        RBrace,          // }
        Colon,           // :
        ColonColon,      // ::
        Semicolon,       // ;
        Comma,           // ,
        Equal,           // =
        Ampersand,       // &
        Less,            // <
        Greater,         // >

        // Keywords
        KeywordStruct,
        KeywordClass,
        KeywordUsing,
        KeywordVoid,
        KeywordConst,
        KeywordAuto,

        Unknown
    };

    struct Token
    {
        TokenType   type   = TokenType::Eof;
        std::string text;
        int         line   = 1;
        int         column = 1;
    };

    class Lexer
    {
    public:
        explicit Lexer(std::string_view source, std::string filename = "");

        auto next_token() -> Token;
        auto peek_token() -> Token;

        auto get_filename() const -> const std::string& { return filename; }

    private:
        char peek(size_t offset = 0) const;
        char advance();
        bool is_at_end() const;

        void skip_whitespace_and_comments();
        auto scan_identifier_or_keyword() -> Token;
        auto scan_number() -> Token;
        auto scan_string() -> Token;

        std::string_view source;
        std::string      filename;
        size_t           cursor = 0;
        int              line   = 1;
        int              column = 1;

        std::vector<Token> peek_buffer;
    };

} // namespace lyra::reflect

#endif // LYRA_REFLECT_LEXER_H
