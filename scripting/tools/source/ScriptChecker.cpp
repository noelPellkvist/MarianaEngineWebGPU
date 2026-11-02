#include <Mariana/ScriptTools/ScriptChecker.hpp>
#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Scripting/Script.hpp>

#include "Luau/Autocomplete.h"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/Error.h"
#include "Luau/Frontend.h"
#include "Luau/ToString.h"
#include "Luau/TypeArena.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace
{
    // The scripts' text, by name (a resource path, or a file name for one in /Scripts), as the checker
    // reads it: what the caller checks, else the file a require names, read from disk.
    struct Sources final : Luau::FileResolver
    {
        std::unordered_map<std::string, std::string> text;
        std::unordered_set<std::string> fromDisk; // read again before each check

        std::optional<Luau::SourceCode> readSource(const Luau::ModuleName& name) override
        {
            if (const auto it = text.find(name); it != text.end())
                return Luau::SourceCode{ it->second, Luau::SourceCode::Module };
            const std::filesystem::path file = name.rfind('/', 0) == 0 ? FileSystem::Resolve(name) : std::filesystem::path();
            std::ifstream stream(file, std::ios::binary);
            if (file.empty() || !stream)
                return std::nullopt;
            std::ostringstream source;
            source << stream.rdbuf();
            fromDisk.insert(name);
            return Luau::SourceCode{ source.str(), Luau::SourceCode::Module };
        }

        // require("./Util") as the game resolves it (ScriptAsset::ResolveRequire).
        std::optional<Luau::ModuleInfo> resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* expr, const Luau::TypeCheckLimits&) override
        {
            const Luau::AstExprConstantString* string = expr ? expr->as<Luau::AstExprConstantString>() : nullptr;
            if (!string)
                return std::nullopt;
            const std::string resolved = ScriptAsset::ResolveRequire(std::string(string->value.data, string->value.size), context ? context->name : std::string());
            if (resolved.empty())
                return std::nullopt;
            return Luau::ModuleInfo{ resolved };
        }
    };

    ScriptDiagnostic Diagnostic(const Luau::TypeError& error, Luau::FileResolver* files)
    {
        ScriptDiagnostic d;
        d.line = static_cast<int>(error.location.begin.line);
        d.column = static_cast<int>(error.location.begin.column);
        d.endLine = static_cast<int>(error.location.end.line);
        d.endColumn = static_cast<int>(error.location.end.column);
        if (const Luau::SyntaxError* syntax = Luau::get_if<Luau::SyntaxError>(&error.data))
        {
            d.message = syntax->message;
            d.syntax = true;
        }
        else
            d.message = Luau::toString(error, Luau::TypeErrorToStringOptions{ files });
        return d;
    }
}

struct ScriptChecker::Impl
{
    Sources sources;
    Luau::NullConfigResolver config;
    std::unique_ptr<Luau::Frontend> frontend;
    std::string definitionErrors;

    explicit Impl(const std::string& definitions)
    {
        config.defaultConfig.mode = Luau::Mode::Nonstrict;
        Luau::FrontendOptions options;
        options.retainFullTypeGraphs = true; // autocomplete reads the types at a position
        options.runLintChecks = false;
        // The solver luau-lsp uses unless told otherwise, so VS Code and the editor agree; it types
        // `self` in `function T:Update()` as T.
        frontend = std::make_unique<Luau::Frontend>(Luau::SolverMode::Old, &sources, &config, options);
        // The globals for checking and those for autocomplete (checked strictly for precise types).
        for (const bool forAutocomplete : { false, true })
        {
            Luau::GlobalTypes& globals = forAutocomplete ? frontend->globalsForAutocomplete : frontend->globals;
            Luau::registerBuiltinGlobals(*frontend, globals, forAutocomplete);
            const Luau::LoadDefinitionFileResult loaded =
                frontend->loadDefinitionFile(globals, globals.globalScope, definitions, "@mariana", false, forAutocomplete);
            if (!loaded.success && definitionErrors.empty())
            {
                for (const Luau::ParseError& error : loaded.parseResult.errors)
                    definitionErrors += "line " + std::to_string(error.getLocation().begin.line + 1) + ": " + error.getMessage() + "\n";
                if (loaded.module)
                    for (const Luau::TypeError& error : loaded.module->errors)
                        definitionErrors += "line " + std::to_string(error.location.begin.line + 1) + ": " + Luau::toString(error) + "\n";
                if (definitionErrors.empty())
                    definitionErrors = "the definitions didn't load";
            }
            Luau::freeze(globals.globalTypes);
        }
    }

