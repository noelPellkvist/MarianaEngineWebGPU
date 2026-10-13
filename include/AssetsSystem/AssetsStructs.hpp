#pragma once

#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <filesystem>
#include <unordered_map>
#include <functional>

struct UUID
{
    uint64_t high = 0;
    uint64_t low = 0;

    static UUID Generate()
    {
        static thread_local std::random_device rd;
        static thread_local std::mt19937_64 engine(rd());

        UUID id;
        id.high = engine();
        id.low = engine();

        id.high &= 0xFFFFFFFFFFFF0FFFULL;
        id.high |= 0x0000000000004000ULL;

        id.low &= 0x3FFFFFFFFFFFFFFFULL;
        id.low |= 0x8000000000000000ULL;

        return id;
    }

    bool operator==(const UUID& other) const
    {
        return high == other.high && low == other.low;
    }

    bool operator!=(const UUID& other) const
    {
        return !(*this == other);
    }

    bool IsValid() const
    {
        return high != 0 || low != 0;
    }

    // Canonical 8-4-4-4-12 hex form.
    std::string ToString() const
    {
        char buffer[37];
        std::snprintf(buffer, sizeof(buffer), "%08x-%04x-%04x-%04x-%012llx",
                      static_cast<uint32_t>(high >> 32),
                      static_cast<uint32_t>((high >> 16) & 0xFFFF),
                      static_cast<uint32_t>(high & 0xFFFF),
                      static_cast<uint32_t>(low >> 48),
                      static_cast<unsigned long long>(low & 0xFFFFFFFFFFFFULL));
        return buffer;
    }
};

namespace std
{
    template<>
    struct hash<UUID>
    {
        size_t operator()(const UUID& id) const noexcept
        {
            size_t h1 = std::hash<uint64_t>{}(id.high);
            size_t h2 = std::hash<uint64_t>{}(id.low);
            return h1 ^ (h2 << 1);
        }
    };
}

using AssetID = UUID;

enum class AssetType : uint8_t
{
    Texture,
    Mesh,
    Material,
    Shader,
    Audio,
    Font,
    Animation,
    Skin,
    Script,
    Prefab,
    Unknown
};

inline const char* AssetTypeName(AssetType type)
{
    switch (type)
    {
        case AssetType::Texture:   return "Texture";
        case AssetType::Mesh:      return "Mesh";
        case AssetType::Material:  return "Material";
        case AssetType::Shader:    return "Shader";
        case AssetType::Audio:     return "Audio";
        case AssetType::Font:      return "Font";
        case AssetType::Animation: return "Animation";
        case AssetType::Skin:      return "Skin";
        case AssetType::Script:    return "Script";
        case AssetType::Prefab:    return "Prefab";
        default:                   return "Unknown";
    }
}

enum class AssetStatus
{
    Unloaded,
    Loading,
    Loaded,
    Failed
};

inline const char* AssetStatusName(AssetStatus status)
{
    switch (status)
    {
        case AssetStatus::Unloaded: return "Unloaded";
        case AssetStatus::Loading:  return "Loading";
        case AssetStatus::Loaded:   return "Loaded";
        case AssetStatus::Failed:   return "Failed";
        default:                    return "Unknown";
    }
}

struct AssetMetadata
{
    AssetID id{};
    AssetType type = AssetType::Unknown;
    std::string name;

    // File a registered loader can (re)load this asset from. Empty for assets created in
    // memory (sub-assets of a file, builtins), which cannot be reloaded once unloaded.
    std::filesystem::path path;

    // Unique lookup key: the normalized file path, "<file>#<kind>/<index>" for sub-assets
    // of a file, or "builtin://..." for engine-provided assets. May be empty.
    std::string key;

    AssetStatus status = AssetStatus::Unloaded;

    uint32_t refCount = 0;
    bool persistent = false;

    std::unordered_map<std::string, std::string> tags;
};
