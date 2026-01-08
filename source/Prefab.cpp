#include <Prefab.hpp>
#include <cassert>

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
    Prefab prefab(root.GetName());
    if (!root.IsValid()) return prefab;

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

uint32_t Prefab::CloneEntityRecursive(const Scene& src, Scene& dst, uint32_t srcId, uint32_t dstParentId) {
    Entity srcEnt = src.FromId(srcId);
    const char* name = srcEnt.GetName();

    Entity dstEnt = dst.Instantiate(name && *name ? name : nullptr);
    if (dstParentId) {
        dstEnt.SetParent(dst.FromId(dstParentId));
    }

    src.ForEachComponent(srcEnt, [&](const ComponentView& c){
        if (!c.name || !*c.name) return;
        uint32_t dstCompId = dst._ensureComponentByName(c.name, c.size, c.align);
        dst._addById(dstEnt.RawId(), dstCompId, c.data, c.size, c.align);
    });

    src.ForEachChild(srcEnt, [&](Entity child){
        CloneEntityRecursive(src, dst, child.RawId(), dstEnt.RawId());
    });

    return dstEnt.RawId();
}
