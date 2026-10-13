#include <Mariana/Scene/ECS.hpp>
#include <Mariana/Scene/Prefab.hpp>
#include <flecs.h>
#include <unordered_map>
#include <typeindex>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstring>
#include <cassert>

namespace {



static const ECS::ComponentLifecycle* GetLifecycleFromTypeInfo(const ecs_type_info_t* ti) {
    if (!ti) return nullptr;
    return static_cast<const ECS::ComponentLifecycle*>(ti->hooks.ctx);
}

static void LifecycleCtor(void* ptr, int32_t count, const ecs_type_info_t* ti) {
    if (const ECS::ComponentLifecycle* lc = GetLifecycleFromTypeInfo(ti)) {
        if (lc->ctor) lc->ctor(ptr, count);
    }
}

static void LifecycleDtor(void* ptr, int32_t count, const ecs_type_info_t* ti) {
    if (const ECS::ComponentLifecycle* lc = GetLifecycleFromTypeInfo(ti)) {
        if (lc->dtor) lc->dtor(ptr, count);
    }
}

static void LifecycleCopy(void* dst, const void* src, int32_t count, const ecs_type_info_t* ti) {
    if (const ECS::ComponentLifecycle* lc = GetLifecycleFromTypeInfo(ti)) {
        if (lc->copy) lc->copy(dst, src, count);
    }
}

static void LifecycleMove(void* dst, void* src, int32_t count, const ecs_type_info_t* ti) {
    if (const ECS::ComponentLifecycle* lc = GetLifecycleFromTypeInfo(ti)) {
        if (lc->move) lc->move(dst, src, count);
    }
}

static void LifecycleCopyCtor(void* dst, const void* src, int32_t count, const ecs_type_info_t* ti) {
    if (const ECS::ComponentLifecycle* lc = GetLifecycleFromTypeInfo(ti)) {
        if (lc->copy_ctor) lc->copy_ctor(dst, src, count);
    }
}

static void LifecycleMoveCtor(void* dst, void* src, int32_t count, const ecs_type_info_t* ti) {
    if (const ECS::ComponentLifecycle* lc = GetLifecycleFromTypeInfo(ti)) {
        if (lc->move_ctor) lc->move_ctor(dst, src, count);
    }
}

static ecs_type_hooks_t MakeHooks(const ECS::ComponentLifecycle* lifecycle) {
    ecs_type_hooks_t hooks{};
    if (!lifecycle) return hooks;

    hooks.ctor = &LifecycleCtor;
    hooks.dtor = &LifecycleDtor;
    hooks.copy = &LifecycleCopy;
    hooks.move = &LifecycleMove;
    hooks.copy_ctor = &LifecycleCopyCtor;
    hooks.move_ctor = &LifecycleMoveCtor;

    hooks.flags = ECS_TYPE_HOOK_CTOR |
                  ECS_TYPE_HOOK_DTOR |
                  ECS_TYPE_HOOK_COPY |
                  ECS_TYPE_HOOK_MOVE |
                  ECS_TYPE_HOOK_COPY_CTOR |
                  ECS_TYPE_HOOK_MOVE_CTOR;

    hooks.ctx = const_cast<ECS::ComponentLifecycle*>(lifecycle);
    return hooks;
}


struct RegisteredComponent {
    std::type_index type{typeid(void)};
    std::string name;
    std::size_t size = 0;
    std::size_t align = 0;
    const ECS::ComponentLifecycle* lifecycle = nullptr;
};

std::vector<RegisteredComponent>& GlobalComponentRegistry() {
    static std::vector<RegisteredComponent> registry;
    return registry;
}

const RegisteredComponent* FindRegisteredComponentByType(const std::type_index& type) {
    for (const auto& entry : GlobalComponentRegistry()) {
        if (entry.type == type) return &entry;
    }
    return nullptr;
}

const RegisteredComponent* FindRegisteredComponentByName(const char* name) {
    if (!name || !*name) return nullptr;
    for (const auto& entry : GlobalComponentRegistry()) {
        if (entry.name == name) return &entry;
    }
    return nullptr;
}

static glm::mat4 make_local(const LocalTRS& t) {
    glm::mat4 M(1.f);
    M = glm::translate(M, {t.pos[0], t.pos[1], t.pos[2]});
    M *= glm::mat4_cast(glm::quat(t.rot_quat[3], t.rot_quat[0], t.rot_quat[1], t.rot_quat[2]));
    M = glm::scale(M, {t.scl[0], t.scl[1], t.scl[2]});
    return M;
}
}

