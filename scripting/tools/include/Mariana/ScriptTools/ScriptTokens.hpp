#pragma once
#include <cstdint>
#include <string>
#include <vector>

// A piece of a script as Luau's lexer reads it, for colouring the text: where (lines and columns
// count from 0; the end is past it, and a long string or comment can span lines) and what it is.
struct ScriptToken
{
    enum class Kind : uint8_t { Keyword, Name, Library, Number, String, Comment, Operator };
    int line = 0;
    int column = 0;
    int endLine = 0;
    int endColumn = 0;
    Kind kind = Kind::Operator;
};

// The tokens of a script, in order (what isn't one is white space). Library = the API's globals and
// Luau's common ones (scene, physics, vec3, math, print, ...); `self` counts as a keyword.
std::vector<ScriptToken> ScriptTokens(const std::string& source);
