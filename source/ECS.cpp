#include <ECS.hpp>
#include <Prefab.hpp>
#include <flecs.h>
#include <unordered_map>
#include <typeindex>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
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
    M *= glm::eulerAngleXYZ(glm::radians(t.rot_euler[0]), glm::radians(t.rot_euler[2]), glm::radians(t.rot_euler[2]));
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

    Scene* owner = nullptr;
    uint32_t next_trs_id = 0;

    Impl(Scene* s) : ecs(), owner(s) {}
    Impl() : ecs(), owner(nullptr) {}

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
    void(*cb)(void* ctx, uint32_t eid, void** comps, float delta) = nullptr;
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
    ecs_iter_t it = ecs_query_iter(world, q);
    while (ecs_query_next(&it)) {
        // for each matched entity in this batch:
        for (int i = 0; i < it.count; ++i) {
            ecs_entity_t e = it.entities[i];
            // Build comps array
            size_t n = impl->comp_ids.size();
            std::vector<void*> comps(n, nullptr);
            bool ok = true;
            for (size_t j = 0; j < n; ++j) {
                if (impl->sizes[j] == 0) {
                    comps[j] = tag_dummy;
                    continue;
                }
                void* base = nullptr;
                if (it.ptrs && it.ptrs[j]) {
                    // compute per-entity address
                    size_t stride = (it.sizes && it.sizes[j] ? (size_t)it.sizes[j] : impl->sizes[j]);
                    base = static_cast<char*>(it.ptrs[j]) + (size_t)i * stride;
                } else {
                    // fallback: get pointer via ecs_get_id
                    base = ecs_get_mut_id(world, e, impl->comp_ids[j]);
                    if (!base) { ok = false; break; }
                }
                comps[j] = base;
            }
            if (!ok) continue;

            // call callback with comps.data()
            impl->cb(impl->ctx_ptr, (uint32_t)e, comps.data(), delta);
        }
    }
}

Scene::Scene() : _p(new Impl) {
    // Register components so typed APIs work
    auto idLocal = _p->ecs.component<LocalTRS>().set_name("LocalTRS").id();
    auto idWorld = _p->ecs.component<WorldXform>().set_name("WorldXform").id();
    _p->comp.emplace(std::type_index(typeid(LocalTRS)),   idLocal);
    _p->comp.emplace(std::type_index(typeid(WorldXform)), idWorld);
    _p->comp_info[idLocal] = { sizeof(LocalTRS), alignof(LocalTRS) };
    _p->comp_info[idWorld] = { sizeof(WorldXform), alignof(WorldXform) };

    auto idCache = _p->ecs.component<XformCache>().set_name("_XformCache").id();
    auto idClock = _p->ecs.component<TransformClock>().set_name("_TransformClock").id();
    _p->comp_info[idCache] = { sizeof(XformCache), alignof(XformCache) };
    _p->comp_info[idClock] = { sizeof(TransformClock), alignof(TransformClock) };
    _p->ecs.set<TransformClock>({1});

        // Only two typed terms; process parents before children
    _p->ecs.system<LocalTRS, WorldXform>()
      .term().cascade(flecs::ChildOf)
      .each([this](flecs::entity e, LocalTRS& local, WorldXform& world)
    {
        // Ensure we have a cache and take it by reference
        if (!e.has<XformCache>()) { e.set<XformCache>({}); }
        XformCache& cache = e.get_mut<XformCache>();

        // Read parent's world version (if any), and get pointer to parent's WorldXform via C API
        uint32_t parent_ver = 0;
        const WorldXform* parent_world = nullptr;
        if (flecs::entity p = e.target(flecs::ChildOf)) {
            // v4: get<T>() → const T& (not T*)
            if (p.has<XformCache>()) {
                const XformCache& pc = p.get<XformCache>();
                parent_ver = pc.world_v;
            }
        
            // Keep using C API for parent's WorldXform pointer
            auto wptr  = _p->ecs.c_ptr();
            auto wx_id = _p->comp.at(std::type_index(typeid(WorldXform)));
            parent_world = static_cast<const WorldXform*>(ecs_get_id(wptr, p.id(), wx_id));
        }

        // Skip if nothing changed
        if (cache.local_v == local.version && cache.parent_v == parent_ver) return;

        // Build local, then compose with parent if present
        glm::mat4 T(1.f);
        T = glm::translate(T, {local.pos[0], local.pos[1], local.pos[2]});
        T *= glm::eulerAngleXYZ(glm::radians(local.rot_euler[0]), glm::radians(local.rot_euler[1]), glm::radians(local.rot_euler[2]));
        T = glm::scale(T, {local.scl[0], local.scl[1], local.scl[2]});

        glm::mat4 M = T;
        if (parent_world) {
            glm::mat4 P = glm::make_mat4(parent_world->model);
            M = P * T;
        }

        // Write out model and normal
        std::memcpy(world.model,  glm::value_ptr(M), sizeof(float) * 16);
        glm::mat3 N = glm::mat3(glm::transpose(glm::inverse(glm::mat3(M))));
        std::memcpy(world.normal, glm::value_ptr(N), sizeof(float) * 9);

        auto& clk = _p->ecs.get_mut<TransformClock>();
        cache.world_v = clk.tick++;
        cache.local_v = local.version;
        cache.parent_v = parent_ver;
    });

    ApplyGlobalRegistryToScene(*_p);
}

