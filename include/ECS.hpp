#pragma once
#include <cstdint>
#include <typeinfo>
#include <typeindex>
#include <vector>
#include <functional>
#include <utility>
#include <memory>
#include <cstddef>
#include <string>
#include <type_traits>

struct LocalTRS {
    float pos[3]      {0.f,0.f,0.f};
    float rot_euler[3]{0.f,0.f,0.f};
    float scl[3]      {1.f,1.f,1.f};
    uint32_t version = 0;
};

struct WorldXform {
    float model[16] { 
        1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,0,1
    };
    float normal[9] { 
        1,0,0, 0,1,0, 0,0,1
    };
    uint32_t id = 0;
};

struct XformCache { uint32_t local_v=~0u, parent_v=~0u, world_v=0; };
struct TransformClock { uint32_t tick = 1; };

struct ComponentView {
    uint32_t id = 0;
    const char* name = nullptr;
    const void* data = nullptr;
    std::size_t size = 0;
    std::size_t align = 0;
    bool isTag = false;
};

namespace ECS {
    void RegisterComponent(const std::type_info& ti, std::size_t size, std::size_t align, const char* name = nullptr);
    void RegisterTag(const std::type_info& ti, const char* name = nullptr);

    template<class T>
    inline void RegisterComponent(const char* name = nullptr) {
        RegisterComponent(typeid(T), sizeof(T), alignof(T), name);
    }

    template<class T>
    inline void RegisterTag(const char* name = nullptr) {
        RegisterTag(typeid(T), name);
    }
}

class Scene;

class System {
public:
    struct Impl;            // opaque, defined in cpp
    System() noexcept : _p(nullptr) {}
    ~System();

    System(System&&) noexcept;
    System& operator=(System&&) noexcept;

    // Not copyable
    System(const System&) = delete;
    System& operator=(const System&) = delete;

    // Run the underlying flecs system (delta default 0). Implemented in cpp.
    void Run(float delta = 0.0f);

    bool IsValid() const noexcept { return _p != nullptr; }

private:
    explicit System(Impl* p) : _p(p) {}
    Impl* _p;

    friend class Scene;
};

class Prefab;

class Entity {
public:
    Entity() = default;
    bool IsValid() const { return _id != 0; }

    Entity& SetName(const char* name);
    const char* GetName() const;
    Entity& SetParent(Entity parent);
    int GetChildCount() const;
    inline bool HasChildren() { return GetChildCount() > 0; }

    Entity& AddTransform(); 
    Entity& SetPosition(float x, float y, float z);
    Entity& SetRotationEuler(float rx, float ry, float rz);
    Entity& SetScale(float sx, float sy, float sz);
    Entity& SetScaleUniform(float s);

    template<class T> Entity&   Add(const T& value);
    template<class T> Entity&   AddTag();
    template<class T> bool      Has() const;
    template<class T> bool      HasTag() const;
    template<class T> T*        Get();
    template<class T> const T*  TryGet() const;
    template<class T> void      Remove();
    template<class T> void      RemoveTag();

    uint32_t RawId() const { return _id; }

private:
    friend class Scene;
    friend class Prefab;
    Entity(Scene* s, uint32_t id) : _scene(s), _id(id) {}
    Scene* _scene = nullptr;
    uint32_t _id = 0;
};

class Scene {
public:
    struct Impl;

    Scene();
    ~Scene();
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) noexcept;
    Scene& operator=(Scene&&) noexcept;

    Entity Instantiate(const char* name = nullptr);
    Entity Instantiate(const Prefab& prefab, const char* name = nullptr);
    void Destroy(Entity e);
    void Update(float dt);
    void UpdateComponentRegistry();

    template<class Fn>
    void ForEachChild(Entity parent, Fn&& fn);

    template<class Fn>
    void ForEachChild(Entity parent, Fn&& fn) const;

    template<class Fn>
    void ForEachRoot(Fn&& fn);

    template<class Fn>
    void ForEachRoot(Fn&& fn) const;

    template<class Fn>
    void ForEachComponent(Entity e, Fn&& fn) const;

    template<typename... Components, typename Fn>
    System CreateSystem(Fn&& fn, bool cascade = false);

    Entity Parent(Entity e) const;
    Entity FromId(uint32_t id) const;

