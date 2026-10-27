#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// A problem Luau's checker found in a script: where (lines and columns count from 0) and what.
struct ScriptDiagnostic
{
    int line = 0;
    int column = 0;
    int endLine = 0;
    int endColumn = 0;
    std::string message;
    bool syntax = false; // the script doesn't parse (else a type error)
};

// What could be typed at a place in a script.
struct ScriptCompletion
{
    enum class Kind : uint8_t { Property, Method, Variable, Keyword, String, Type };
    std::string text;   // what to insert
    std::string detail; // its type
    Kind kind = Kind::Variable;
};

// Luau's type checker with the Mariana API's types (ScriptTypes::Definitions): checks scripts and
// completes code from their text as it is, so a script being edited is checked unsaved. Scripts are
// checked nonstrict unless they start with --!strict.
class ScriptChecker
{
public:
    explicit ScriptChecker(const std::string& definitions);
    ~ScriptChecker();
    ScriptChecker(const ScriptChecker&) = delete;
    ScriptChecker& operator=(const ScriptChecker&) = delete;

    // What was wrong with the definitions (empty when they loaded).
    const std::string& DefinitionErrors() const;

    // `name` identifies the script (its file name); its problems, in order.
    std::vector<ScriptDiagnostic> Check(const std::string& name, const std::string& source);
    // What fits at a position (sorted by text).
    std::vector<ScriptCompletion> Complete(const std::string& name, const std::string& source, int line, int column);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
