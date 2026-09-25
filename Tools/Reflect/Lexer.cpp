#include "Lexer.h"
#include <cctype>

using namespace lyra::reflect;

Lexer::Lexer(std::string_view source, std::string filename)
    : source(source), filename(std::move(filename))
{
}

char Lexer::peek(size_t offset) const
{
    if (cursor + offset >= source.size()) {
        return '\0';
    }
    return source[cursor + offset];
}

char Lexer::advance()
{
    if (is_at_end()) {
        return '\0';
    }
    char c = source[cursor++];
    if (c == '\n') {
        line++;
        column = 1;
        return c;
    }
    column++;
    return c;
}

bool Lexer::is_at_end() const
{
    return cursor >= source.size();
}

void Lexer::skip_whitespace_and_comments()
{
    while (!is_at_end()) {
        char c = peek();
        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
            continue;
        }

        // Single-line comment
        if (c == '/' && peek(1) == '/') {
            advance();
            advance();
            while (!is_at_end() && peek() != '\n') {
                advance();
            }
            continue;
        }

        // Multi-line comment
        if (c == '/' && peek(1) == '*') {
            advance();
            advance();
            while (!is_at_end()) {
                if (peek() == '*' && peek(1) == '/') {
                    advance();
                    advance();
                    break;
                }
                advance();
            }
            continue;
        }

        // Preprocessor lines (e.g. #include, #pragma, #define) - skip until newline
        if (c == '#') {
            advance();
            while (!is_at_end() && peek() != '\n') {
                advance();
            }
            continue;
        }

        break;
    }
}

Token Lexer::scan_identifier_or_keyword()
{
    int    start_col  = column;
    int    start_line = line;
    size_t start      = cursor;

    while (!is_at_end()) {
        char c = peek();
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            advance();
            continue;
        }
        break;
    }

    std::string text(source.substr(start, cursor - start));
    TokenType   type = TokenType::Identifier;

    if (text == "struct") {
        type = TokenType::KeywordStruct;
    } else if (text == "class") {
        type = TokenType::KeywordClass;
    } else if (text == "using") {
        type = TokenType::KeywordUsing;
    } else if (text == "void") {
        type = TokenType::KeywordVoid;
    } else if (text == "const") {
        type = TokenType::KeywordConst;
    } else if (text == "auto") {
        type = TokenType::KeywordAuto;
    }

    return Token{type, std::move(text), start_line, start_col};
}

Token Lexer::scan_number()
{
    int    start_col  = column;
    int    start_line = line;
    size_t start      = cursor;

    // Optional leading minus / plus if already consumed
    while (!is_at_end()) {
        char c = peek();
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == 'f' || c == 'F' || c == 'u' || c == 'U') {
            advance();
            continue;
        }
        break;
    }

    std::string text(source.substr(start, cursor - start));
    return Token{TokenType::NumberLiteral, std::move(text), start_line, start_col};
}

Token Lexer::scan_string()
{
    int start_col  = column;
    int start_line = line;

    // consume opening quote
    advance();

    std::string text;
    while (!is_at_end() && peek() != '"') {
        char c = advance();
        if (c == '\\' && !is_at_end()) {
            char escaped = advance();
            if (escaped == 'n')
                text += '\n';
            else if (escaped == 't')
                text += '\t';
            else if (escaped == 'r')
                text += '\r';
            else if (escaped == '"')
                text += '"';
            else if (escaped == '\\')
                text += '\\';
            else
                text += escaped;
            continue;
        }
        text += c;
    }

    // consume closing quote
    if (!is_at_end() && peek() == '"') {
        advance();
    }

    return Token{TokenType::StringLiteral, std::move(text), start_line, start_col};
}

Token Lexer::next_token()
{
    if (!peek_buffer.empty()) {
        Token tok = peek_buffer.front();
        peek_buffer.erase(peek_buffer.begin());
        return tok;
    }

    skip_whitespace_and_comments();

    if (is_at_end()) {
        return Token{TokenType::Eof, "", line, column};
    }

    int  start_col  = column;
    int  start_line = line;
    char c          = peek();

    // Check double brackets [[ and ]]
    if (c == '[' && peek(1) == '[') {
        advance();
        advance();
        return Token{TokenType::DoubleLBracket, "[[", start_line, start_col};
    }

    if (c == ']' && peek(1) == ']') {
        advance();
        advance();
        return Token{TokenType::DoubleRBracket, "]]", start_line, start_col};
    }

    // Check ::
    if (c == ':' && peek(1) == ':') {
        advance();
        advance();
        return Token{TokenType::ColonColon, "::", start_line, start_col};
    }

    // Strings
    if (c == '"') {
        return scan_string();
    }

    // Identifiers & keywords
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        return scan_identifier_or_keyword();
    }

    // Numbers (including negative numbers when followed by digit)
    if (std::isdigit(static_cast<unsigned char>(c)) || ((c == '-' || c == '+') && std::isdigit(static_cast<unsigned char>(peek(1))))) {
        return scan_number();
    }

    // Single character tokens
    advance();
    std::string text(1, c);

    switch (c) {
        case '[':
            return Token{TokenType::LBracket, text, start_line, start_col};
        case ']':
            return Token{TokenType::RBracket, text, start_line, start_col};
        case '(':
            return Token{TokenType::LParen, text, start_line, start_col};
        case ')':
            return Token{TokenType::RParen, text, start_line, start_col};
        case '{':
            return Token{TokenType::LBrace, text, start_line, start_col};
        case '}':
            return Token{TokenType::RBrace, text, start_line, start_col};
        case ':':
            return Token{TokenType::Colon, text, start_line, start_col};
        case ';':
            return Token{TokenType::Semicolon, text, start_line, start_col};
        case ',':
            return Token{TokenType::Comma, text, start_line, start_col};
        case '=':
            return Token{TokenType::Equal, text, start_line, start_col};
        case '&':
            return Token{TokenType::Ampersand, text, start_line, start_col};
        case '<':
            return Token{TokenType::Less, text, start_line, start_col};
        case '>':
            return Token{TokenType::Greater, text, start_line, start_col};
        default:
            return Token{TokenType::Unknown, text, start_line, start_col};
    }
}

Token Lexer::peek_token()
{
    if (peek_buffer.empty()) {
        peek_buffer.push_back(next_token());
    }
    return peek_buffer.front();
}
