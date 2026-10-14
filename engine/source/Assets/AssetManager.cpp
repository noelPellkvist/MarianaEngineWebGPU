#include <Mariana/Assets/AssetManager.hpp>
#include <Mariana/Foundation/Logger.hpp>

#include <algorithm>
#include <cctype>
#include <exception>
#include <system_error>

namespace
{
    std::string ToLower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }
}

AssetManager2::~AssetManager2()
{
    // Free the assets while the metadata is still around for the handles they release.
    storages.clear();
}

std::string AssetManager2::MakeKey(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::path absolute = std::filesystem::absolute(path, ec);
    std::string key = (ec ? path : absolute).lexically_normal().generic_string();
#ifdef _WIN32
    key = ToLower(std::move(key));
#endif
    return key;
}

AssetID AssetManager2::FindID(const std::string& key) const
{
    auto it = keyToID.find(key);
    return it != keyToID.end() ? it->second : AssetID{};
}

AssetMetadata& AssetManager2::CreateMetadata(AssetType type, const std::string& name, const std::string& key, const std::filesystem::path& path)
{
    AssetID id = AssetID::Generate();
    while (metadata.find(id) != metadata.end())
        id = AssetID::Generate();

    AssetMetadata& meta = metadata[id];
    meta.id = id;
    meta.type = type;
    meta.name = name;
    meta.key = key;
    meta.path = path;

    if (!key.empty())
        keyToID[key] = id;

    return meta;
}

void AssetManager2::AddLoader(const std::vector<std::string>& extensions, LoaderEntry entry)
{
    for (const std::string& extension : extensions)
        loaders[ToLower(extension)] = entry;
}

const AssetManager2::LoaderEntry* AssetManager2::FindLoader(const std::filesystem::path& path) const
{
    auto it = loaders.find(ToLower(path.extension().string()));
    return it != loaders.end() ? &it->second : nullptr;
}

AssetID AssetManager2::GetOrCreateFileAsset(const std::filesystem::path& path)
{
    const std::string key = MakeKey(path);
    if (AssetID existing = FindID(key); existing.IsValid())
        return existing;

    const LoaderEntry* loader = FindLoader(path);
    if (!loader)
    {
        Logger::Error("No asset loader registered for: " + path.string());
        return {};
    }

    return CreateMetadata(loader->type, path.stem().string(), key, path).id;
}

bool AssetManager2::Load(AssetID id)
{
    auto it = metadata.find(id);
    if (it == metadata.end())
        return false;

    AssetMetadata& meta = it->second;
    if (meta.status == AssetStatus::Loaded)
        return true;

    if (meta.status == AssetStatus::Loading)
    {
        Logger::Error("Asset is already loading (circular dependency?): " + meta.name);
        return false;
    }

    if (meta.path.empty())
    {
        Logger::Error("Asset '" + meta.name + "' was created in memory and can't be reloaded");
        return false;
    }

    const LoaderEntry* loader = FindLoader(meta.path);
    if (!loader)
    {
        Logger::Error("No asset loader registered for: " + meta.path.string());
        meta.status = AssetStatus::Failed;
        return false;
    }

    meta.status = AssetStatus::Loading;

    // The loader may register sub-assets, so hand it a copy rather than a reference into the map.
    const AssetMetadata snapshot = meta;
    bool loaded = false;
    try
    {
        loaded = loader->load(*this, id, snapshot);
    }
    catch (const std::exception& e)
    {
        Logger::Error("Exception while loading " + snapshot.path.string() + ": " + e.what());
    }
    catch (...)
    {
        Logger::Error("Unknown exception while loading " + snapshot.path.string());
    }

    metadata.at(id).status = loaded ? AssetStatus::Loaded : AssetStatus::Failed;
    if (!loaded)
        Logger::Error("Failed to load asset: " + snapshot.path.string());

    return loaded;
}

void AssetManager2::AddHandle(AssetID id)
{
    auto it = metadata.find(id);
    if (it != metadata.end())
    {
        it->second.refCount++;
    }
}

void AssetManager2::Release(AssetID id)
{
    auto it = metadata.find(id);
    if (it != metadata.end())
    {
        if (it->second.refCount > 0)
        {
            it->second.refCount--;
        }
    }
}

void AssetManager2::UnloadFromStorages(AssetID id)
{
    for (auto& [type, storage] : storages)
    {
        if (storage->Contains(id))
            storage->Unload(id);
    }
}

size_t AssetManager2::UnloadUnused()
{
    size_t unloadedCount = 0;
    bool unloadedAny = true;

    while (unloadedAny)
    {
        unloadedAny = false;

        std::vector<AssetID> unused;
        for (const auto& [id, meta] : metadata)
        {
            if (meta.refCount == 0 && !meta.persistent && meta.status == AssetStatus::Loaded)
                unused.push_back(id);
        }

        for (AssetID id : unused)
        {
            metadata.at(id).status = AssetStatus::Unloaded;
            UnloadFromStorages(id);
            ++unloadedCount;
            unloadedAny = true;
        }
    }

    return unloadedCount;
}

void AssetManager2::SetPersistent(AssetID id, bool persistent)
{
    auto it = metadata.find(id);
    if (it != metadata.end())
        it->second.persistent = persistent;
}

void AssetManager2::SetSource(AssetID id, const std::filesystem::path& source)
{
    auto it = metadata.find(id);
    if (it != metadata.end())
        it->second.source = source;
}

bool AssetManager2::IsLoaded(AssetID id) const
{
    auto it = metadata.find(id);
    return it != metadata.end() && it->second.status == AssetStatus::Loaded;
}

AssetStatus AssetManager2::GetStatus(AssetID id) const
{
    auto it = metadata.find(id);
    if (it == metadata.end())
        return AssetStatus::Unloaded;

    return it->second.status;
}

AssetType AssetManager2::GetType(AssetID id) const
{
    auto it = metadata.find(id);
    if (it == metadata.end())
        return AssetType::Unknown;

    return it->second.type;
}

const AssetMetadata* AssetManager2::GetMetadata(AssetID id) const
{
    auto it = metadata.find(id);
    return it != metadata.end() ? &it->second : nullptr;
}