struct Scene::Impl {
    flecs::world ecs;
    std::unordered_map<std::type_index, ecs_entity_t> comp;
    struct CompInfo {
        std::size_t size = 0;
        std::size_t align = 0;
    };
    std::unordered_map<ecs_entity_t, CompInfo> comp_info;

    // Component id per ECS::detail::TypeSlot, filled lazily from `comp`. Lets the typed Entity
    // API skip hashing std::type_info on every access.
    std::vector<ecs_entity_t> slot_ids;

    // Cached query for entities without a parent, built on first use (freed with the world).
    ecs_query_t* root_query = nullptr;

    Scene* owner = nullptr;
    uint32_t next_trs_id = 0;

    Impl(Scene* s) : ecs(), owner(s) {}
    Impl() : ecs(), owner(nullptr) {}

    ecs_entity_t cachedSlot(uint32_t slot) const {
        return slot < slot_ids.size() ? slot_ids[slot] : 0;
    }

    void cacheSlot(uint32_t slot, ecs_entity_t cid) {
        if (slot >= slot_ids.size()) slot_ids.resize(slot + 1, 0);
        slot_ids[slot] = cid;
    }

    // Existing component id for a type, or 0 if the scene has never seen it.
    ecs_entity_t findComponent(uint32_t slot, const std::type_info& ti) {
        if (ecs_entity_t cid = cachedSlot(slot)) return cid;
        auto it = comp.find(std::type_index(ti));
        if (it == comp.end()) return 0;
        cacheSlot(slot, it->second);
        return it->second;
    }

    ecs_entity_t ensureComponent(uint32_t slot, const std::type_info& ti, std::size_t sz, std::size_t align) {
        if (ecs_entity_t cid = cachedSlot(slot)) return cid;
        ecs_entity_t cid = ensureComponent(ti, sz, align);
        cacheSlot(slot, cid);
        return cid;
    }

    // changed to accept a component name string (we no longer try to access std::type_info)
    ecs_entity_t ensureComponentByName(const std::string& name, std::size_t sz, std::size_t align, const ECS::ComponentLifecycle* lifecycle) {
        ecs_entity_t existing = ecs_lookup(ecs.c_ptr(), name.c_str());
        if (existing) return existing;

        // if we already have a mapping by name, return it
        // Note: we kept a comp map keyed by type_index in other code. For discoverability we also
        // allow ensureComponentByName to create a new entity for this name.
        // For simplicity: just create new entity with that name (use low id so it matches textual query)
        ecs_entity_desc_t ed{};
        ed.name = name.c_str();
        ed.use_low_id = true;
        ecs_entity_t ent = ecs_entity_init(ecs.c_ptr(), &ed);

        // register it as a component with size and alignment
        ecs_component_desc_t cd{};
        cd.entity = ent;
        cd.type.size = (ecs_size_t)sz;
        cd.type.alignment = (ecs_size_t)align;
        if (lifecycle) {
            cd.type.hooks = MakeHooks(lifecycle);
        }
        ecs_entity_t cid = ecs_component_init(ecs.c_ptr(), &cd);

        comp_info[cid] = { sz, align };
        return cid;
    }

    ecs_entity_t ensureComponent(const std::type_info& ti, std::size_t sz, std::size_t align) {
        auto key = std::type_index(ti);
        if (auto it = comp.find(key); it != comp.end()) return it->second;

        const RegisteredComponent* reg = FindRegisteredComponentByType(key);
        const char* nm = (reg && !reg->name.empty()) ? reg->name.c_str() : ti.name();
        std::size_t use_size = reg ? reg->size : sz;
        std::size_t use_align = reg ? reg->align : align;
        const ECS::ComponentLifecycle* lifecycle = reg ? reg->lifecycle : nullptr;

        ecs_entity_desc_t ed{};
        ed.name = nm;
        ed.use_low_id = true;
        ecs_entity_t ent = ecs_entity_init(ecs.c_ptr(), &ed);

        ecs_component_desc_t cd{};
        cd.entity = ent;
        cd.type.size = (ecs_size_t)use_size;
        cd.type.alignment = (ecs_size_t)use_align;
        if (lifecycle) {
            cd.type.hooks = MakeHooks(lifecycle);
        }
        ecs_entity_t cid = ecs_component_init(ecs.c_ptr(), &cd);

        comp_info[cid] = { use_size, use_align };
        comp.emplace(key, cid);
        return cid;
    }
};

