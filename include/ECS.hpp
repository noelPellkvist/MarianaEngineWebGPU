#pragma once
#include <cstdint>
#include <typeinfo>
#include <typeindex>

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
};

class Scene;

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
    template<class T> bool      Has() const;
    template<class T> T*        Get();
    template<class T> const T*  TryGet() const;
    template<class T> void      Remove();

    uint64_t RawId() const { return _id; }

private:
    friend class Scene;
    Entity(Scene* s, uint64_t id) : _scene(s), _id(id) {}
    Scene* _scene = nullptr;
    uint64_t _id = 0;
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
    void Destroy(Entity e);
    void Update(float dt);

    template<class Fn>
    void ForEachChild(Entity parent, Fn&& fn);

    template<class Fn>
    void ForEachRoot(Fn&& fn);

    Entity Parent(Entity e) const;

private:
    friend class Entity;

    uint64_t _create(const char* name);
    void     _destroy(uint64_t eid);
    void     _update(float dt);
    void     _setName(uint64_t eid, const char* name);
    const char* _getName(uint64_t eid) const;
    int _childCount(uint64_t parentId) const;

    bool     _has(uint64_t id, const std::type_info& ti) const;
    void*    _getMut(uint64_t id, const std::type_info& ti, std::size_t size, std::size_t align) const;
    void     _addSet(uint64_t id, const std::type_info& ti, const void* data, std::size_t size, std::size_t align);
    void     _remove(uint64_t id, const std::type_info& ti) const;

    void     _setParent(uint64_t id, uint64_t parentId);
    void     _forEachChildOpaque(uint64_t parentId, void(*cb)(void*, uint64_t, Scene*), void* ctx) const;
    void _forEachRootOpaque(void(*cb)(void*, uint64_t, Scene*), void* ctx) const;
    uint64_t _getParentId(uint64_t id) const;

    Impl* _p;
};

inline Entity Scene::Instantiate(const char* name) { return Entity(this, _create(name)); }
inline void   Scene::Destroy(Entity e) { _destroy(e.RawId()); }
inline void   Scene::Update(float dt) { _update(dt); }
inline Entity& Entity::SetName(const char* name) { _scene->_setName(_id, name); return *this; }
inline const char* Entity::GetName() const { return _scene->_getName(_id); }
inline Entity& Entity::SetParent(Entity parent) { _scene->_setParent(_id, parent.RawId()); return *this; }
inline int Entity::GetChildCount() const { return _scene->_childCount(_id); }

template<class T> inline Entity& Entity::Add(const T& value) {
    _scene->_addSet(_id, typeid(T), &value, sizeof(T), alignof(T));
    return *this;
}
template<class T> inline bool Entity::Has() const { return _scene->_has(_id, typeid(T)); }
template<class T> inline T* Entity::Get() {
    return static_cast<T*>(_scene->_getMut(_id, typeid(T), sizeof(T), alignof(T)));
}
template<class T> inline const T* Entity::TryGet() const {
    return _scene->_has(_id, typeid(T))
        ? static_cast<const T*>( const_cast<Scene*>(_scene)->_getMut(_id, typeid(T), sizeof(T), alignof(T)) )
        : nullptr;
}
template<class T> inline void Entity::Remove() { _scene->_remove(_id, typeid(T)); }

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
        [](void* u, uint64_t childId, Scene* self){
            auto& c = *static_cast<Ctx*>(u);
            c.fn( Entity(self, childId) );
        }, &ctx);
}

template<class Fn>
inline void Scene::ForEachRoot(Fn&& fn) {
    struct Ctx { Fn fn; Scene* self; };
    Ctx ctx{ std::forward<Fn>(fn), this };
    _forEachRootOpaque(
        [](void* u, uint64_t id, Scene* self){
            auto& c = *static_cast<Ctx*>(u);
            c.fn( Entity(self, id) );
        },
        &ctx
    );
}

inline Entity Scene::Parent(Entity e) const {
    uint64_t pid = _getParentId(e.RawId());
    return pid ? Entity(const_cast<Scene*>(this), pid) : Entity{};
}
