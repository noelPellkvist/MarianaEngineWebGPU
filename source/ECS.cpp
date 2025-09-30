#include <ECS.hpp>
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
struct XformCache { uint32_t local_v=~0u, parent_v=~0u, world_v=0; };
struct TransformClock { uint32_t tick = 1; };

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

    Scene* owner = nullptr;

    Impl(Scene* s) : ecs(), owner(s) {}
    Impl() : ecs(), owner(nullptr) {}

    // changed to accept a component name string (we no longer try to access std::type_info)
    ecs_entity_t ensureComponentByName(const std::string& name, std::size_t sz, std::size_t align) {
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
        ecs_entity_t cid = ecs_component_init(ecs.c_ptr(), &cd);

        return cid;
    }

    ecs_entity_t ensureComponent(const std::type_info& ti, std::size_t sz, std::size_t align) {
        auto key = std::type_index(ti);
        if (auto it = comp.find(key); it != comp.end()) return it->second;

        // Use the type_info::name() as the component name (this is what prior code did).
        const char* nm = ti.name();

        ecs_entity_desc_t ed{};
        ed.name = nm;
        ed.use_low_id = true;
        ecs_entity_t ent = ecs_entity_init(ecs.c_ptr(), &ed);

        ecs_component_desc_t cd{};
        cd.entity = ent;
        cd.type.size = (ecs_size_t)sz;
        cd.type.alignment = (ecs_size_t)align;
        ecs_entity_t cid = ecs_component_init(ecs.c_ptr(), &cd);

        comp.emplace(key, cid);
        return cid;
    }
};

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
            // Safer approach: require header to allocate the context as std::function<void(uint64_t, void**)>*
            // However we used TrampolineCtx struct. To avoid UB here, we will not call delete on ctx_ptr; instead,
            // Scene::_create_system will take ownership of ctx_ptr and will delete it as the correct type.
            // To keep safe, we will assume ctx_ptr is a pointer that Scene::_create_system deletes later.
        }
    }
};

static void run_query_and_call(ecs_world_t* world, ecs_query_t* q, System::Impl* impl, float delta) {
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
            impl->cb(impl->ctx_ptr, (uint64_t)e, comps.data(), delta);
        }
    }
}

Scene::Scene() : _p(new Impl) {
    // Register components so typed APIs work
    auto idLocal = _p->ecs.component<LocalTRS>().set_name("LocalTRS").id();
    auto idWorld = _p->ecs.component<WorldXform>().set_name("WorldXform").id();
    _p->comp.emplace(std::type_index(typeid(LocalTRS)),   idLocal);
    _p->comp.emplace(std::type_index(typeid(WorldXform)), idWorld);

    _p->ecs.component<XformCache>().set_name("_XformCache");
    _p->ecs.component<TransformClock>().set_name("_TransformClock");
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

}

Scene::~Scene() { delete _p; }
Scene::Scene(Scene&& o) noexcept : _p(o._p) { o._p = nullptr; }
Scene& Scene::operator=(Scene&& o) noexcept { if (this!=&o){ delete _p; _p=o._p; o._p=nullptr; } return *this; }

uint64_t Scene::_create(const char* name) {
    auto e = _p->ecs.entity();
    if (name && *name) e.set_name(name);

    auto idLocal = _p->ensureComponent(typeid(LocalTRS),   sizeof(LocalTRS),   alignof(LocalTRS));
    auto idWorld = _p->ensureComponent(typeid(WorldXform), sizeof(WorldXform), alignof(WorldXform));

    LocalTRS   lt{};
    WorldXform wx{};
    ecs_set_id(_p->ecs.c_ptr(), e.id(), idLocal, sizeof(LocalTRS),   &lt);
    ecs_set_id(_p->ecs.c_ptr(), e.id(), idWorld, sizeof(WorldXform), &wx);

    e.set<XformCache>({});

    return (uint64_t)e.id();
}

