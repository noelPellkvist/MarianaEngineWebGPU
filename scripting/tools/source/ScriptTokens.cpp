#include <Mariana/ScriptTools/ScriptTokens.hpp>

#include "Luau/Lexer.h"

#include <cstring>
#include <string_view>

namespace
{
    bool IsLibrary(std::string_view name)
    {
        static const char* const kNames[] = {
            "scene", "input", "time", "physics", "vec3", "quat", "vector", "math", "string", "table", "buffer", "bit32",
            "coroutine", "utf8", "os", "print", "warn", "error", "assert", "pcall", "xpcall", "pairs", "ipairs", "next",
            "select", "tostring", "tonumber", "type", "typeof", "setmetatable", "getmetatable", "rawget", "rawset",
            "Script", "Entity", "Component",
        };
        for (const char* n : kNames)
            if (name == n)
                return true;
        return false;
    }
}

std::vector<ScriptToken> ScriptTokens(const std::string& source)
{
    std::vector<ScriptToken> tokens;
    Luau::Allocator allocator;
    Luau::AstNameTable names(allocator);
    Luau::Lexer lexer(source.data(), source.size(), names);
    lexer.setSkipComments(false);
    lexer.setReadNames(true);
    for (;;)
    {
        const Luau::Lexeme& lexeme = lexer.next();
        if (lexeme.type == Luau::Lexeme::Eof)
            break;
        ScriptToken token;
        token.line = static_cast<int>(lexeme.location.begin.line);
        token.column = static_cast<int>(lexeme.location.begin.column);
        token.endLine = static_cast<int>(lexeme.location.end.line);
        token.endColumn = static_cast<int>(lexeme.location.end.column);
        switch (lexeme.type)
        {
        case Luau::Lexeme::Number: token.kind = ScriptToken::Kind::Number; break;
        case Luau::Lexeme::QuotedString:
        case Luau::Lexeme::RawString:
        case Luau::Lexeme::InterpStringBegin:
        case Luau::Lexeme::InterpStringMid:
        case Luau::Lexeme::InterpStringEnd:
        case Luau::Lexeme::InterpStringSimple:
        case Luau::Lexeme::BrokenString: token.kind = ScriptToken::Kind::String; break;
        case Luau::Lexeme::Comment:
        case Luau::Lexeme::BlockComment:
        case Luau::Lexeme::BrokenComment: token.kind = ScriptToken::Kind::Comment; break;
        case Luau::Lexeme::Attribute: token.kind = ScriptToken::Kind::Keyword; break;
        case Luau::Lexeme::Name:
        {
            const std::string_view name = lexeme.name ? std::string_view(lexeme.name) : std::string_view();
            token.kind = name == "self" ? ScriptToken::Kind::Keyword : IsLibrary(name) ? ScriptToken::Kind::Library : ScriptToken::Kind::Name;
            break;
        }
        default:
            token.kind = lexeme.type >= Luau::Lexeme::Reserved_BEGIN && lexeme.type < Luau::Lexeme::Reserved_END ? ScriptToken::Kind::Keyword
                                                                                                                 : ScriptToken::Kind::Operator;
            break;
        }
        tokens.push_back(token);
    }
    return tokens;
}
