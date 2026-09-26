#include "Parser.h"
#include <iostream>

using namespace lyra::reflect;

static bool ends_with(const std::string& str, const std::string& suffix)
{
    if (str.size() < suffix.size()) {
        return false;
    }
    return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

Parser::Parser(Lexer& lexer)
    : lexer(lexer)
{
    current = lexer.next_token();
}

bool Parser::fast_check(std::string_view source)
{
    return source.find("lyra::") != std::string_view::npos ||
           source.find("using lyra:") != std::string_view::npos;
}

Token Parser::consume(TokenType expected, const std::string& error_msg)
{
    if (current.type == expected) {
        Token prev = current;
        current    = lexer.next_token();
        return prev;
    }

    std::cerr << lexer.get_filename() << ":" << current.line << ":" << current.column
              << ": error: " << (error_msg.empty() ? "Unexpected token" : error_msg)
              << " (got '" << current.text << "')" << std::endl;
    return current;
}

bool Parser::match(TokenType type)
{
    if (current.type != type) {
        return false;
    }
    current = lexer.next_token();
    return true;
}

bool Parser::check(TokenType type)
{
    return current.type == type;
}

std::vector<Attribute> Parser::parse_attribute_list()
{
    std::vector<Attribute> attrs;
    if (!match(TokenType::DoubleLBracket)) {
        return attrs;
    }

    std::string prefix_scope;

    // Check for "using lyra:"
    if (check(TokenType::KeywordUsing)) {
        consume(TokenType::KeywordUsing);
        if (check(TokenType::Identifier)) {
            prefix_scope = consume(TokenType::Identifier).text;
        }
        consume(TokenType::Colon, "Expected ':' after using scope in attribute");
    }

    while (!check(TokenType::DoubleRBracket) && !check(TokenType::Eof)) {
        parse_attribute(attrs, prefix_scope);
        if (!match(TokenType::Comma)) {
            break;
        }
    }

    consume(TokenType::DoubleRBracket, "Expected ']]' at end of attribute list");
    return attrs;
}

void Parser::parse_attribute(std::vector<Attribute>& out_attrs, const std::string& prefix_scope)
{
    Attribute attr;

    // Namespace prefix (e.g. lyra::)
    std::string name;
    if (check(TokenType::Identifier)) {
        name = consume(TokenType::Identifier).text;
        if (match(TokenType::ColonColon)) {
            // This was the namespace prefix, next is the attribute name
            if (check(TokenType::Identifier)) {
                name = consume(TokenType::Identifier).text;
            }
        }
    }

    attr.name = name;

    // Argument list: ( arg1, arg2, key = val, ... )
    if (match(TokenType::LParen)) {
        while (!check(TokenType::RParen) && !check(TokenType::Eof)) {
            std::string arg_name;
            std::string arg_val;

            if (check(TokenType::Identifier) && lexer.peek_token().type == TokenType::Equal) {
                arg_name = consume(TokenType::Identifier).text;
                consume(TokenType::Equal);

                if (check(TokenType::Identifier) || check(TokenType::StringLiteral) || check(TokenType::NumberLiteral)) {
                    arg_val = current.text;
                    consume(current.type);
                }
                attr.named_args[arg_name] = arg_val;
            } else {
                if (check(TokenType::Identifier) || check(TokenType::StringLiteral) || check(TokenType::NumberLiteral)) {
                    arg_val = current.text;
                    consume(current.type);
                    attr.positional_args.push_back(arg_val);
                } else {
                    consume(current.type);
                }
            }

            if (!match(TokenType::Comma)) {
                break;
            }
        }
        consume(TokenType::RParen, "Expected ')' at end of attribute arguments");
    }

    out_attrs.push_back(std::move(attr));
}

void Parser::parse_field(ComponentMeta& comp)
{
    std::vector<Attribute> attrs;
    if (check(TokenType::DoubleLBracket)) {
        attrs = parse_attribute_list();
    }

    FieldMeta field;
    field.line = current.line;

    // Parse type (could be multiple tokens like unsigned int or const float or lyra::Vector3)
    std::string field_type;
    while (!check(TokenType::Semicolon) && !check(TokenType::Equal) && !check(TokenType::Eof)) {
        if (lexer.peek_token().type == TokenType::Semicolon || lexer.peek_token().type == TokenType::Equal) {
            // The last identifier is the field name!
            field.name = current.text;
            consume(current.type);
            break;
        }

        if (!field_type.empty() && current.type != TokenType::ColonColon && field_type.back() != ':') {
            field_type += " ";
        }
        field_type += current.text;
        consume(current.type);
    }

    field.type = field_type;

    // Parse optional default value
    if (match(TokenType::Equal)) {
        std::string def_val;
        int         paren_depth = 0;
        int         brace_depth = 0;

        while (!check(TokenType::Eof)) {
            if (check(TokenType::LParen))
                paren_depth++;
            else if (check(TokenType::RParen))
                paren_depth--;
            else if (check(TokenType::LBrace))
                brace_depth++;
            else if (check(TokenType::RBrace))
                brace_depth--;
            else if (check(TokenType::Semicolon) && paren_depth == 0 && brace_depth == 0) {
                break;
            }

            if (!def_val.empty() && current.type != TokenType::ColonColon && def_val.back() != ':') {
                def_val += " ";
            }
            def_val += current.text;
            consume(current.type);
        }
        field.default_value = def_val;
    }

    consume(TokenType::Semicolon, "Expected ';' after field declaration");

    // Process attributes
    for (const auto& a : attrs) {
        if (a.name == "hidden") {
            field.hidden = true;
            continue;
        }
        if (a.name == "edit") {
            field.drawer         = a.get_pos(0);
            field.drawer_options = a.get_arg("order");
            continue;
        }
        if (a.name == "range") {
            field.min_val = a.get_pos(0);
            field.max_val = a.get_pos(1);
            continue;
        }
        if (a.name == "speed") {
            field.speed = a.get_pos(0);
            continue;
        }
        if (a.name == "reset") {
            field.reset_val = a.get_pos(0);
            continue;
        }
        if (a.name == "tooltip") {
            field.tooltip = a.get_pos(0);
            continue;
        }
        if (a.name == "label") {
            field.label = a.get_pos(0);
            continue;
        }
    }

    // Default drawer deduction if none specified
    if (field.drawer.empty()) {
        if (ends_with(field.type, "bool")) {
            field.drawer = "checkbox";
        } else if (ends_with(field.type, "float") || ends_with(field.type, "double") ||
                   ends_with(field.type, "int") || ends_with(field.type, "uint") ||
                   ends_with(field.type, "uint32_t") || ends_with(field.type, "int32_t")) {
            field.drawer = "number";
        } else if (ends_with(field.type, "Vector2") || ends_with(field.type, "Vector3") || ends_with(field.type, "Vector4")) {
            field.drawer = "vector";
        } else if (ends_with(field.type, "Quaternion")) {
            field.drawer = "euler";
            if (field.drawer_options.empty()) {
                field.drawer_options = "YXZ";
            }
        } else if (ends_with(field.type, "SceneNode")) {
            field.drawer = "node";
        }
    }

    comp.fields.push_back(std::move(field));
}

void Parser::parse_struct(const std::vector<Attribute>& attrs, ModuleReflection& out_module)
{
    ComponentMeta comp;
    comp.header_file = lexer.get_filename();
    comp.line        = current.line;

    // Apply component attributes
    for (const auto& a : attrs) {
        if (a.name == "component") {
            std::string cat = a.get_pos(0);
            if (!cat.empty()) {
                comp.category = cat;
            }
        } else if (a.name == "category") {
            comp.category = a.get_pos(0);
        } else if (a.name == "tooltip") {
            comp.tooltip = a.get_pos(0);
        }
    }

    // Struct name
    if (check(TokenType::Identifier)) {
        comp.name = consume(TokenType::Identifier).text;
    }

    if (!match(TokenType::LBrace)) {
        return;
    }

    while (!check(TokenType::RBrace) && !check(TokenType::Eof)) {
        // Skip access specifiers (public:, private:, etc.)
        if (check(TokenType::Identifier) && (current.text == "public" || current.text == "private" || current.text == "protected")) {
            consume(TokenType::Identifier);
            match(TokenType::Colon);
            continue;
        }

        parse_field(comp);
    }

    consume(TokenType::RBrace, "Expected '}' at end of struct body");
    match(TokenType::Semicolon);

    out_module.components.push_back(std::move(comp));
}

ParamMeta Parser::parse_parameter()
{
    ParamMeta   param;
    std::string raw;

    if (check(TokenType::KeywordConst)) {
        param.is_const = true;
        raw += "const ";
        consume(TokenType::KeywordConst);
    }

    std::string type_name;
    while (check(TokenType::Identifier) || check(TokenType::ColonColon)) {
        type_name += current.text;
        consume(current.type);
    }
    raw += type_name;

    // Check for Query<T1, T2>
    if ((ends_with(type_name, "Query")) && match(TokenType::Less)) {
        param.is_query = true;
        raw += "<";
        while (!check(TokenType::Greater) && !check(TokenType::Eof)) {
            ParamMeta sub;
            if (check(TokenType::KeywordConst)) {
                sub.is_const = true;
                consume(TokenType::KeywordConst);
            }
            std::string sub_type;
            while (check(TokenType::Identifier) || check(TokenType::ColonColon)) {
                sub_type += current.text;
                consume(current.type);
            }
            sub.type = sub_type;
            param.query_subtypes.push_back(sub);
            if (!match(TokenType::Comma)) {
                break;
            }
        }
        consume(TokenType::Greater, "Expected '>' after Query template args");
        raw += ">";
    }

    if (match(TokenType::Ampersand)) {
        param.is_ref = true;
        raw += "&";
    }

    param.type     = type_name;
    param.raw_type = raw;

    // Consume parameter variable name
    if (check(TokenType::Identifier)) {
        consume(TokenType::Identifier);
    }

    return param;
}

void Parser::parse_function(const std::vector<Attribute>& attrs, ModuleReflection& out_module)
{
    SystemMeta sys;
    sys.header_file = lexer.get_filename();
    sys.line        = current.line;

    for (const auto& a : attrs) {
        if (a.name != "system") {
            continue;
        }
        std::string stage = a.get_pos(0, "UPDATE");
        if (!stage.empty()) {
            sys.stage = stage;
        }
        sys.group = a.get_arg("group");
        if (a.has_arg("run_in_editor") || a.get_pos(1) == "run_in_editor") {
            sys.run_in_editor = true;
        }
    }

    // Skip any specifiers (inline, static, FORCE_INLINE, constexpr, etc.)
    while (check(TokenType::Identifier) || check(TokenType::KeywordAuto) || check(TokenType::KeywordVoid)) {
        if (current.text == "inline" || current.text == "static" || current.text == "constexpr" ||
            current.text == "FORCE_INLINE" || current.text == "virtual") {
            consume(current.type);
            continue;
        }
        break;
    }

    // Return type (void or auto or identifier)
    if (check(TokenType::KeywordVoid)) {
        consume(TokenType::KeywordVoid);
    } else if (check(TokenType::KeywordAuto)) {
        consume(TokenType::KeywordAuto);
    } else if (check(TokenType::Identifier)) {
        consume(TokenType::Identifier);
    }

    // Function name
    if (check(TokenType::Identifier)) {
        sys.name = consume(TokenType::Identifier).text;
    }

    // Parameters
    if (match(TokenType::LParen)) {
        while (!check(TokenType::RParen) && !check(TokenType::Eof)) {
            ParamMeta p = parse_parameter();
            sys.params.push_back(p);
            if (!match(TokenType::Comma)) {
                break;
            }
        }
        consume(TokenType::RParen, "Expected ')' after system parameter list");
    }

    // Skip function body { ... }
    if (match(TokenType::LBrace)) {
        int depth = 1;
        while (depth > 0 && !check(TokenType::Eof)) {
            if (check(TokenType::LBrace)) {
                depth++;
            } else if (check(TokenType::RBrace)) {
                depth--;
            }
            consume(current.type);
        }
    } else {
        match(TokenType::Semicolon);
    }

    out_module.systems.push_back(std::move(sys));
}

bool Parser::parse(ModuleReflection& out_module)
{
    while (!check(TokenType::Eof)) {
        std::vector<Attribute> attrs;

        // Check for attributes in front of struct or function
        if (check(TokenType::DoubleLBracket)) {
            attrs = parse_attribute_list();
        }

        // Check if next is struct
        if (check(TokenType::KeywordStruct) || check(TokenType::KeywordClass)) {
            consume(current.type);

            // Attributes placed after struct keyword: struct [[lyra::component]] Name
            if (check(TokenType::DoubleLBracket)) {
                std::vector<Attribute> inline_attrs = parse_attribute_list();
                attrs.insert(attrs.end(), inline_attrs.begin(), inline_attrs.end());
            }

            bool is_comp = false;
            for (const auto& a : attrs) {
                if (a.name == "component") {
                    is_comp = true;
                    break;
                }
            }

            if (is_comp) {
                parse_struct(attrs, out_module);
                continue;
            }
        }

        // Check if next is a system function
        bool is_sys = false;
        for (const auto& a : attrs) {
            if (a.name == "system") {
                is_sys = true;
                break;
            }
        }

        if (is_sys) {
            parse_function(attrs, out_module);
            continue;
        }

        // Advance to next token if not matched
        consume(current.type);
    }

    return true;
}
