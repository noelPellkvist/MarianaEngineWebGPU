#pragma once

#include <Mariana/Assets/AssetsStructs.hpp>
#include <Mariana/Assets/AssetStorage.hpp>
#include <Mariana/Assets/AssetHandle.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

// Owns every asset. Assets live in per-type storages and are referenced through ref-counted
// AssetHandle<T>s. File assets are loaded by loaders registered per extension and are
// deduplicated by path; assets made in memory (sub-assets of a file, builtins) are added with
// RegisterAsset(). Anything without handles that isn't persistent is freed by UnloadUnused().
class AssetManager2
{
public:
    template <typename T>
    using Loader = std::function<std::shared_ptr<T>(AssetManager2&, const AssetMetadata&)>;

    AssetManager2() = default;
    ~AssetManager2();

    AssetManager2(const AssetManager2&) = delete;
    AssetManager2& operator=(const AssetManager2&) = delete;

    // Registers `loader` as the way to load files with any of `extensions` (e.g. ".glb") into a T.
    template <typename T>
    void RegisterLoader(AssetType type, const std::vector<std::string>& extensions, Loader<T> loader);

    // Loads the file at `path`, or reuses it if it is already loaded. Returns an empty handle if
    // no loader handles the extension or loading failed.
    template <typename T>
    AssetHandle<T> Load(const std::filesystem::path& path);

    // Handle to a known asset, loading it from its file first if needed. Empty if that fails.
    template <typename T>
    AssetHandle<T> LoadAsset(AssetID id);

    template <typename T>
    std::shared_ptr<T> Get(AssetID id);

    // Adds an asset created in memory and returns a handle to it. If `key` is already known its
    // ID is reused, so re-creating an unloaded sub-asset keeps the same ID.
    template <typename T>
    AssetHandle<T> RegisterAsset(std::shared_ptr<T> asset, AssetType type, const std::string& name, const std::string& key = {});

    template <typename T>
    void RegisterAsset(AssetID id, std::shared_ptr<T> asset, AssetType type);

    // Handle to the loaded T registered under `key`, or an empty handle.
    template <typename T>
    AssetHandle<T> Find(const std::string& key);

    // Calls fn(AssetID, T&) for every loaded T.
    template <typename T, typename Fn>
    void ForEach(Fn&& fn);

    bool Load(AssetID id);
    void AddHandle(AssetID id);
    void Release(AssetID id);

    // Frees every loaded asset that has no handles and isn't persistent. Freeing an asset can
    // release the last handle to others (a prefab's meshes, a material's textures), so this
    // repeats until nothing changes. Returns the number of assets unloaded.
    size_t UnloadUnused();

    void SetPersistent(AssetID id, bool persistent);

    bool IsLoaded(AssetID id) const;
    AssetStatus GetStatus(AssetID id) const;
    AssetType GetType(AssetID id) const;
    const AssetMetadata* GetMetadata(AssetID id) const;
    const std::unordered_map<AssetID, AssetMetadata>& GetAllMetadata() const { return metadata; }

    AssetID FindID(const std::string& key) const;

    // Lookup key for a file path: absolute, normalized, forward slashes (lowercase on Windows).
    static std::string MakeKey(const std::filesystem::path& path);

private:
    struct LoaderEntry
    {
        AssetType type = AssetType::Unknown;
        std::function<bool(AssetManager2&, AssetID, const AssetMetadata&)> load;
    };

    template <typename T>
    AssetStorage<T>& GetStorage();

    AssetMetadata& CreateMetadata(AssetType type, const std::string& name, const std::string& key, const std::filesystem::path& path);
    AssetID GetOrCreateFileAsset(const std::filesystem::path& path);
    const LoaderEntry* FindLoader(const std::filesystem::path& path) const;
    void AddLoader(const std::vector<std::string>& extensions, LoaderEntry entry);
    void UnloadFromStorages(AssetID id);

private:
    // Declared before `storages` so it outlives them: destroying an asset releases the handles it holds.
    std::unordered_map<AssetID, AssetMetadata> metadata;
    std::unordered_map<std::string, AssetID> keyToID;
    std::unordered_map<std::string, LoaderEntry> loaders; // by lowercase extension
    std::unordered_map<std::type_index, std::unique_ptr<IAssetStorage>> storages;
};

