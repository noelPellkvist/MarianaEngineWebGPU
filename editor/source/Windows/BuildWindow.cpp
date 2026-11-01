#include <Editor/Windows/BuildWindow.hpp>
#include <Editor/Scripting/ScriptEditor.hpp>
#include <Mariana/Foundation/Logger.hpp>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <set>

namespace fs = std::filesystem;

namespace
{
    // Resource paths of a project folder's files with an extension ("/Scenes/A.scene").
    std::vector<std::string> List(const fs::path& project, const char* folder, const char* extension)
    {
        std::set<std::string> found;
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(project / folder, ec))
            if (entry.path().extension() == extension)
                found.insert(std::string("/") + folder + "/" + entry.path().filename().string());
        return { found.begin(), found.end() };
    }

    // A path as a text field edits it.
    bool PathField(const char* label, fs::path& path)
    {
        std::string text = path.generic_string();
        if (!ImGui::InputText(label, &text))
            return false;
        path = text;
        return true;
    }
}

BuildWindow::BuildWindow(GUI& gui, fs::path project)
    : EditorWindow("Build", gui, false), m_Project(std::move(project))
{
    std::string error;
    if (!BuildProfiles::Load(m_Project, m_Profiles, error))
    {
        Logger::Warning(error);
        m_Profiles = BuildProfiles::Defaults(m_Project);
    }
    m_SavedJson = m_Profiles.ToJson();
    for (size_t i = 0; i < m_Profiles.profiles.size(); ++i)
        if (m_Profiles.profiles[i].name == m_Profiles.active)
            m_Selected = static_cast<int>(i);
    m_Tools = BuildTools::Load(m_Project);
}

BuildWindow::~BuildWindow() = default;

bool BuildWindow::CanBuild()
{
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
    return true;
#else
    return false;
#endif
}

void BuildWindow::Refresh()
{
    m_Scenes = List(m_Project, "Scenes", ".scene");
    m_Pipelines = List(m_Project, "Pipelines", ".pipeline");
}

void BuildWindow::Save()
{
    const std::string json = m_Profiles.ToJson();
    if (json == m_SavedJson)
        return;
    std::string error;
    if (m_Profiles.Save(m_Project, error))
        m_SavedJson = json;
    else
        m_Status = error;
}

void BuildWindow::SaveTools()
{
    std::string error;
    if (!m_Tools.Save(m_Project, error))
        m_Status = error;
}

void BuildWindow::Build(bool run)
{
    if (!CanBuild() || IsBuilding() || m_Profiles.profiles.empty())
        return;
    m_Selected = std::clamp(m_Selected, 0, static_cast<int>(m_Profiles.profiles.size()) - 1);
    Save();
    m_open = true;
    m_Log.clear();
    m_Status.clear();
    m_RunWhenBuilt = run;
    m_Exporter = std::make_unique<GameExporter>(m_Project, m_Profiles.profiles[m_Selected], m_Tools);
    m_Exporter->Start();
}

void BuildWindow::Poll()
{
    if (!m_Exporter)
        return;
    std::vector<GameExporter::Line> lines = m_Exporter->TakeLines();
    if (!lines.empty())
    {
        m_Log.insert(m_Log.end(), std::make_move_iterator(lines.begin()), std::make_move_iterator(lines.end()));
        // A long build's warnings: the newest are what matter.
        if (m_Log.size() > 20000)
            m_Log.erase(m_Log.begin(), m_Log.begin() + static_cast<std::ptrdiff_t>(m_Log.size() - 20000));
        m_ScrollLog = true;
    }
    if (!m_Exporter->IsDone() || !m_RunWhenBuilt)
        return;
    m_RunWhenBuilt = false;
    if (!m_Exporter->Succeeded())
        return;
    std::string error;
    if (m_Runner.Start(m_Exporter->Profile(), m_Exporter->Output(), m_Tools, error))
        m_Status = m_Exporter->Profile().platform == BuildPlatform::Web ? "Serving the game at " + m_Runner.Address() : "Started " + m_Exporter->Launchable().filename().string();
    else
        m_Status = error;
}

