#pragma once

#include <Editor/Windows/EditorWindow.hpp>
#include <Mariana/Export/GameExporter.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// File > Build: the project's build profiles (BuildProfiles.json, saved as they are edited), the
// tools they build with, and building one into a game (GameExporter on a thread of its own) with its
// log; Build and Run then starts it (the exe, or the web build served on localhost). Building needs
// the desktop editor: on the web the profiles can be edited only.
class BuildWindow final : public EditorWindow
{
public:
    BuildWindow(GUI& gui, std::filesystem::path project);
    ~BuildWindow() override;
    void Draw() override;

    // Builds the selected profile; with `run`, starts the game when it is built.
    void Build(bool run);
    static bool CanBuild();
    bool IsBuilding() const { return m_Exporter && m_Exporter->IsRunning(); }

private:
    std::filesystem::path m_Project;
    BuildProfiles m_Profiles;
    std::string m_SavedJson;
    int m_Selected = 0;
    BuildTools m_Tools;
    std::string m_SavedTools;
    std::unique_ptr<GameExporter> m_Exporter;
    bool m_RunWhenBuilt = false;
    GameRunner m_Runner;
    std::vector<GameExporter::Line> m_Log;
    bool m_ScrollLog = false;
    std::string m_Status;
    // The project's scenes and pipelines (resource paths), listed when the window opens.
    std::vector<std::string> m_Scenes;
    std::vector<std::string> m_Pipelines;
    bool m_WasOpen = false;

    void Refresh();
    void Save();
    void SaveTools();
    void Poll();
    void DrawProfiles();
    void DrawProfile(BuildProfile& profile);
    void DrawTools();
    void DrawBuild();
};
