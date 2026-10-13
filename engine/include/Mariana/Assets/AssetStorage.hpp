#pragma once

#include <Mariana/Assets/AssetsStructs.hpp>
#include <memory>
#include <unordered_map>

class IAssetStorage
{
public:
    virtual ~IAssetStorage() = default;

    virtual void Unload(AssetID id) = 0;
    virtual bool Contains(AssetID id) const = 0;
};

template <typename T>
class AssetStorage : public IAssetStorage
{
public:
    std::unordered_map<AssetID, std::shared_ptr<T>> assets;

    void Unload(AssetID id) override
    {
        auto it = assets.find(id);
        if (it == assets.end())
            return;

        // Destroy the asset only after the map is consistent again: its destructor may release
        // handles to other assets, which calls back into the AssetManager.
        std::shared_ptr<T> keepAlive = std::move(it->second);
        assets.erase(it);
    }

    bool Contains(AssetID id) const override
    {
        return assets.find(id) != assets.end();
    }

    std::shared_ptr<T> Get(AssetID id)
    {
        auto it = assets.find(id);

        if (it == assets.end())
            return nullptr;

        return it->second;
    }

    void Insert(AssetID id, std::shared_ptr<T> asset)
    {
        assets[id] = std::move(asset);
    }
};
