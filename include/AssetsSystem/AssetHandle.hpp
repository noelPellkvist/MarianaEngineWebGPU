#pragma once

#include <AssetsSystem/AssetsStructs.hpp>
#include <memory>

class AssetManager2;

// Ref-counted reference to an asset owned by an AssetManager2. While any handle to an asset
// exists, AssetManager2::UnloadUnused() keeps it loaded, so the handle can resolve the asset
// once and hand it out without lookups.
//
// The member functions that talk to the manager are defined in AssetsSystem/AssetManager.hpp,
// so include that header (not just this one) wherever handles are created or copied.
template <typename T>
class AssetHandle
{
public:
    AssetHandle() = default;

    AssetHandle(AssetManager2* manager, AssetID id);

    AssetHandle(const AssetHandle& other)
        : manager(other.manager), id(other.id), asset(other.asset)
    {
        AddRef();
    }

    AssetHandle& operator=(const AssetHandle& other)
    {
        if (this == &other)
            return *this;

        ReleaseRef();

        manager = other.manager;
        id = other.id;
        asset = other.asset;

        AddRef();

        return *this;
    }

    AssetHandle(AssetHandle&& other) noexcept
        : manager(other.manager), id(other.id), asset(std::move(other.asset))
    {
        other.manager = nullptr;
        other.id = {};
    }

    AssetHandle& operator=(AssetHandle&& other) noexcept
    {
        if (this == &other)
            return *this;

        ReleaseRef();

        manager = other.manager;
        id = other.id;
        asset = std::move(other.asset);

        other.manager = nullptr;
        other.id = {};

        return *this;
    }

    ~AssetHandle()
    {
        ReleaseRef();
    }

    // The asset, or null if the handle is empty or the asset isn't a T.
    const std::shared_ptr<T>& Get() const { return asset; }

    bool IsLoaded() const;

    // Name, type, key etc. of the referenced asset, or nullptr for an empty handle.
    const AssetMetadata* GetMetadata() const;

    void Reset()
    {
        ReleaseRef();
        manager = nullptr;
        id = {};
        asset.reset();
    }

    AssetID GetID() const
    {
        return id;
    }

    bool IsValid() const
    {
        return manager != nullptr && id.IsValid();
    }

private:
    void AddRef();
    void ReleaseRef();

private:
    AssetManager2* manager = nullptr;
    AssetID id{};
    std::shared_ptr<T> asset;
};
