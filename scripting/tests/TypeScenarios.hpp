#pragma once
// Scenarios for the scripts' types (MarianaScriptTools): the definitions load, cover what the
// bindings define, the shipped scripts check clean, mistakes are found where they are, and
// autocomplete offers what fits. Run by MarianaScriptingTest and the web build.
#include "Scenarios.hpp"

#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/ScriptTools/ScriptChecker.hpp>
#include <Mariana/ScriptTools/ScriptTypes.hpp>

#include <fstream>
#include <set>
#include <sstream>

namespace ScriptScenarios
{
    namespace detail
    {
        inline std::string ReadResource(const std::string& resource)
        {
            std::ifstream stream(FileSystem::Resolve(resource), std::ios::binary);
            std::ostringstream text;
            text << stream.rdbuf();
            return text.str();
        }

        inline std::string Lines(const std::vector<ScriptDiagnostic>& diagnostics)
        {
            std::string text;
            for (const ScriptDiagnostic& d : diagnostics)
                text += " | " + std::to_string(d.line + 1) + ": " + d.message;
            return text;
        }

        inline bool Offers(const std::vector<ScriptCompletion>& completions, const char* text)
        {
            for (const ScriptCompletion& c : completions)
                if (c.text == text)
                    return true;
            return false;
        }
    }

    // The definitions load into the checker, and every function of the scene, input, physics and
    // quat libraries the bindings define is declared in them.
    inline Result Definitions(const Log& log, const std::string& definitions, ScriptChecker& checker)
    {
        using namespace detail;
        World w;
        Entity e = w.Make("Lister");
        Attach(e, w.Source("Lister.luau", R"(
local L = {}
function L:Start()
    local names = {}
    for _, lib in { { "scene", scene }, { "input", input }, { "physics", physics }, { "quat", quat } } do
        for key in lib[2] do
            table.insert(names, key)
        end
    end
    table.sort(names)
    self.entity.name = table.concat(names, " ")
end
return L
)"));
        w.Frames(1);
        std::istringstream bound(e.GetName());
        std::string name, missing;
        int count = 0;
        while (bound >> name)
        {
            ++count;
            if (definitions.find(" " + name + ":") == std::string::npos)
                missing += " " + name;
        }
        const bool passed = checker.DefinitionErrors().empty() && missing.empty() && count > 40;
        return Finish(log, "definitions",
                      std::to_string(definitions.size()) + " bytes, " + std::to_string(count) + " library members bound" +
                          (missing.empty() ? std::string(", all declared") : ", not declared:" + missing) +
                          (checker.DefinitionErrors().empty() ? "" : " | " + checker.DefinitionErrors()),
                      passed);
    }

    // The scripts that come with the library and the Sandbox's check without a problem.
    inline Result ShippedScripts(const Log& log, ScriptChecker& checker)
    {
        using namespace detail;
        std::string problems;
        int checked = 0;
        for (const char* script : { "/Scripts/VehicleDriver.luau", "/Scripts/CharacterDriver.luau", "/Scripts/ChaseCamera.luau",
                                    "/Scripts/Respawn.luau", "/Scripts/LaunchPad.luau", "/Scripts/Hover.luau" })
        {
            const std::string source = ReadResource(script);
            if (source.empty())
            {
                problems += std::string(" | no ") + script;
                continue;
            }
            ++checked;
            const std::vector<ScriptDiagnostic> diagnostics = checker.Check(script, source);
            if (!diagnostics.empty())
                problems += std::string(" | ") + script + Lines(diagnostics);
        }
        return Finish(log, "shipped scripts", std::to_string(checked) + " checked" + (problems.empty() ? ", no problems" : problems),
                      checked == 6 && problems.empty());
    }

    // A script with mistakes gets a problem on each line that has one, and only there.
    inline Result Mistakes(const Log& log, ScriptChecker& checker)
    {
        using namespace detail;
        const char* source = R"(--!strict
local Bad = {} :: Script
function Bad.Update(self: Script, dt: number)
    self.entity:Rotat(vec3(0, 1, 0), 90)
    local ground = scene.Fnd("Ground")
    local wrong = self.entity:Get("RigidBdy")
    local body = self.entity:Get("RigidBody")
    if body then
        body.masss = 5
        body.motion = "Floating"
    end
    physics.AddForce(self.entity, 5)
    self.entity:Rotate(vec3(0, 1, 0), 90 * dt)
end
return Bad
)";
        const std::vector<ScriptDiagnostic> diagnostics = checker.Check("Bad.luau", source);
        std::set<int> lines;
        for (const ScriptDiagnostic& d : diagnostics)
            lines.insert(d.line + 1);
        const std::set<int> expected = { 4, 5, 6, 9, 10, 12 };
        return Finish(log, "mistakes", std::to_string(diagnostics.size()) + " problems" + Lines(diagnostics), lines == expected);
    }

    // Autocomplete: an entity's methods after ':', component names inside Get("..."), a component's
    // fields after '.'.
    inline Result Completion(const Log& log, ScriptChecker& checker)
    {
        using namespace detail;
        const std::string head = "--!strict\nlocal C = {} :: Script\nfunction C.Update(self: Script, dt: number)\n";
        const auto at = [&](const std::string& line) {
            return checker.Complete("Complete.luau", head + line + "\nend\nreturn C\n", 3, static_cast<int>(line.size()));
        };
        const std::vector<ScriptCompletion> methods = at("    self.entity:");
        const std::vector<ScriptCompletion> components = at("    self.entity:Get(\"");
        const std::vector<ScriptCompletion> fields = checker.Complete("Complete.luau",
            head + "    local body = self.entity:Get(\"RigidBody\")\n    if body then\n        body.\n    end\nend\nreturn C\n", 5, 13);
        const bool passed = Offers(methods, "Rotate") && Offers(methods, "GetScript") && Offers(methods, "LookAt") && !Offers(methods, "position") &&
                            Offers(components, "RigidBody") && Offers(components, "Vehicle") && Offers(fields, "mass") && Offers(fields, "motion");
        std::string offered;
        if (!passed)
            for (const auto* list : { &methods, &components, &fields })
            {
                offered += " |";
                for (const ScriptCompletion& c : *list)
                    offered += " " + c.text;
            }
        return Finish(log, "completion",
                      std::to_string(methods.size()) + " after ':', " + std::to_string(components.size()) + " component names, " +
                          std::to_string(fields.size()) + " RigidBody fields" + offered,
                      passed);
    }

    inline std::vector<Result> RunTypes(const Log& log)
    {
        const std::string definitions = ScriptTypes::Definitions();
        ScriptChecker checker(definitions);
        return { Definitions(log, definitions, checker), ShippedScripts(log, checker), Mistakes(log, checker), Completion(log, checker) };
    }
}
