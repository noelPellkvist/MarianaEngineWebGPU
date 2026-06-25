#include <Prefab.hpp>
#include <cassert>
#include <cstring>

Prefab::Prefab(const char* name) {
    _scene.UpdateComponentRegistry();
    Entity root = _scene.Instantiate(name);
    _rootId = root.RawId();
}

Prefab::Prefab(Prefab&& o) noexcept
    : _scene(std::move(o._scene)),
      _rootId(o._rootId) {
    o._rootId = 0;
}

Prefab& Prefab::operator=(Prefab&& o) noexcept {
    if (this != &o) {
        _scene = std::move(o._scene);
        _rootId = o._rootId;
        o._rootId = 0;
    }
    return *this;
}

Prefab Prefab::FromEntity(const Scene& src, Entity root) {
    if (!root.IsValid()) return Prefab();
    Prefab prefab(root.GetName());

    prefab._scene.Destroy(prefab.Root());
    prefab._rootId = CloneEntityRecursive(src, prefab._scene, root.RawId(), 0);
    return prefab;
}

const char* Prefab::GetName() const {
    Entity root = Root();
    return root.IsValid() ? root.GetName() : "";
}

Entity Prefab::Root() {
    return _scene.FromId(_rootId);
}

Entity Prefab::Root() const {
    return _scene.FromId(_rootId);
}

Entity Prefab::Instantiate(const char* name) {
    return _scene.Instantiate(name);
}

uint64_t Prefab::CloneEntityRecursive(const Scene& src, Scene& dst, uint64_t srcId, uint64_t dstParentId) {
    Entity srcEnt = src.FromId(srcId);
    if (!srcEnt.IsValid()) return 0;

    const char* name = srcEnt.GetName();

    Entity dstEnt = dst.Instantiate(name && *name ? name : nullptr);
    if (!dstEnt.IsValid()) return 0;

    if (dstParentId) {
        Entity dstParent = dst.FromId(dstParentId);
        if (dstParent.IsValid()) {
            dstEnt.SetParent(dstParent);
        }
    }

    src.ForEachComponent(srcEnt, [&](const ComponentView& c){
        if (!c.name || !*c.name) return;
        if (std::strcmp(c.name, "WorldXform") == 0) return;
        if (std::strcmp(c.name, "_XformCache") == 0) return;
        if (std::strcmp(c.name, "XformCache") == 0) return;
        if (std::strcmp(c.name, "_TransformClock") == 0) return;
        if (std::strcmp(c.name, "TransformClock") == 0) return;
        uint64_t dstCompId = dst._ensureComponentByName(c.name, c.size, c.align);
        dst._addById(dstEnt.RawId(), dstCompId, c.data, c.size, c.align);
    });

    src.ForEachChild(srcEnt, [&](Entity child){
        if (!child.IsValid()) return;
        CloneEntityRecursive(src, dst, child.RawId(), dstEnt.RawId());
    });

    return dstEnt.RawId();
}