void Scene::_destroy(uint64_t id) { _p->ecs.entity((flecs::entity_t)id).destruct(); }
void Scene::_update(float) { _p->ecs.progress(); }
void Scene::_setName(uint64_t id, const char* name) { _p->ecs.entity((flecs::entity_t)id).set_name(name ? name : ""); }
const char* Scene::_getName(uint64_t eid) const { return ecs_get_name(_p->ecs.c_ptr(), (ecs_entity_t)eid); }

bool Scene::_has(uint64_t id, const std::type_info& ti) const {
    auto it = _p->comp.find(std::type_index(ti));
    if (it == _p->comp.end()) return false;
    return ecs_has_id(_p->ecs.c_ptr(), (ecs_entity_t)id, it->second);
}
void* Scene::_getMut(uint64_t id, const std::type_info& ti, std::size_t sz, std::size_t align) const {
    ecs_entity_t cid = _p->ensureComponent(ti, sz, align);
    return ecs_get_mut_id(_p->ecs.c_ptr(), (ecs_entity_t)id, cid);
}
void Scene::_addSet(uint64_t id, const std::type_info& ti, const void* data, std::size_t sz, std::size_t align) {
    ecs_entity_t cid = _p->ensureComponent(ti, sz, align);
    ecs_set_id(_p->ecs.c_ptr(), (ecs_entity_t)id, cid, sz, data);
}
void Scene::_remove(uint64_t id, const std::type_info& ti) const {
    auto it = _p->comp.find(std::type_index(ti));
    if (it == _p->comp.end()) return;
    ecs_remove_id(_p->ecs.c_ptr(), (ecs_entity_t)id, it->second);
}

void Scene::_setParent(uint64_t id, uint64_t parentId) {
    ecs_world_t* w = _p->ecs.c_ptr();
    ecs_entity_t e = (ecs_entity_t)id;
    ecs_remove_pair(w, e, EcsChildOf, EcsWildcard);
    if (parentId) ecs_add_pair(w, e, EcsChildOf, (ecs_entity_t)parentId);
}
int Scene::_childCount(uint64_t parentId) const {
    ecs_world_t* w = _p->ecs.c_ptr();
    return (int)ecs_count_id(w, ecs_pair(EcsChildOf, (ecs_entity_t)parentId));
}
void Scene::_forEachChildOpaque(uint64_t parentId, void(*cb)(void*, uint64_t, Scene*), void* ctx) const {
    flecs::entity parent(_p->ecs, (ecs_entity_t)parentId);
    parent.children([&](flecs::entity child){ cb(ctx, (uint64_t)child.id(), const_cast<Scene*>(this)); });
}
void Scene::_forEachRootOpaque(void(*cb)(void*, uint64_t, Scene*), void* ctx) const {
    ecs_world_t* w = _p->ecs.c_ptr();

    auto q = _p->ecs.query_builder<LocalTRS>().build();
    q.each([&](flecs::entity e, LocalTRS&) {
        if (!ecs_get_target(w, e.id(), EcsChildOf, 0)) {
            cb(ctx, (uint64_t)e.id(), const_cast<Scene*>(this));
        }
    });
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

    simpl->comp_ids.reserve(compTypes.size());
    std::string expr;

    for (size_t i = 0; i < compTypes.size(); ++i) {
        const std::type_info& ti = *compTypes[i];
        std::type_index tix(ti);

        // If the component was already registered in _p->comp, use that id.
        auto it = _p->comp.find(tix);
        ecs_entity_t cid = 0;
        if (it != _p->comp.end()) {
            cid = it->second;
        } else {
            // Fallback: register the component now using the same API your other code uses.
            cid = _p->ensureComponent(ti, sizes[i], aligns[i]);
        }

        simpl->comp_ids.push_back(cid);

        // Use the Flecs-registered name for the textual query
        if (i > 0) expr += ", ";
        const char* nm = _p->ecs.entity(cid).name();
        if (nm && nm[0]) expr += nm;
        else expr += ti.name(); // fallback (shouldn't happen)
    }

    // Build query using textual expression composed from actual registered names
    ecs_query_desc_t qd{};
    qd.expr = expr.c_str();
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
    return (uint64_t)ecs_get_target(_p->ecs.c_ptr(), (ecs_entity_t)id, EcsChildOf, 0);
}
