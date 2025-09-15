#include <ECS.hpp>
#include <flecs.h>
#include <unordered_map>

struct Scene::Impl {
    flecs::world ecs;
    std::unordered_map<std::type_index, flecs::id_t> comp;

    flecs::id_t ensureComponent(const std::type_info& ti,
                                std::size_t sz, std::size_t align) {
        auto key = std::type_index(ti);
        if (auto it = comp.find(key); it != comp.end()) return it->second;

        ecs_entity_desc_t edesc{};
        edesc.name      = ti.name();    
        edesc.use_low_id = true;       
        ecs_entity_t comp_entity = ecs_entity_init(ecs.c_ptr(), &edesc);

        ecs_component_desc_t cdesc{};
        cdesc.entity         = comp_entity;
        cdesc.type.size      = sz;
        cdesc.type.alignment = align;

        ecs_entity_t cid = ecs_component_init(ecs.c_ptr(), &cdesc);
        comp.emplace(key, cid);
        return cid;
    }
};

Scene::Scene() : _p(new Impl) { }
Scene::~Scene() { delete _p; }
Scene::Scene(Scene&& o) noexcept : _p(o._p) { o._p = nullptr; }
Scene& Scene::operator=(Scene&& o) noexcept { if (this!=&o){ delete _p; _p=o._p; o._p=nullptr; } return *this; }

uint64_t Scene::_create(const char* name) {
    auto e = _p->ecs.entity();
    if (name && *name) e.set_name(name);
    return static_cast<uint64_t>(e.id());
}

void Scene::_destroy(uint64_t eid) {
    _p->ecs.entity((flecs::entity_t)eid).destruct();
}

void Scene::_update(float /*dt*/) {
    _p->ecs.progress(); // in Step 1 we don’t run any systems yet
}

void Scene::_setName(uint64_t eid, const char* name) {
    _p->ecs.entity((flecs::entity_t)eid).set_name(name ? name : "");
}

bool Scene::_has(uint64_t eid, const std::type_info& ti) const {
    auto it = _p->comp.find(std::type_index(ti));
    if (it == _p->comp.end()) return false;
    return ecs_has_id(_p->ecs.c_ptr(), (ecs_entity_t)eid, it->second);
}

void* Scene::_getMut(uint64_t eid, const std::type_info& ti,
                     std::size_t sz, std::size_t align) const {
    auto cid = _p->ensureComponent(ti, sz, align);
    return ecs_get_mut_id(_p->ecs.c_ptr(), (ecs_entity_t)eid, cid); // ← v4 C API
}

void Scene::_addSet(uint64_t eid, const std::type_info& ti,
                    const void* data, std::size_t sz, std::size_t align) {
    auto cid = _p->ensureComponent(ti, sz, align);
    ecs_set_id(_p->ecs.c_ptr(), (ecs_entity_t)eid, cid, sz, data);  // ← v4 C API
}

void Scene::_remove(uint64_t eid, const std::type_info& ti) const {
    auto it = _p->comp.find(std::type_index(ti));
    if (it == _p->comp.end()) return;
    ecs_remove_id(_p->ecs.c_ptr(), (ecs_entity_t)eid, it->second);  // ← v4 C API
}

void Scene::_setParent(uint64_t id, uint64_t parentId) {
    ecs_world_t* w = _p->ecs.c_ptr();
    ecs_entity_t e = (ecs_entity_t)id;
    ecs_entity_t p = (ecs_entity_t)parentId;

    // Remove any existing parent (use wildcard, not 0)
    ecs_remove_pair(w, e, EcsChildOf, EcsWildcard);

    // If caller passed a valid parent, add it; if parentId==0, this just detaches.
    if (p) {
        ecs_add_pair(w, e, EcsChildOf, p);
    }
}

void Scene::_forEachChildOpaque(uint64_t parentId,
                                void(*cb)(void*, uint64_t, Scene*),
                                void* ctx) const
{
    // Using the C++ helper here is simplest & fast; stays hidden in .cpp
    flecs::entity parent(_p->ecs, (ecs_entity_t)parentId);
    parent.children([&](flecs::entity child){
        cb(ctx, (uint64_t)child.id(), const_cast<Scene*>(this));
    });
}

uint64_t Scene::_getParentId(uint64_t id) const {
    ecs_world_t* w = _p->ecs.c_ptr();
    ecs_entity_t e = (ecs_entity_t)id;
    ecs_entity_t p = ecs_get_target(w, e, EcsChildOf, 0); // direct parent (or 0)
    return (uint64_t)p;
}

const char* Scene::_getName(uint64_t eid) const {
    return ecs_get_name(_p->ecs.c_ptr(), (ecs_entity_t)eid);
}