private:
    friend class Entity;
    friend class Prefab;

    uint32_t _create(const char* name);
    void     _destroy(uint32_t eid);
    void     _update(float dt);
    void     _applyRegistry();
    void     _setName(uint32_t eid, const char* name);
    const char* _getName(uint32_t eid) const;
    int _childCount(uint32_t parentId) const;

    bool     _has(uint32_t id, const std::type_info& ti) const;
    void*    _getMut(uint32_t id, const std::type_info& ti, std::size_t size, std::size_t align) const;
    void     _addSet(uint32_t id, const std::type_info& ti, const void* data, std::size_t size, std::size_t align);
    void     _remove(uint32_t id, const std::type_info& ti) const;
    void     _addTag(uint32_t id, const std::type_info& ti);
    bool     _hasTag(uint32_t id, const std::type_info& ti) const;
    void     _removeTag(uint32_t id, const std::type_info& ti) const;
    uint32_t _ensureComponentByName(const char* name, std::size_t size, std::size_t align);
    void     _addById(uint32_t entityId, uint32_t compId, const void* data, std::size_t size, std::size_t align);

    void     _setParent(uint32_t id, uint32_t parentId);
    void     _forEachChildOpaque(uint32_t parentId, void(*cb)(void*, uint32_t, Scene*), void* ctx) const;
    void _forEachRootOpaque(void(*cb)(void*, uint32_t, Scene*), void* ctx) const;
    void _forEachComponentOpaque(uint32_t id,
                                 void(*cb)(void*, uint32_t, const char*, const void*, std::size_t, std::size_t, bool),
                                 void* ctx) const;

    System _create_system(const std::vector<const std::type_info*>& compTypes,
                      const std::vector<std::size_t>& sizes,
                      const std::vector<std::size_t>& aligns,
                      void(*cb)(void* ctx, uint32_t eid, void** comps, float dt),
                      void* ctx,
                      bool cascade);

    template<typename Ctx, typename... ComponentsT>
    static void CreateSystem_trampoline(void* ctxptr, uint32_t eid, void** comps, float delta) {
        auto* ctx = static_cast<Ctx*>(ctxptr);
        // Build an Entity with the Scene pointer that will be set by _create_system
        Entity e(ctx->scene, eid);
        // expand comps[] into typed references and call the user's function stored in ctx->fn
        // Use index sequence to unpack
        call_with_index_sequence([&](auto... I){
            ctx->fn(e, *reinterpret_cast<ComponentsT*>(comps[I])..., delta);
        }, std::index_sequence_for<ComponentsT...>{});
    }

    template<typename Callable, size_t... I>
    static void call_with_index_sequence_impl(Callable&& c, std::index_sequence<I...>) {
        c(I...);
    }
    template<typename Callable, size_t... I>
    static void call_with_index_sequence(Callable&& c, std::index_sequence<I...>) {
        call_with_index_sequence_impl(std::forward<Callable>(c), std::index_sequence<I...>{});
    }
    uint32_t _getParentId(uint32_t id) const;

    Impl* _p;
};

inline Entity Scene::Instantiate(const char* name) { return Entity(this, _create(name)); }
inline void   Scene::Destroy(Entity e) { _destroy(e.RawId()); }
inline void   Scene::Update(float dt) { _update(dt); }
inline void   Scene::UpdateComponentRegistry() { _applyRegistry(); }
inline Entity& Entity::SetName(const char* name) { _scene->_setName(_id, name); return *this; }
inline const char* Entity::GetName() const { return _scene->_getName(_id); }
inline Entity& Entity::SetParent(Entity parent) { _scene->_setParent(_id, parent.RawId()); return *this; }
inline int Entity::GetChildCount() const { return _scene->_childCount(_id); }

template<class T> inline Entity& Entity::Add(const T& value) {
    _scene->_addSet(_id, typeid(T), &value, sizeof(T), alignof(T));
    return *this;
}
template<class T> inline Entity& Entity::AddTag() {
    _scene->_addTag(_id, typeid(T));
    return *this;
}
template<class T> inline bool Entity::Has() const { return _scene->_has(_id, typeid(T)); }
template<class T> inline bool Entity::HasTag() const { return _scene->_hasTag(_id, typeid(T)); }
template<class T> inline T* Entity::Get() {
    return static_cast<T*>(_scene->_getMut(_id, typeid(T), sizeof(T), alignof(T)));
}
template<class T> inline const T* Entity::TryGet() const {
    return _scene->_has(_id, typeid(T))
        ? static_cast<const T*>( const_cast<Scene*>(_scene)->_getMut(_id, typeid(T), sizeof(T), alignof(T)) )
        : nullptr;
}
template<class T> inline void Entity::Remove() { _scene->_remove(_id, typeid(T)); }
template<class T> inline void Entity::RemoveTag() { _scene->_removeTag(_id, typeid(T)); }

