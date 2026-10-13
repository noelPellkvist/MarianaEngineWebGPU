#pragma once
#include <filesystem>
#include <string>
#include <vector>

// Where resource paths ("/Shaders/test.wgsl", "/Models/Tree.glb") are found: a list of root folders
// searched in order. Apps add their own (a project's content, the editor's resources) in front of
// the engine's built-in resources, so a project can override an engine file by putting one at the
// same path. Resource paths are always relative to the roots, never OS paths.
namespace FileSystem
{
    // Adds a root folder; the most recently added is searched first, the engine's last.
    void AddRoot(const std::filesystem::path& folder);
    // The engine's built-in resources (a development build's engine/resources by default).
    void SetEngineRoot(const std::filesystem::path& folder);

    // Every root, in search order.
    std::vector<std::filesystem::path> Roots();

    // The file a resource path refers to: the first root that has it, or empty when none does.
    std::filesystem::path Resolve(const std::string& path);
    // Where to write a resource path: the file it already resolves to, or under the first root.
    std::filesystem::path ResolveForWrite(const std::string& path);
}