void ApplyGlobalRegistryToScene(Scene::Impl& impl) {
    for (const auto& entry : GlobalComponentRegistry()) {
        ecs_entity_t cid = impl.ensureComponentByName(entry.name, entry.size, entry.align, entry.lifecycle);
        impl.comp[entry.type] = cid;
    }
    // Type -> id mappings may have changed.
    impl.slot_ids.clear();
}

struct System::Impl {
    ecs_world_t* world = nullptr;
    ecs_query_t* query = nullptr;
    // list of component flecs ids (for fallback ecs_get_id)
    std::vector<ecs_entity_t> comp_ids;
    // sizes/aligns (mirrors header's info)
    std::vector<std::size_t> sizes;
    std::vector<std::size_t> aligns;

    // opaque callback that was passed from header (CreateSystem_trampoline)
    void(*cb)(void* ctx, uint64_t eid, void** comps, float delta) = nullptr;
    void* ctx_ptr = nullptr; // heap allocated trampoline context; will be deleted in destructor

    ~Impl() {
        if (query) {
            ecs_query_fini(query);
            query = nullptr;
        }
        if (ctx_ptr) {
            // header allocated a TrampolineCtx using 'new' — delete as void* to the known type
            // we don't know exact type here, but in our design header always allocates a concrete type whose destructor is accessible,
            // so a simple delete on the void* cast to char* is undefined. Instead: we delete as std::function pointer
            // BUT earlier we allocated TrampolineCtx (a small struct). To safely destroy it we must know its type.
            // Safer approach: require header to allocate the context as std::function<void(uint32_t, void**)>*
            // However we used TrampolineCtx struct. To avoid UB here, we will not call delete on ctx_ptr; instead,
            // Scene::_create_system will take ownership of ctx_ptr and will delete it as the correct type.
            // To keep safe, we will assume ctx_ptr is a pointer that Scene::_create_system deletes later.
        }
    }
};

static void run_query_and_call(ecs_world_t* world, ecs_query_t* q, System::Impl* impl, float delta) {
    alignas(std::max_align_t) static unsigned char tag_dummy[sizeof(std::max_align_t)] = {};
    const size_t n = impl->comp_ids.size();

    // Field arrays are resolved once per matched table, then advanced per entity.
    char*  bases[FLECS_TERM_COUNT_MAX];
    size_t strides[FLECS_TERM_COUNT_MAX];
    void*  comps[FLECS_TERM_COUNT_MAX];

    ecs_iter_t it = ecs_query_iter(world, q);
    while (ecs_query_next(&it)) {
        for (size_t j = 0; j < n; ++j) {
            const size_t size = (size_t)ecs_field_size(&it, (int8_t)j);
            if (size == 0) {
                // Tags have no data.
                bases[j] = reinterpret_cast<char*>(tag_dummy);
                strides[j] = 0;
                continue;
            }
            bases[j] = static_cast<char*>(ecs_field_w_size(&it, size, (int8_t)j));
            strides[j] = ecs_field_is_self(&it, (int8_t)j) ? size : 0;
        }

        for (int i = 0; i < it.count; ++i) {
            for (size_t j = 0; j < n; ++j) {
                comps[j] = bases[j] + strides[j] * (size_t)i;
            }
            impl->cb(impl->ctx_ptr, (uint64_t)it.entities[i], comps, delta);
        }
    }
}