Scene::~Scene() { delete _p; }
Scene::Scene(Scene&& o) noexcept : _p(o._p) { o._p = nullptr; }
Scene& Scene::operator=(Scene&& o) noexcept { if (this!=&o){ delete _p; _p=o._p; o._p=nullptr; } return *this; }

uint32_t Scene::_create(const char* name) {
    auto e = _p->ecs.entity();
    if (name && *name) e.set_name(name);

    auto idLocal = _p->ensureComponent(typeid(LocalTRS),   sizeof(LocalTRS),   alignof(LocalTRS));
    auto idWorld = _p->ensureComponent(typeid(WorldXform), sizeof(WorldXform), alignof(WorldXform));

    LocalTRS   lt{};
    WorldXform wx{};
    wx.id = _p->next_trs_id++;
    ecs_set_id(_p->ecs.c_ptr(), e.id(), idLocal, sizeof(LocalTRS),   &lt);
    ecs_set_id(_p->ecs.c_ptr(), e.id(), idWorld, sizeof(WorldXform), &wx);

    e.set<XformCache>({});

    return (uint32_t)e.id();
}

Entity Scene::Instantiate(const Prefab& prefab, const char* name) {
    if (!prefab.IsValid()) return Entity{};

    uint32_t newRootId = Prefab::CloneEntityRecursive(prefab._scene, *this, prefab._rootId, 0);
    Entity root = FromId(newRootId);
    if (name && *name) {
        root.SetName(name);
    }
    return root;
}

void Scene::_destroy(uint32_t id) { _p->ecs.entity((flecs::entity_t)id).destruct(); }
void Scene::_update(float) { _p->ecs.progress(); }
void Scene::_setName(uint32_t id, const char* name) { _p->ecs.entity((flecs::entity_t)id).set_name(name ? name : ""); }
const char* Scene::_getName(uint32_t eid) const { return ecs_get_name(_p->ecs.c_ptr(), (ecs_entity_t)eid); }

