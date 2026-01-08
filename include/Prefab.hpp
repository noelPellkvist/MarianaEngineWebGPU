#pragma once
#include <cstdint>
#include <string>
#include <ECS.hpp>

class Prefab {
public:
    explicit Prefab(const char* name = nullptr);
    Prefab(Prefab&&) noexcept;
    Prefab& operator=(Prefab&&) noexcept;

    Prefab(const Prefab&) = delete;
    Prefab& operator=(const Prefab&) = delete;

    // Build prefab from an existing scene entity (root + hierarchy)
    static Prefab FromEntity(const Scene& src, Entity root);

    bool IsValid() const { return _rootId != 0; }
    const char* GetName() const;
    Entity Root();
    Entity Root() const;

    // Create children inside the prefab scene.
    Entity Instantiate(const char* name = nullptr);

    // Convenience: operate on the root entity.
    template<class T> Prefab& Add(const T& value);
    template<class T> bool    Has() const;
    template<class T> T*      Get();
    template<class T> void    Remove();

private:
    friend class Scene;

    Scene _scene;
    uint32_t _rootId = 0;

    // Internal clone helpers
    static uint32_t CloneEntityRecursive(const Scene& src, Scene& dst, uint32_t srcId, uint32_t dstParentId);
};

template<class T>
inline Prefab& Prefab::Add(const T& value) {
    Root().Add<T>(value);
    return *this;
}

template<class T>
inline bool Prefab::Has() const {
    return Root().Has<T>();
}

template<class T>
inline T* Prefab::Get() {
    return Root().Get<T>();
}

template<class T>
inline void Prefab::Remove() {
    Root().Remove<T>();
}