Scene::Scene() : _p(new Impl) {
    // Register components so typed APIs work
    auto idName = _p->ecs.component<NameComponent>().set_name("NameComponent").id();
    auto idLocal = _p->ecs.component<LocalTRS>().set_name("LocalTRS").id();
    auto idWorld = _p->ecs.component<WorldXform>().set_name("WorldXform").id();
    _p->comp.emplace(std::type_index(typeid(NameComponent)), idName);
    _p->comp.emplace(std::type_index(typeid(LocalTRS)),   idLocal);
    _p->comp.emplace(std::type_index(typeid(WorldXform)), idWorld);
    _p->comp_info[idName] = { sizeof(NameComponent), alignof(NameComponent) };
    _p->comp_info[idLocal] = { sizeof(LocalTRS), alignof(LocalTRS) };
    _p->comp_info[idWorld] = { sizeof(WorldXform), alignof(WorldXform) };

    auto idCache = _p->ecs.component<XformCache>().set_name("_XformCache").id();
    auto idClock = _p->ecs.component<TransformClock>().set_name("_TransformClock").id();
    _p->comp_info[idCache] = { sizeof(XformCache), alignof(XformCache) };
    _p->comp_info[idClock] = { sizeof(TransformClock), alignof(TransformClock) };
    _p->ecs.set<TransformClock>({1});
    ECS::RegisterBuiltinComponents();

    // Parents are processed before children (cascade), and the parent's WorldXform/XformCache
    // come straight from the query (null for roots) instead of per-entity lookups.
    _p->ecs.system<LocalTRS, WorldXform, XformCache, const WorldXform*, const XformCache*>()
      .term_at(3).cascade(flecs::ChildOf)
      .term_at(4).up(flecs::ChildOf)
      .each([](flecs::entity e, LocalTRS& local, WorldXform& world, XformCache& cache,
               const WorldXform* parent_world, const XformCache* parent_cache)
    {
        const uint32_t parent_ver = parent_cache ? parent_cache->world_v : 0;

        // Skip if nothing changed
        if (cache.local_v == local.version && cache.parent_v == parent_ver) return;

        // Build local (T * R * S written out directly), then compose with parent if present
        glm::mat4 T = glm::mat4_cast(glm::quat(local.rot_quat[3], local.rot_quat[0], local.rot_quat[1], local.rot_quat[2]));
        T[0] *= local.scl[0];
        T[1] *= local.scl[1];
        T[2] *= local.scl[2];
        T[3] = glm::vec4(local.pos[0], local.pos[1], local.pos[2], 1.f);

        glm::mat4 M = T;
        if (parent_world) {
            glm::mat4 P = glm::make_mat4(parent_world->model);
            M = P * T;
        }

        // Write out model and normal
        std::memcpy(world.model,  glm::value_ptr(M), sizeof(float) * 16);
        glm::mat3 N = glm::mat3(glm::transpose(glm::inverse(glm::mat3(M))));
        std::memcpy(world.normal, glm::value_ptr(N), sizeof(float) * 9);
        ++world.version;

        auto& clk = e.world().get_mut<TransformClock>();
        cache.world_v = clk.tick++;
        cache.local_v = local.version;
        cache.parent_v = parent_ver;
    });

    ApplyGlobalRegistryToScene(*_p);
}

Scene::~Scene() { delete _p; }
Scene::Scene(Scene&& o) noexcept : _p(o._p) { o._p = nullptr; }
Scene& Scene::operator=(Scene&& o) noexcept { if (this!=&o){ delete _p; _p=o._p; o._p=nullptr; } return *this; }

uint64_t Scene::_create(const char* name) {
    auto e = _p->ecs.entity();
    auto idName = _p->ensureComponent(ECS::detail::TypeSlot<NameComponent>(), typeid(NameComponent), sizeof(NameComponent), alignof(NameComponent));
    auto idLocal = _p->ensureComponent(ECS::detail::TypeSlot<LocalTRS>(), typeid(LocalTRS),   sizeof(LocalTRS),   alignof(LocalTRS));
    auto idWorld = _p->ensureComponent(ECS::detail::TypeSlot<WorldXform>(), typeid(WorldXform), sizeof(WorldXform), alignof(WorldXform));

    NameComponent nc{ (name && *name) ? name : "Entity" };
    LocalTRS   lt{};
    WorldXform wx{};
    wx.id = _p->next_trs_id++;
    ecs_set_id(_p->ecs.c_ptr(), e.id(), idName,  sizeof(NameComponent), &nc);
    ecs_set_id(_p->ecs.c_ptr(), e.id(), idLocal, sizeof(LocalTRS),   &lt);
    ecs_set_id(_p->ecs.c_ptr(), e.id(), idWorld, sizeof(WorldXform), &wx);

    e.set<XformCache>({});

    return (uint64_t)e.id();
}