inline Entity& Entity::AddTransform() {
    if (!Has<LocalTRS>())   Add(LocalTRS{});
    if (!Has<WorldXform>()) Add(WorldXform{});
    return *this;
}
inline Entity& Entity::SetPosition(float x, float y, float z) {
    auto* t = Get<LocalTRS>(); if (!t) { Add(LocalTRS{}); t = Get<LocalTRS>(); }
    t->pos[0]=x; t->pos[1]=y; t->pos[2]=z; ++t->version; return *this;
}
inline Entity& Entity::SetRotationEuler(float rx, float ry, float rz) {
    auto* t = Get<LocalTRS>(); if (!t) { Add(LocalTRS{}); t = Get<LocalTRS>(); }
    t->rot_euler[0]=rx; t->rot_euler[1]=ry; t->rot_euler[2]=rz; ++t->version; return *this;
}
inline Entity& Entity::SetScale(float sx, float sy, float sz) {
    auto* t = Get<LocalTRS>(); if (!t) { Add(LocalTRS{}); t = Get<LocalTRS>(); }
    t->scl[0]=sx; t->scl[1]=sy; t->scl[2]=sz; ++t->version; return *this;
}
inline Entity& Entity::SetScaleUniform(float s) { return SetScale(s,s,s); }

template<class Fn>
inline void Scene::ForEachChild(Entity parent, Fn&& fn) {
    struct Ctx { Fn fn; Scene* self; };
    Ctx ctx{ std::forward<Fn>(fn), this };
    _forEachChildOpaque(parent.RawId(),
        [](void* u, uint32_t childId, Scene* self){
            auto& c = *static_cast<Ctx*>(u);
            c.fn( Entity(self, childId) );
        }, &ctx);
}

template<class Fn>
inline void Scene::ForEachChild(Entity parent, Fn&& fn) const {
    struct Ctx { Fn fn; Scene* self; };
    Ctx ctx{ std::forward<Fn>(fn), const_cast<Scene*>(this) };
    _forEachChildOpaque(parent.RawId(),
        [](void* u, uint32_t childId, Scene* self){
            auto& c = *static_cast<Ctx*>(u);
            c.fn( Entity(self, childId) );
        }, &ctx);
}

template<class Fn>
inline void Scene::ForEachRoot(Fn&& fn) {
    struct Ctx { Fn fn; Scene* self; };
    Ctx ctx{ std::forward<Fn>(fn), this };
    _forEachRootOpaque(
        [](void* u, uint32_t id, Scene* self){
            auto& c = *static_cast<Ctx*>(u);
            c.fn( Entity(self, id) );
        },
        &ctx
    );
}

template<class Fn>
inline void Scene::ForEachRoot(Fn&& fn) const {
    struct Ctx { Fn fn; Scene* self; };
    Ctx ctx{ std::forward<Fn>(fn), const_cast<Scene*>(this) };
    _forEachRootOpaque(
        [](void* u, uint32_t id, Scene* self){
            auto& c = *static_cast<Ctx*>(u);
            c.fn( Entity(self, id) );
        },
        &ctx
    );
}

template<typename... Components, typename Fn>
System Scene::CreateSystem(Fn&& fn, bool cascade) {
    static_assert(sizeof...(Components) > 0, "CreateSystem requires at least one component type.");

    // Build lists of type_info pointers, sizes, aligns
    std::vector<const std::type_info*> types { &typeid(Components)... };
    std::vector<std::size_t> sizes { sizeof(Components)... };
    std::vector<std::size_t> aligns{ alignof(Components)... };

    // Trampoline context type (templated to own the user's Fn)
    using UserFn = std::decay_t<Fn>;
    struct TrampolineCtxBase {
        Scene* scene = nullptr; // will be set by Scene::_create_system
    };
    // Specialized context with function
    struct TrampolineCtx : TrampolineCtxBase {
        UserFn fn;
        TrampolineCtx(UserFn&& f) : fn(std::move(f)) {}
    };
    // Allocate on heap and capture user's fn inside
    auto* ctx = new TrampolineCtx(UserFn(std::forward<Fn>(fn)));

    using OpaqueCb = void(*)(void* ctx, uint32_t eid, void** comps, float dt);
    OpaqueCb cb = &Scene::template CreateSystem_trampoline<TrampolineCtx, Components...>;

    // Call the factory: pass types (type_info*), sizes and aligns.
    return _create_system(types, sizes, aligns, cb, ctx, cascade);
}

inline Entity Scene::Parent(Entity e) const {
    uint32_t pid = _getParentId(e.RawId());
    return pid ? Entity(const_cast<Scene*>(this), pid) : Entity{};
}

inline Entity Scene::FromId(uint32_t id) const
{
    return id ? Entity(const_cast<Scene*>(this), id) : Entity{};
}

template<class Fn>
inline void Scene::ForEachComponent(Entity e, Fn&& fn) const {
    struct Ctx { Fn fn; };
    Ctx ctx{ std::forward<Fn>(fn) };
    _forEachComponentOpaque(e.RawId(),
        [](void* u, uint32_t compId, const char* name, const void* data, std::size_t size, std::size_t align, bool isTag){
            auto& c = *static_cast<Ctx*>(u);
            ComponentView view{};
            view.id = compId;
            view.name = name;
            view.data = data;
            view.size = size;
            view.align = align;
            view.isTag = isTag;
            c.fn(view);
        }, &ctx);
}