// ==============================
// Template implementations
// ==============================

template <typename T>
AssetStorage<T>& AssetManager2::GetStorage()
{
    std::type_index key = std::type_index(typeid(T));

    auto it = storages.find(key);

    if (it == storages.end())
    {
        auto storage = std::make_unique<AssetStorage<T>>();
        auto* ptr = storage.get();

        storages.emplace(key, std::move(storage));

        return *ptr;
    }

    return *static_cast<AssetStorage<T>*>(it->second.get());
}

template <typename T>
void AssetManager2::RegisterLoader(AssetType type, const std::vector<std::string>& extensions, Loader<T> loader)
{
    LoaderEntry entry;
    entry.type = type;
    entry.load = [loader = std::move(loader)](AssetManager2& manager, AssetID id, const AssetMetadata& meta) -> bool
    {
        std::shared_ptr<T> asset = loader(manager, meta);
        if (!asset)
            return false;

        manager.GetStorage<T>().Insert(id, std::move(asset));
        return true;
    };
    AddLoader(extensions, std::move(entry));
}

template <typename T>
AssetHandle<T> AssetManager2::Load(const std::filesystem::path& path)
{
    return LoadAsset<T>(GetOrCreateFileAsset(path));
}

template <typename T>
AssetHandle<T> AssetManager2::LoadAsset(AssetID id)
{
    if (!id.IsValid() || metadata.find(id) == metadata.end())
        return {};

    if (!IsLoaded(id) && !Load(id))
        return {};

    return AssetHandle<T>(this, id);
}

template <typename T>
std::shared_ptr<T> AssetManager2::Get(AssetID id)
{
    return GetStorage<T>().Get(id);
}

template <typename T>
AssetHandle<T> AssetManager2::RegisterAsset(std::shared_ptr<T> asset, AssetType type, const std::string& name, const std::string& key)
{
    if (!asset)
        return {};

    AssetID id = key.empty() ? AssetID{} : FindID(key);
    if (!id.IsValid())
        id = CreateMetadata(type, name, key, {}).id;

    GetStorage<T>().Insert(id, std::move(asset));

    AssetMetadata& meta = metadata.at(id);
    meta.type = type;
    meta.name = name;
    meta.status = AssetStatus::Loaded;

    return AssetHandle<T>(this, id);
}

template <typename T>
void AssetManager2::RegisterAsset(AssetID id, std::shared_ptr<T> asset, AssetType type)
{
    GetStorage<T>().Insert(id, std::move(asset));

    auto& meta = metadata[id];
    meta.id = id;
    meta.type = type;
    meta.status = AssetStatus::Loaded;
}

template <typename T>
AssetHandle<T> AssetManager2::Find(const std::string& key)
{
    AssetID id = FindID(key);
    if (!id.IsValid() || !IsLoaded(id) || !GetStorage<T>().Contains(id))
        return {};

    return AssetHandle<T>(this, id);
}

template <typename T, typename Fn>
void AssetManager2::ForEach(Fn&& fn)
{
    for (auto& [id, asset] : GetStorage<T>().assets)
    {
        if (asset)
            fn(id, *asset);
    }
}

template <typename T>
AssetHandle<T>::AssetHandle(AssetManager2* manager, AssetID id)
    : manager(manager), id(id), asset(manager ? manager->Get<T>(id) : nullptr)
{
    AddRef();
}

template <typename T>
bool AssetHandle<T>::IsLoaded() const
{
    return manager ? manager->IsLoaded(id) : false;
}

template <typename T>
const AssetMetadata* AssetHandle<T>::GetMetadata() const
{
    return manager ? manager->GetMetadata(id) : nullptr;
}

template <typename T>
void AssetHandle<T>::AddRef()
{
    if (manager && id.IsValid())
        manager->AddHandle(id);
}

template <typename T>
void AssetHandle<T>::ReleaseRef()
{
    if (manager && id.IsValid())
        manager->Release(id);
}