Entity Scene::Instantiate(const Prefab& prefab, const char* name) {
    if (!prefab.IsValid()) return Entity{};

    uint64_t newRootId = Prefab::CloneEntityRecursive(prefab._scene, *this, prefab._rootId, 0);
    Entity root = FromId(newRootId);
    if (!root.IsValid()) return Entity{};
    if (name && *name) {
        root.SetName(name);
    }
    return root;
}

void Scene::_destroy(uint64_t id) {
    if (!id || !_isAlive(id)) return;

    std::vector<uint64_t> children;
    _forEachChildOpaque(id,
        [](void* ctx, uint64_t childId, Scene*) {
            static_cast<std::vector<uint64_t>*>(ctx)->push_back(childId);
        },
        &children);

    for (uint64_t childId : children) {
        _destroy(childId);
    }

    _p->ecs.entity((flecs::entity_t)id).destruct();
}
void Scene::_update(float) { _p->ecs.progress(); }
bool Scene::_isAlive(uint64_t id) const { return id && ecs_is_alive(_p->ecs.c_ptr(), (ecs_entity_t)id); }
uint64_t Scene::_getAliveId(uint64_t id) const {
    if (!id) return 0;
    ecs_world_t* w = _p->ecs.c_ptr();
    ecs_entity_t e = (ecs_entity_t)id;
    if (!ecs_is_alive(w, e)) {
        e = ecs_get_alive(w, e);
    }
    return ecs_is_alive(w, e) ? (uint64_t)e : 0;
}
void Scene::_setName(uint64_t id, const char* name) {
    if (!_isAlive(id)) return;
    ecs_entity_t cid = _p->ensureComponent(ECS::detail::TypeSlot<NameComponent>(), typeid(NameComponent), sizeof(NameComponent), alignof(NameComponent));
    NameComponent* current = static_cast<NameComponent*>(ecs_get_mut_id(_p->ecs.c_ptr(), (ecs_entity_t)id, cid));
    if (current && current->name == (name ? name : "")) return;
    NameComponent nc{ name ? name : "" };
    ecs_set_id(_p->ecs.c_ptr(), (ecs_entity_t)id, cid, sizeof(NameComponent), &nc);
}
const char* Scene::_getName(uint64_t eid) const {
    if (!_isAlive(eid)) return "";
    ecs_entity_t cid = _p->findComponent(ECS::detail::TypeSlot<NameComponent>(), typeid(NameComponent));
    if (!cid) return "";
    const NameComponent* name = static_cast<const NameComponent*>(ecs_get_id(_p->ecs.c_ptr(), (ecs_entity_t)eid, cid));
    return name ? name->name.c_str() : "";
}

bool Scene::_has(uint64_t id, uint32_t slot, const std::type_info& ti) const {
    if (!_isAlive(id)) return false;
    ecs_entity_t cid = _p->findComponent(slot, ti);
    return cid && ecs_has_id(_p->ecs.c_ptr(), (ecs_entity_t)id, cid);
}
void* Scene::_getMut(uint64_t id, uint32_t slot, const std::type_info& ti, std::size_t sz, std::size_t align) const {
    if (!_isAlive(id)) return nullptr;
    ecs_entity_t cid = _p->ensureComponent(slot, ti, sz, align);
    return ecs_get_mut_id(_p->ecs.c_ptr(), (ecs_entity_t)id, cid);
}
const void* Scene::_tryGet(uint64_t id, uint32_t slot, const std::type_info& ti) const {
    if (!_isAlive(id)) return nullptr;
    ecs_entity_t cid = _p->findComponent(slot, ti);
    return cid ? ecs_get_id(_p->ecs.c_ptr(), (ecs_entity_t)id, cid) : nullptr;
}
void Scene::_addSet(uint64_t id, uint32_t slot, const std::type_info& ti, const void* data, std::size_t sz, std::size_t align) {
    if (!_isAlive(id)) return;
    ecs_entity_t cid = _p->ensureComponent(slot, ti, sz, align);
    if (ti == typeid(WorldXform) && data && sz == sizeof(WorldXform)) {
        WorldXform temp = *static_cast<const WorldXform*>(data);
        const WorldXform* existing = static_cast<const WorldXform*>(
            ecs_get_id(_p->ecs.c_ptr(), (ecs_entity_t)id, cid)
        );
        temp.id = existing ? existing->id : _p->next_trs_id++;
        ecs_set_id(_p->ecs.c_ptr(), (ecs_entity_t)id, cid, sz, &temp);
        return;
    }
    ecs_set_id(_p->ecs.c_ptr(), (ecs_entity_t)id, cid, sz, data);
}
void Scene::_remove(uint64_t id, uint32_t slot, const std::type_info& ti) const {
    if (!_isAlive(id)) return;
    ecs_entity_t cid = _p->findComponent(slot, ti);
    if (!cid) return;
    ecs_remove_id(_p->ecs.c_ptr(), (ecs_entity_t)id, cid);
}

