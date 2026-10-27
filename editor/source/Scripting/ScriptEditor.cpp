#include <Editor/Scripting/ScriptEditor.hpp>

#include <Editor/UndoStack.hpp>
#include <Editor/Windows/InspectorComponents/ComponentEdit.hpp>
#include <Editor/Windows/InspectorWindow.hpp>
#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Foundation/FileSystem.hpp>
#include <Mariana/Foundation/Logger.hpp>
#include <Mariana/Scripting/ScriptWorld.hpp>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    using InspectorComponents::EditComponent;
    using InspectorComponents::Section;

    // The project's script files (resource paths), rescanned at most once a second.
    const std::vector<std::string>& ProjectScripts(const fs::path& projectDir)
    {
        static std::vector<std::string> files;
        static auto scanned = std::chrono::steady_clock::time_point{};
        const auto now = std::chrono::steady_clock::now();
        if (now - scanned < std::chrono::seconds(1))
            return files;
        scanned = now;
        files.clear();
        std::error_code ec;
        for (auto it = fs::recursive_directory_iterator(projectDir, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
        {
            const fs::path& path = it->path();
            if (it->is_directory(ec) && path.filename().string().rfind('.', 0) == 0)
            {
                it.disable_recursion_pending(); // .mariana and the like
                continue;
            }
            std::string extension = path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (extension == ".luau" || extension == ".lua")
                if (std::string resource = FileSystem::ToResourcePath(path); !resource.empty())
                    files.push_back(resource);
        }
        std::sort(files.begin(), files.end());
        return files;
    }

    std::string ScriptName(const AssetHandle<ScriptAsset>& script)
    {
        if (const std::shared_ptr<ScriptAsset>& asset = script.Get(); asset && !asset->file.empty())
            return asset->file.filename().string();
        const AssetMetadata* meta = script.GetMetadata();
        return meta ? meta->name : std::string("(missing script)");
    }

    ScriptValue* FindValue(std::vector<ScriptValue>& values, const ScriptValue& property)
    {
        for (ScriptValue& v : values)
            if (v.name == property.name && v.type == property.type)
                return &v;
        return nullptr;
    }

    // A property's widget: the entity's value if it has one, else the script's default. True when
    // the value changed (`value` is then what to keep).
    bool DrawProperty(const ScriptValue& property, ScriptValue& value, bool set, bool& reset)
    {
        if (set)
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
        bool changed = false;
        const char* name = property.name.c_str();
        switch (property.type)
        {
        case ScriptValueType::Number: changed = ImGui::DragFloat(name, &value.number, 0.05f); break;
        case ScriptValueType::Bool: changed = ImGui::Checkbox(name, &value.boolean); break;
        case ScriptValueType::String: changed = ImGui::InputText(name, &value.text); break;
        case ScriptValueType::Vector: changed = ImGui::DragFloat3(name, glm::value_ptr(value.vector), 0.05f); break;
        }
        if (set)
            ImGui::PopStyleColor();
        if (ImGui::BeginPopupContextItem("##property"))
        {
            if (ImGui::MenuItem("Reset to default", nullptr, false, set))
                reset = true;
            ImGui::EndPopup();
        }
        if (ImGui::IsItemHovered() && set)
            ImGui::SetTooltip("Set on this entity (right-click to reset to the script's default)");
        return changed;
    }

    // Adds a script file to the entity's scripts (undoable).
    void AddScript(Entity entity, UndoStack& undo, AssetManager2& assets, const fs::path& file)
    {
        AssetHandle<ScriptAsset> script = assets.Load<ScriptAsset>(file);
        if (!script.Get())
        {
            Logger::Error("Could not load the script " + file.string());
            return;
        }
        const bool had = entity.Has<Scripts>();
        Scripts before = had ? *entity.TryGet<Scripts>() : Scripts{};
        Scripts after = before;
        after.scripts.push_back(ScriptSlot{ script, true, {} });
        entity.Add<Scripts>(after);
        undo.Push("Add script " + file.stem().string(),
            [entity, had, before]() mutable {
                if (!entity.IsValid())
                    return;
                if (had)
                    entity.Add<Scripts>(before);
                else
                    entity.Remove<Scripts>();
            },
            [entity, after]() mutable { if (entity.IsValid()) entity.Add<Scripts>(after); });
    }

    void DrawAddScript(Entity entity, InspectorWindow& inspector, UndoStack& undo, AssetManager2& assets, const fs::path& projectDir)
    {
        if (ImGui::Button("Add Script"))
            ImGui::OpenPopup("##add_script");
        ImGui::SameLine();
        ImGui::TextDisabled("or drop a .luau here");
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MARIANA_ASSET_SCRIPT"))
            {
                const fs::path file(static_cast<const char*>(payload->Data));
                inspector.Defer([=, &undo, &assets]() { AddScript(entity, undo, assets, file); });
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopup("##add_script"))
        {
            static std::string newName = "NewScript";
            ImGui::TextDisabled("New script in /Scripts");
            ImGui::SetNextItemWidth(160.0f);
            ImGui::InputText("##new", &newName);
            ImGui::SameLine();
            if (ImGui::Button("Create"))
            {
                const fs::path file = ScriptEditor::CreateScript(projectDir / "Scripts", newName);
                if (!file.empty())
                    inspector.Defer([=, &undo, &assets]() { AddScript(entity, undo, assets, file); });
                ImGui::CloseCurrentPopup();
            }
            const std::vector<std::string>& files = ProjectScripts(projectDir);
            if (!files.empty())
            {
                ImGui::Separator();
                ImGui::TextDisabled("The project's scripts");
            }
            for (const std::string& resource : files)
                if (ImGui::Selectable(resource.c_str()))
                    if (const fs::path file = FileSystem::Resolve(resource); !file.empty())
                        inspector.Defer([=, &undo, &assets]() { AddScript(entity, undo, assets, file); });
            ImGui::EndPopup();
        }
    }
}

namespace ScriptEditor
{
    void RegisterInspector(InspectorWindow& inspector, UndoStack& undo, ScriptWorld& scripts, AssetManager2& assets, fs::path projectDir)
    {
        inspector.SetComponentDrawer("Scripts", [&inspector, &undo, &scripts, &assets, projectDir](Entity& entity) {
            ImGui::PushID("Scripts");
            if (Section<Scripts>(inspector, undo, entity, "Scripts"))
            {
                EditComponent<Scripts>(entity, undo, "Edit Scripts", [&](Scripts& s) {
                    bool changed = false;
                    int remove = -1;
                    for (size_t i = 0; i < s.scripts.size(); ++i)
                    {
                        ScriptSlot& slot = s.scripts[i];
                        ImGui::PushID(static_cast<int>(i));
                        changed |= ImGui::Checkbox("##enabled", &slot.enabled);
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Enabled (a disabled script keeps its state but isn't updated)");
                        ImGui::SameLine();
                        ImGui::TextUnformatted(ScriptName(slot.script).c_str());
                        const std::shared_ptr<ScriptAsset>& asset = slot.script.Get();
#ifndef __EMSCRIPTEN__
                        if (asset && !asset->file.empty())
                        {
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Open"))
                                OpenWithSystem(asset->file);
                        }
#endif
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Remove"))
                            remove = static_cast<int>(i);

                        // Why it doesn't run: it didn't compile, or it stopped with an error.
                        std::string error = asset ? asset->error : std::string("The script file is missing");
                        if (error.empty())
                            error = scripts.GetError(entity, i);
                        if (!error.empty())
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.4f, 1.0f));
                            ImGui::TextWrapped("%s", error.c_str());
                            ImGui::PopStyleColor();
                        }

                        const ScriptInfo& info = scripts.Describe(slot.script);
                        if (!info.error.empty() && (!asset || asset->error.empty()))
                            ImGui::TextDisabled("Properties unknown: %s", info.error.c_str());
                        for (const ScriptValue& property : info.properties)
                        {
                            ImGui::PushID(property.name.c_str());
                            ScriptValue* set = FindValue(slot.values, property);
                            ScriptValue value = set ? *set : property;
                            bool reset = false;
                            if (DrawProperty(property, value, set != nullptr, reset))
                            {
                                if (set)
                                    *set = value;
                                else
                                    slot.values.push_back(value);
                                changed = true;
                            }
                            if (reset)
                            {
                                slot.values.erase(slot.values.begin() + (set - slot.values.data()));
                                changed = true;
                            }
                            ImGui::PopID();
                        }
                        ImGui::Separator();
                        ImGui::PopID();
                    }
                    if (remove >= 0)
                    {
                        s.scripts.erase(s.scripts.begin() + remove);
                        changed = true;
                    }
                    return changed;
                });
                DrawAddScript(entity, inspector, undo, assets, projectDir);
            }
            ImGui::PopID();
        });

        inspector.AddComponentOption({ "Scripting", "Scripts",
            [](Entity e) { return !e.Has<Scripts>(); },
            [&undo](Entity e) {
                e.Add<Scripts>(Scripts{});
                undo.Push("Add Scripts",
                    [e]() mutable { if (e.IsValid()) e.Remove<Scripts>(); },
                    [e]() mutable { if (e.IsValid()) e.Add<Scripts>(Scripts{}); });
            } });
    }

    fs::path CreateScript(const fs::path& folder, const std::string& name)
    {
        // A Luau identifier: letters, digits and underscores, not starting with a digit.
        std::string id;
        for (char c : name)
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
                id += c;
        if (id.empty() || std::isdigit(static_cast<unsigned char>(id[0])))
            id = "Script" + id;

        std::error_code ec;
        fs::create_directories(folder, ec);
        fs::path file = folder / (id + ".luau");
        for (int n = 2; fs::exists(file, ec); ++n)
            file = folder / (id + std::to_string(n) + ".luau");
        const std::string module = file.stem().string();

        std::ofstream out(file, std::ios::binary);
        if (!out)
        {
            Logger::Error("Could not write " + file.string());
            return {};
        }
        out << "-- " << module << ": runs on every entity that has it (scripting/API.md lists what scripts can do).\n"
            << "local " << module << " = {}\n\n"
            << "-- Values the inspector edits per entity (numbers, booleans, strings, vectors), with their defaults.\n"
            << module << ".properties = {\n"
            << "    speed = 1,\n"
            << "}\n\n"
            << "function " << module << ":Start()\n"
            << "end\n\n"
            << "function " << module << ":Update(dt: number)\n"
            << "end\n\n"
            << "return " << module << "\n";
        return file;
    }
}