    Luau::CheckResult Check(const std::string& name, const std::string& source, bool forAutocomplete = false)
    {
        sources.text[name] = source;
        frontend->markDirty(name);
        // Required scripts may have changed on disk since.
        for (const std::string& required : sources.fromDisk)
            frontend->markDirty(required);
        Luau::FrontendOptions options = frontend->options;
        options.forAutocomplete = forAutocomplete;
        return frontend->check(name, options);
    }
};

ScriptChecker::ScriptChecker(const std::string& definitions)
    : m_Impl(std::make_unique<Impl>(definitions))
{
}

ScriptChecker::~ScriptChecker() = default;

const std::string& ScriptChecker::DefinitionErrors() const
{
    return m_Impl->definitionErrors;
}

std::vector<ScriptDiagnostic> ScriptChecker::Check(const std::string& name, const std::string& source)
{
    std::vector<ScriptDiagnostic> diagnostics;
    try
    {
        const Luau::CheckResult result = m_Impl->Check(name, source);
        for (const Luau::TypeError& error : result.errors)
            diagnostics.push_back(Diagnostic(error, &m_Impl->sources));
    }
    catch (const std::exception& e)
    {
        // The checker gave up (a time or size limit): say so rather than nothing.
        ScriptDiagnostic d;
        d.message = std::string("Couldn't check it: ") + e.what();
        diagnostics.push_back(d);
    }
    std::sort(diagnostics.begin(), diagnostics.end(), [](const ScriptDiagnostic& a, const ScriptDiagnostic& b) {
        return a.line != b.line ? a.line < b.line : a.column < b.column;
    });
    return diagnostics;
}

std::vector<ScriptCompletion> ScriptChecker::Complete(const std::string& name, const std::string& source, int line, int column)
{
    std::vector<ScriptCompletion> completions;
    try
    {
        m_Impl->Check(name, source, true);
        const Luau::AutocompleteResult result = Luau::autocomplete(*m_Impl->frontend, name,
            Luau::Position{ static_cast<unsigned>(line), static_cast<unsigned>(column) },
            [](std::string, std::optional<const Luau::ExternType*>, std::optional<std::string>) { return std::nullopt; });
        // Inside a string only the strings that fit (a component's name in entity:Get("...")): when
        // there are any, the rest is what an unclosed string leaves the parser guessing at.
        const bool inString = result.context == Luau::AutocompleteContext::String ||
                              std::any_of(result.entryMap.begin(), result.entryMap.end(),
                                          [](const auto& e) { return e.second.kind == Luau::AutocompleteEntryKind::String; });
        for (const auto& [text, entry] : result.entryMap)
        {
            // Fields offered after ':' (or methods after '.') don't fit there.
            if (entry.wrongIndexType || entry.deprecated)
                continue;
            if (inString && entry.kind != Luau::AutocompleteEntryKind::String)
                continue;
            ScriptCompletion c;
            c.text = text;
            // Strings come quoted; what is typed is inside the quotes.
            if (entry.kind == Luau::AutocompleteEntryKind::String && c.text.size() >= 2 && c.text.front() == '"' && c.text.back() == '"')
                c.text = c.text.substr(1, c.text.size() - 2);
            if (entry.type)
                c.detail = Luau::toString(*entry.type);
            switch (entry.kind)
            {
            case Luau::AutocompleteEntryKind::Property:
            {
                // A function, or overloads of one (an intersection of functions).
                const Luau::TypeId type = entry.type ? Luau::follow(*entry.type) : nullptr;
                const bool function = type && (Luau::get<Luau::FunctionType>(type) || Luau::get<Luau::IntersectionType>(type));
                c.kind = function ? ScriptCompletion::Kind::Method : ScriptCompletion::Kind::Property;
                break;
            }
            case Luau::AutocompleteEntryKind::Keyword: c.kind = ScriptCompletion::Kind::Keyword; break;
            case Luau::AutocompleteEntryKind::String: c.kind = ScriptCompletion::Kind::String; break;
            case Luau::AutocompleteEntryKind::Type: c.kind = ScriptCompletion::Kind::Type; break;
            default: c.kind = ScriptCompletion::Kind::Variable; break;
            }
            completions.push_back(std::move(c));
        }
    }
    catch (const std::exception&)
    {
        return {};
    }
    std::sort(completions.begin(), completions.end(), [](const ScriptCompletion& a, const ScriptCompletion& b) { return a.text < b.text; });
    return completions;
}