void Scene::_addTag(uint64_t id, uint32_t slot, const std::type_info& ti) {
    if (!_isAlive(id)) return;
    ecs_entity_t cid = _p->ensureComponent(slot, ti, 0, 0);
    ecs_add_id(_p->ecs.c_ptr(), (ecs_entity_t)id, cid);
}

bool Scene::_hasTag(uint64_t id, uint32_t slot, const std::type_info& ti) const {
    return _has(id, slot, ti);
}

void Scene::_removeTag(uint64_t id, uint32_t slot, const std::type_info& ti) const {
    _remove(id, slot, ti);
}

uint64_t Scene::_ensureComponentByName(const char* name, std::size_t size, std::size_t align) {
    const char* resolved = (name && *name) ? name : "UnnamedComponent";
    const RegisteredComponent* reg = FindRegisteredComponentByName(resolved);
    ecs_entity_t cid = _p->ensureComponentByName(resolved, size, align, reg ? reg->lifecycle : nullptr);
    return (uint64_t)cid;
}

void Scene::_addById(uint64_t entityId, uint64_t compId, const void* data, std::size_t size, std::size_t /*align*/) {
    if (!_isAlive(entityId) || !compId) return;
    ecs_world_t* w = _p->ecs.c_ptr();
    ecs_entity_t e = (ecs_entity_t)entityId;
    ecs_entity_t c = (ecs_entity_t)compId;
    if (size == 0 || data == nullptr) {
        ecs_add_id(w, e, c);
        return;
    }
    auto it = _p->comp.find(std::type_index(typeid(WorldXform)));
    if (it != _p->comp.end() && (ecs_entity_t)it->second == c && size == sizeof(WorldXform)) {
        WorldXform temp = *static_cast<const WorldXform*>(data);
        const WorldXform* existing = static_cast<const WorldXform*>(
            ecs_get_id(w, e, c)
        );
        temp.id = existing ? existing->id : _p->next_trs_id++;
        ecs_set_id(w, e, c, size, &temp);
        return;
    }
    ecs_set_id(w, e, c, size, data);
}

void Scene::_setParent(uint64_t id, uint64_t parentId) {
    if (!_isAlive(id) || (parentId && !_isAlive(parentId))) return;
    ecs_world_t* w = _p->ecs.c_ptr();
    ecs_entity_t e = (ecs_entity_t)id;
    ecs_remove_pair(w, e, EcsChildOf, EcsWildcard);
    if (parentId) ecs_add_pair(w, e, EcsChildOf, (ecs_entity_t)parentId);
}
int Scene::_childCount(uint64_t parentId) const {
    if (!_isAlive(parentId)) return 0;
    ecs_world_t* w = _p->ecs.c_ptr();
    return (int)ecs_count_id(w, ecs_pair(EcsChildOf, (ecs_entity_t)parentId));
}
void Scene::_forEachChildOpaque(uint64_t parentId, void(*cb)(void*, uint64_t, Scene*), void* ctx) const {
    if (!_isAlive(parentId)) return;
    flecs::entity parent(_p->ecs, (ecs_entity_t)parentId);
    parent.children([&](flecs::entity child){
        if (ecs_is_alive(_p->ecs.c_ptr(), child.id())) {
            cb(ctx, (uint64_t)child.id(), const_cast<Scene*>(this));
        }
    });
}
void Scene::_forEachRootOpaque(void(*cb)(void*, uint64_t, Scene*), void* ctx) const {
    ecs_world_t* w = _p->ecs.c_ptr();

    if (!_p->root_query) {
        // Entities with a transform and no parent. Cached, so it isn't rebuilt every call.
        ecs_query_desc_t qd{};
        qd.terms[0].id = _p->ecs.component<LocalTRS>().id();
        qd.terms[1].id = ecs_pair(EcsChildOf, EcsWildcard);
        qd.terms[1].oper = EcsNot;
        qd.cache_kind = EcsQueryCacheAuto;
        _p->root_query = ecs_query_init(w, &qd);
    }

    ecs_iter_t it = ecs_query_iter(w, _p->root_query);
    while (ecs_query_next(&it)) {
        for (int i = 0; i < it.count; ++i) {
            cb(ctx, (uint64_t)it.entities[i], const_cast<Scene*>(this));
        }
    }
}

