#pragma once

#include <Editor/Windows/EditorWindow.hpp>
#include <Mariana/Graphics/Texture.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class AssetsExplorer final : public EditorWindow
{
public:
    AssetsExplorer(GUI& gui);
    void Draw() override;

private:
    // A directory listing entry, with everything the grid needs precomputed at scan time.
    struct Entry
    {
        std::filesystem::path path;
        std::string pathString;
        std::string name;
        std::string extension; // lowercase, with the dot
        std::string localKey;  // "/sub/dir/file.ext" relative to RESOURCE_DIR
        bool isDir = false;
        bool isImage = false;
        bool isGltf = false;
    };

    std::unordered_map<std::string, Texture> AssetsTextures;
    std::unordered_set<std::string> m_failedThumbnails;
    std::vector<Entry> m_entries;
    std::filesystem::path m_entriesDir;
    std::filesystem::path m_entriesRelative;
    double m_lastScan = -1.0;

    void DrawAssetsWindow();
    void ScanDirectory(const std::filesystem::path& dir, const std::filesystem::path& root);
    bool LoadFileTexture(const std::string& path);
};