bool Scene::_has(uint32_t id, const std::type_info& ti) const {
    auto it = _p->comp.find(std::type_index(ti));
    if (it == _p->comp.end()) return false;
    return ecs_has_id(_p->ecs.c_ptr(), (ecs_entity_t)id, it->second);
}
void* Scene::_getMut(uint32_t id, const std::type_info& ti, std::size_t sz, std::size_t align) const {
    ecs_entity_t cid = _p->ensureComponent(ti, sz, align);
    return ecs_get_mut_id(_p->ecs.c_ptr(), (ecs_entity_t)id, cid);
}
void Scene::_addSet(uint32_t id, const std::type_info& ti, const void* data, std::size_t sz, std::size_t align) {
    ecs_entity_t cid = _p->ensureComponent(ti, sz, align);
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
void Scene::_remove(uint32_t id, const std::type_info& ti) const {
    auto it = _p->comp.find(std::type_index(ti));
    if (it == _p->comp.end()) return;
    ecs_remove_id(_p->ecs.c_ptr(), (ecs_entity_t)id, it->second);
}

void Scene::_addTag(uint32_t id, const std::type_info& ti) {
    ecs_entity_t cid = _p->ensureComponent(ti, 0, 0);
    ecs_add_id(_p->ecs.c_ptr(), (ecs_entity_t)id, cid);
}

bool Scene::_hasTag(uint32_t id, const std::type_info& ti) const {
    auto it = _p->comp.find(std::type_index(ti));
    if (it == _p->comp.end()) return false;
    return ecs_has_id(_p->ecs.c_ptr(), (ecs_entity_t)id, it->second);
}

void Scene::_removeTag(uint32_t id, const std::type_info& ti) const {
    auto it = _p->comp.find(std::type_index(ti));
    if (it == _p->comp.end()) return;
    ecs_remove_id(_p->ecs.c_ptr(), (ecs_entity_t)id, it->second);
}

uint32_t Scene::_ensureComponentByName(const char* name, std::size_t size, std::size_t align) {
    const char* resolved = (name && *name) ? name : "UnnamedComponent";
    const RegisteredComponent* reg = FindRegisteredComponentByName(resolved);
    ecs_entity_t cid = _p->ensureComponentByName(resolved, size, align, reg ? reg->lifecycle : nullptr);
    return (uint32_t)cid;
}

void Scene::_addById(uint32_t entityId, uint32_t compId, const void* data, std::size_t size, std::size_t /*align*/) {
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

void Scene::_setParent(uint32_t id, uint32_t parentId) {
    ecs_world_t* w = _p->ecs.c_ptr();
    ecs_entity_t e = (ecs_entity_t)id;
    ecs_remove_pair(w, e, EcsChildOf, EcsWildcard);
    if (parentId) ecs_add_pair(w, e, EcsChildOf, (ecs_entity_t)parentId);
}
int Scene::_childCount(uint32_t parentId) const {
    ecs_world_t* w = _p->ecs.c_ptr();
    return (int)ecs_count_id(w, ecs_pair(EcsChildOf, (ecs_entity_t)parentId));
}
void Scene::_forEachChildOpaque(uint32_t parentId, void(*cb)(void*, uint32_t, Scene*), void* ctx) const {
    flecs::entity parent(_p->ecs, (ecs_entity_t)parentId);
    parent.children([&](flecs::entity child){ cb(ctx, (uint32_t)child.id(), const_cast<Scene*>(this)); });
}
void Scene::_forEachRootOpaque(void(*cb)(void*, uint32_t, Scene*), void* ctx) const {
    ecs_world_t* w = _p->ecs.c_ptr();

    auto q = _p->ecs.query_builder<LocalTRS>().build();
    q.each([&](flecs::entity e, LocalTRS&) {
        if (!ecs_get_target(w, e.id(), EcsChildOf, 0)) {
            cb(ctx, (uint32_t)e.id(), const_cast<Scene*>(this));
        }
    });
}

void Scene::_forEachComponentOpaque(uint32_t id,
                                    void(*cb)(void*, uint32_t, const char*, const void*, std::size_t, std::size_t, bool),
                                    void* ctx) const {
    ecs_world_t* w = _p->ecs.c_ptr();
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

        cb(ctx, (uint32_t)compEnt, name, data, size, align, isTag);
    }
}

void Scene::_applyRegistry() {
    ApplyGlobalRegistryToScene(*_p);
}

System Scene::_create_system(const std::vector<const std::type_info*>& compTypes,
                             const std::vector<std::size_t>& sizes,
                             const std::vector<std::size_t>& aligns,
                             void(*cb)(void* ctx, uint32_t eid, void** comps, float dt),
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
qd.cache_kind = EcsQueryCacheNone; // default caching
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

uint32_t Scene::_getParentId(uint32_t id) const {
    return (uint32_t)ecs_get_target(_p->ecs.c_ptr(), (ecs_entity_t)id, EcsChildOf, 0);
}

namespace ECS {
void RegisterComponent(const std::type_info& ti, std::size_t size, std::size_t align, const char* name) {
    RegisterComponent(ti, size, align, name, nullptr);
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