void Scene::_forEachComponentOpaque(uint64_t id,
                                    void(*cb)(void*, uint64_t, const char*, const void*, std::size_t, std::size_t, bool),
                                    void* ctx) const {
    ecs_world_t* w = _p->ecs.c_ptr();
    if (!_isAlive(id)) return;
    ecs_entity_t ent = (ecs_entity_t)id;
    const ecs_type_t* type = ecs_get_type(w, ent);
    if (!type) return;

    const ecs_id_t* ids = type->array;
    int32_t count = type->count;
    for (int32_t i = 0; i < count; ++i) {
        ecs_id_t cid = ids[i];
        if (ecs_id_is_pair(cid)) {
            continue;
        }

        ecs_entity_t compEnt = (ecs_entity_t)cid;
        const char* name = ecs_get_name(w, compEnt);
        std::size_t size = 0;
        std::size_t align = 0;
        bool isTag = false;
        const void* data = nullptr;

#if defined(FLECS_COMPONENT) && defined(EcsComponent)
        const ecs_component_t* comp = ecs_get(w, compEnt, EcsComponent);
        isTag = !comp || comp->size == 0;
        size = comp ? (std::size_t)comp->size : 0;
        align = comp ? (std::size_t)comp->alignment : 0;
        if (!isTag) {
            data = ecs_get_id(w, ent, cid);
        }
#elif defined(FLECS_TYPE_INFO)
        const ecs_type_info_t* ti = ecs_get_type_info(w, compEnt);
        isTag = !ti || ti->size == 0;
        size = ti ? (std::size_t)ti->size : 0;
        align = ti ? (std::size_t)ti->alignment : 0;
        if (!isTag) {
            data = ecs_get_id(w, ent, cid);
        }
#else
        auto it = _p->comp_info.find(compEnt);
        if (it != _p->comp_info.end()) {
            size = it->second.size;
            align = it->second.align;
        }
        data = ecs_get_id(w, ent, cid);
        isTag = (size == 0) || (data == nullptr);
#endif
        if (size == 0) {
            auto it = _p->comp_info.find(compEnt);
            if (it != _p->comp_info.end()) {
                size = it->second.size;
                align = it->second.align;
                if (!data && size > 0) {
                    data = ecs_get_id(w, ent, cid);
                }
                if (size > 0) {
                    isTag = (data == nullptr);
                }
            }
        }

        cb(ctx, (uint64_t)compEnt, name, data, size, align, isTag);
    }
}

void Scene::_applyRegistry() {
    ApplyGlobalRegistryToScene(*_p);
}