void BuildWindow::Draw()
{
    Poll(); // also while closed: Build from the menu runs without the window
    if (!m_open)
    {
        m_WasOpen = false;
        return;
    }
    if (!m_WasOpen)
        Refresh();
    m_WasOpen = true;

    ImGui::SetNextWindowSize(ImVec2(900.0f, 620.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(m_name.c_str(), &m_open))
    {
        ImGui::BeginChild("##profiles", ImVec2(200.0f, ImGui::GetContentRegionAvail().y * 0.55f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
        DrawProfiles();
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##profile", ImVec2(0.0f, ImGui::GetContentRegionAvail().y * 0.55f), ImGuiChildFlags_Borders);
        if (m_Selected >= 0 && m_Selected < static_cast<int>(m_Profiles.profiles.size()))
            DrawProfile(m_Profiles.profiles[m_Selected]);
        else
            ImGui::TextDisabled("No profile: add one on the left");
        ImGui::EndChild();
        DrawBuild();
    }
    ImGui::End();
    Save();
}

void BuildWindow::DrawProfiles()
{
    std::vector<BuildProfile>& profiles = m_Profiles.profiles;
    for (int i = 0; i < static_cast<int>(profiles.size()); ++i)
    {
        const BuildProfile& p = profiles[i];
        const bool active = p.name == m_Profiles.active;
        const std::string label = p.name + (active ? "  (active)" : "") + "##" + std::to_string(i);
        if (ImGui::Selectable(label.c_str(), m_Selected == i))
            m_Selected = i;
        ImGui::SetItemTooltip("%s, %s", BuildProfile::PlatformName(p.platform), BuildProfile::ConfigurationName(p.configuration));
    }
    ImGui::Separator();
    auto add = [&](BuildPlatform platform) {
        BuildProfile p = profiles.empty() ? BuildProfile{} : profiles[std::clamp(m_Selected, 0, static_cast<int>(profiles.size()) - 1)];
        p.platform = platform;
        p.output.clear();
        p.name = m_Profiles.UniqueName(BuildProfile::PlatformName(platform));
        if (p.scene.empty() && !m_Scenes.empty())
            p.scene = m_Scenes.front();
        profiles.push_back(p);
        m_Selected = static_cast<int>(profiles.size()) - 1;
    };
    if (ImGui::SmallButton("+ Windows"))
        add(BuildPlatform::Windows);
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Web"))
        add(BuildPlatform::Web);
    const bool selected = m_Selected >= 0 && m_Selected < static_cast<int>(profiles.size());
    ImGui::BeginDisabled(!selected);
    if (ImGui::SmallButton("Duplicate") && selected)
    {
        BuildProfile copy = profiles[m_Selected];
        copy.name = m_Profiles.UniqueName(copy.name);
        copy.output.clear();
        profiles.push_back(copy);
        m_Selected = static_cast<int>(profiles.size()) - 1;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Delete") && selected)
    {
        profiles.erase(profiles.begin() + m_Selected);
        m_Selected = std::min(m_Selected, static_cast<int>(profiles.size()) - 1);
    }
    if (ImGui::SmallButton("Make active") && selected)
        m_Profiles.active = profiles[m_Selected].name;
    ImGui::SetItemTooltip("The one File > Build builds and MarianaExport builds without --profile");
    ImGui::EndDisabled();
}

void BuildWindow::DrawProfile(BuildProfile& p)
{
    const std::string oldName = p.name;
    ImGui::PushItemWidth(-160.0f);
    if (ImGui::InputText("Name", &p.name) && m_Profiles.active == oldName)
        m_Profiles.active = p.name;

    int platform = static_cast<int>(p.platform);
    if (ImGui::Combo("Platform", &platform, "Windows\0Web\0"))
        p.platform = static_cast<BuildPlatform>(platform);
    int configuration = static_cast<int>(p.configuration);
    if (ImGui::Combo("Configuration", &configuration, "Release\0Debug\0"))
        p.configuration = static_cast<BuildConfiguration>(configuration);
    ImGui::SetItemTooltip("Release is fast and small; Debug checks more and is slower");

    ImGui::SeparatorText("Game");
    ImGui::InputText("Game name", &p.gameName);
    ImGui::SetItemTooltip("%s", p.platform == BuildPlatform::Windows ? "The executable is <name>.exe" : "The window's title when it has none of its own");
    ImGui::InputTextWithHint("Window title", p.gameName.c_str(), &p.title);
    int size[2] = { p.width, p.height };
    if (ImGui::InputInt2("Window size", size))
    {
        p.width = std::max(size[0], 64);
        p.height = std::max(size[1], 64);
    }
    ImGui::InputTextWithHint("Icon", "a resource path, like /logo.png", &p.icon);

    ImGui::SeparatorText("Content");
    if (ImGui::BeginCombo("Start scene", p.scene.empty() ? "(none)" : p.scene.c_str()))
    {
        for (const std::string& scene : m_Scenes)
            if (ImGui::Selectable(scene.c_str(), scene == p.scene))
                p.scene = scene;
        ImGui::EndCombo();
    }
    if (m_Scenes.size() > 1 && ImGui::TreeNode("More scenes"))
    {
        ImGui::TextDisabled("Packaged with it too:");
        for (const std::string& scene : m_Scenes)
        {
            if (scene == p.scene)
                continue;
            const auto found = std::find(p.scenes.begin(), p.scenes.end(), scene);
            bool on = found != p.scenes.end();
            if (ImGui::Checkbox(scene.c_str(), &on))
            {
                if (on)
                    p.scenes.push_back(scene);
                else
                    p.scenes.erase(found);
            }
        }
        ImGui::TreePop();
    }
    if (ImGui::BeginCombo("Pipeline", p.pipeline.empty() ? "(the scene's)" : p.pipeline.c_str()))
    {
        if (ImGui::Selectable("(the scene's)", p.pipeline.empty()))
            p.pipeline.clear();
        if (ImGui::Selectable("/Pipelines/Default.pipeline", p.pipeline == "/Pipelines/Default.pipeline"))
            p.pipeline = "/Pipelines/Default.pipeline";
        for (const std::string& pipeline : m_Pipelines)
            if (pipeline != "/Pipelines/Default.pipeline" && ImGui::Selectable(pipeline.c_str(), pipeline == p.pipeline))
                p.pipeline = pipeline;
        ImGui::EndCombo();
    }
    ImGui::Checkbox("Follow camera", &p.followCamera);
    ImGui::SetItemTooltip("A scene without a camera gets one chasing what the keys drive");
    ImGui::SameLine();
    ImGui::Checkbox("Log frame times", &p.logProfile);
    ImGui::SetItemTooltip("Every 5 s the game logs its average CPU and GPU times and its busiest scopes");
    ImGui::Checkbox("Whole project", &p.wholeProject);
    ImGui::SetItemTooltip("Every file of the project instead of only what the scenes use (bigger, but nothing is left out)");

    ImGui::SeparatorText("Output");
    ImGui::InputTextWithHint("Folder", ("Builds/" + p.name).c_str(), &p.output);
    ImGui::TextDisabled("%s", p.OutputFolder(m_Project).string().c_str());
    ImGui::PopItemWidth();
}

void BuildWindow::DrawTools()
{
    if (!ImGui::CollapsingHeader("Build tools (this machine's)"))
        return;
    bool changed = false;
    ImGui::PushItemWidth(-200.0f);
    changed |= PathField("CMake", m_Tools.cmake);
    changed |= PathField("Native build folder", m_Tools.nativeBuild);
    ImGui::SetItemTooltip("A CMake build folder of the engine (the editor's own by default); the player is built there");
    changed |= PathField("Emscripten SDK", m_Tools.emsdk);
    changed |= PathField("Web build folder (Release)", m_Tools.webBuildRelease);
    changed |= PathField("Web build folder (Debug)", m_Tools.webBuildDebug);
    ImGui::PopItemWidth();
    for (BuildPlatform platform : { BuildPlatform::Windows, BuildPlatform::Web })
    {
        const std::string missing = m_Tools.Missing(platform);
        if (missing.empty())
            ImGui::TextColored(ImVec4(0.45f, 0.8f, 0.45f, 1.0f), "%s: ready", BuildProfile::PlatformName(platform));
        else
            ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f), "%s: %s", BuildProfile::PlatformName(platform), missing.c_str());
    }
    if (ImGui::SmallButton("Defaults"))
    {
        m_Tools = BuildTools::Defaults();
        changed = true;
    }
    if (changed)
        SaveTools();
}

void BuildWindow::DrawBuild()
{
    DrawTools();

    const bool building = IsBuilding();
    const bool selected = m_Selected >= 0 && m_Selected < static_cast<int>(m_Profiles.profiles.size());
    ImGui::BeginDisabled(!CanBuild() || building || !selected);
    if (ImGui::Button("Build"))
        Build(false);
    ImGui::SameLine();
    if (ImGui::Button("Build and Run"))
        Build(true);
    ImGui::EndDisabled();
    if (!CanBuild())
        ImGui::SetItemTooltip("Building runs CMake and Emscripten: it needs the desktop editor");
    ImGui::SameLine();
    ImGui::BeginDisabled(!building);
    if (ImGui::Button("Cancel") && m_Exporter)
        m_Exporter->Cancel();
    ImGui::EndDisabled();
    if (m_Exporter)
    {
        ImGui::SameLine();
        if (ImGui::Button("Open output folder"))
            ScriptEditor::OpenWithSystem(m_Exporter->Output());
    }
    if (m_Runner.IsServing())
    {
        ImGui::SameLine();
        if (ImGui::Button("Stop serving"))
        {
            m_Runner.Stop();
            m_Status.clear();
        }
    }
    if (m_Exporter)
    {
        const bool failed = m_Exporter->IsDone() && !m_Exporter->Succeeded();
        ImGui::ProgressBar(m_Exporter->Progress(), ImVec2(-1.0f, 0.0f), m_Exporter->Step().c_str());
        if (failed)
            ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.35f, 1.0f), "The build failed: the log says why");
    }
    if (!m_Status.empty())
        ImGui::TextUnformatted(m_Status.c_str());

    ImGui::BeginChild("##log", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_Log.size()));
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            const GameExporter::Line& line = m_Log[static_cast<size_t>(i)];
            if (line.error)
                ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.35f, 1.0f), "%s", line.text.c_str());
            else if (line.text.rfind("== ", 0) == 0)
                ImGui::TextColored(ImVec4(0.55f, 0.75f, 1.0f, 1.0f), "%s", line.text.c_str());
            else
                ImGui::TextUnformatted(line.text.c_str());
        }
    }
    if (m_ScrollLog)
    {
        ImGui::SetScrollHereY(1.0f);
        m_ScrollLog = false;
    }
    ImGui::EndChild();
}