System Scene::_create_system(const std::vector<const std::type_info*>& compTypes,
                             const std::vector<std::size_t>& sizes,
                             const std::vector<std::size_t>& aligns,
                             void(*cb)(void* ctx, uint64_t eid, void** comps, float dt),
                             void* ctx,
                             bool /*cascade*/)
{
    // create the Impl and set up the query
    auto* simpl = new System::Impl();
    simpl->world = _p->ecs.c_ptr();
    simpl->cb = cb;
    simpl->ctx_ptr = ctx;
    simpl->sizes = sizes;
    simpl->aligns = aligns;

ecs_term_t terms[FLECS_TERM_COUNT_MAX];
std::memset(terms, 0, sizeof(terms));

size_t term_count = compTypes.size();
if(term_count > FLECS_TERM_COUNT_MAX) {
    // handle error, too many components
    term_count = FLECS_TERM_COUNT_MAX;
}

for(size_t i = 0; i < term_count; ++i) {
    const std::type_info& ti = *compTypes[i];
    std::type_index tix(ti);

    ecs_entity_t cid = 0;
    auto it = _p->comp.find(tix);
    if(it != _p->comp.end()) {
        cid = it->second;
    } else {
        cid = _p->ensureComponent(ti, sizes[i], aligns[i]);
    }

    simpl->comp_ids.push_back(cid);
    if (auto info_it = _p->comp_info.find(cid); info_it != _p->comp_info.end()) {
        simpl->sizes[i] = info_it->second.size;
        simpl->aligns[i] = info_it->second.align;
    }

    // Fill the term struct directly
    ecs_term_t& term = terms[i];
    std::memset(&term, 0, sizeof(term));
    term.id = cid;      // component ID
    term.oper = EcsAnd; // normal AND operation
}
    // Build query using textual expression composed from actual registered names
ecs_query_desc_t qd{};
qd.expr = NULL; // no textual expression
qd.cache_kind = EcsQueryCacheAuto; // systems run every frame, so keep the matched tables cached
qd.flags = 0;

// Copy the fixed array into qd.terms
for(size_t i = 0; i < term_count; ++i) {
    qd.terms[i] = terms[i];
}
    ecs_query_t* q = ecs_query_init(simpl->world, &qd);
    if (!q) {
        delete simpl;
        return System();
    }
    simpl->query = q;

    // set the Scene* into the trampoline context so the header's Entity(ctx->scene, eid) is valid.
    struct TrampolineCtxBase { Scene* scene; };
    if (ctx) {
        auto* base = reinterpret_cast<TrampolineCtxBase*>(ctx);
        base->scene = this;
    }

    return System(simpl);
}

System::~System() {
    if (_p) {
        delete _p;
        _p = nullptr;
    }
}

System::System(System&& o) noexcept : _p(o._p) { o._p = nullptr; }
System& System::operator=(System&& o) noexcept {
    if (this != &o) {
        if (_p) delete _p;
        _p = o._p;
        o._p = nullptr;
    }
    return *this;
}

void System::Run(float delta) {
    if (!_p || !_p->query || !_p->world) return;
    run_query_and_call(_p->world, _p->query, _p, delta);
}

uint64_t Scene::_getParentId(uint64_t id) const {
    if (!_isAlive(id)) return 0;
    return (uint64_t)ecs_get_target(_p->ecs.c_ptr(), (ecs_entity_t)id, EcsChildOf, 0);
}

namespace ECS {
namespace detail {
uint32_t NextTypeSlot() {
    static uint32_t next = 0;
    return next++;
}
} // namespace detail

void RegisterComponent(const std::type_info& ti, std::size_t size, std::size_t align, const char* name) {
    RegisterComponent(ti, size, align, name, nullptr);
}

void RegisterBuiltinComponents() {
    static bool registered = false;
    if (registered)
        return;
    registered = true;
    RegisterComponent<NameComponent>("NameComponent");
    RegisterComponent<LocalTRS>("LocalTRS");
    RegisterComponent<WorldXform>("WorldXform");
    RegisterComponent<TransformClock>("TransformClock");
    RegisterComponent<XformCache>("XformCache");
}

void RegisterComponent(const std::type_info& ti, std::size_t size, std::size_t align, const char* name, const ComponentLifecycle* lifecycle) {
    std::vector<RegisteredComponent>& registry = GlobalComponentRegistry();
    std::type_index key(ti);
    const char* resolvedName = (name && *name) ? name : ti.name();

    for (auto& entry : registry) {
        if (entry.type == key) {
            if (name && *name) {
                entry.name = resolvedName;
            }
            if (lifecycle) {
                entry.lifecycle = lifecycle;
            }
            return;
        }
    }

    RegisteredComponent entry{};
    entry.type = key;
    entry.name = resolvedName;
    entry.size = size;
    entry.align = align;
    entry.lifecycle = lifecycle;
    registry.push_back(entry);
}

void RegisterTag(const std::type_info& ti, const char* name) {
    std::vector<RegisteredComponent>& registry = GlobalComponentRegistry();
    std::type_index key(ti);
    const char* resolvedName = (name && *name) ? name : ti.name();

    for (auto& entry : registry) {
        if (entry.type == key) {
            if (name && *name) {
                entry.name = resolvedName;
            }
            entry.size = 0;
            entry.align = 0;
            entry.lifecycle = nullptr;
            return;
        }
    }

    RegisteredComponent entry{};
    entry.type = key;
    entry.name = resolvedName;
    entry.size = 0;
    entry.align = 0;
    entry.lifecycle = nullptr;
    registry.push_back(entry);
}
} // namespace ECS
