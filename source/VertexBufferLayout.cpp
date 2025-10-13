#include <VertexBufferLayout.hpp> 
#include <webgpu/webgpu_cpp.h>
#include <cassert>
#include <utility>

// ---------------- Impl definition (holds Dawn data) ----------------
struct VertexBufferLayout::Impl {
    std::vector<wgpu::VertexAttribute> wgpuAttrs;
    wgpu::VertexBufferLayout           wgpuLayout{};
    std::size_t                        stride = 0;
    VertexStepMode                     stepMode = VertexStepMode::Vertex;

    Impl() = default;
    Impl(const Impl& rhs)            = default;  // vector + POD are copyable
    Impl& operator=(const Impl& rhs) = default;
};

// --------- small converters (engine enums -> wgpu) ----------
static wgpu::VertexFormat ToWGPU(VertexFormat f) {
    switch (f) {
        case VertexFormat::Float32:   return wgpu::VertexFormat::Float32;
        case VertexFormat::Float32x2: return wgpu::VertexFormat::Float32x2;
        case VertexFormat::Float32x3: return wgpu::VertexFormat::Float32x3;
        case VertexFormat::Float32x4: return wgpu::VertexFormat::Float32x4;

        case VertexFormat::Sint32:    return wgpu::VertexFormat::Sint32;
        case VertexFormat::Sint32x2:  return wgpu::VertexFormat::Sint32x2;
        case VertexFormat::Sint32x3:  return wgpu::VertexFormat::Sint32x3;
        case VertexFormat::Sint32x4:  return wgpu::VertexFormat::Sint32x4;

        case VertexFormat::Uint32:    return wgpu::VertexFormat::Uint32;
        case VertexFormat::Uint32x2:  return wgpu::VertexFormat::Uint32x2;
        case VertexFormat::Uint32x3:  return wgpu::VertexFormat::Uint32x3;
        case VertexFormat::Uint32x4:  return wgpu::VertexFormat::Uint32x4;

        case VertexFormat::Sint16x2:  return wgpu::VertexFormat::Sint16x2;
        case VertexFormat::Sint16x4:  return wgpu::VertexFormat::Sint16x4;
        case VertexFormat::Uint16x2:  return wgpu::VertexFormat::Uint16x2;
        case VertexFormat::Uint16x4:  return wgpu::VertexFormat::Uint16x4;

        case VertexFormat::Sint8x2:   return wgpu::VertexFormat::Sint8x2;
        case VertexFormat::Sint8x4:   return wgpu::VertexFormat::Sint8x4;
        case VertexFormat::Uint8x2:   return wgpu::VertexFormat::Uint8x2;
        case VertexFormat::Uint8x4:   return wgpu::VertexFormat::Uint8x4;
    }
    assert(false && "Unknown VertexFormat");
    return wgpu::VertexFormat::Float32;
}

static wgpu::VertexStepMode ToWGPU(VertexStepMode m) {
    return m == VertexStepMode::Instance ? wgpu::VertexStepMode::Instance
                                         : wgpu::VertexStepMode::Vertex;
}

// ---------------- VertexBufferLayout methods ----------------
VertexBufferLayout::VertexBufferLayout() = default;

VertexBufferLayout::~VertexBufferLayout() { delete _impl; }

VertexBufferLayout::VertexBufferLayout(const VertexBufferLayout& o) {
    if (o._impl) _impl = new Impl(*o._impl);
}
VertexBufferLayout& VertexBufferLayout::operator=(const VertexBufferLayout& o) {
    if (this == &o) return *this;
    delete _impl;
    _impl = o._impl ? new Impl(*o._impl) : nullptr;
    return *this;
}
VertexBufferLayout::VertexBufferLayout(VertexBufferLayout&& o) noexcept : _impl(o._impl) {
    o._impl = nullptr;
}
VertexBufferLayout& VertexBufferLayout::operator=(VertexBufferLayout&& o) noexcept {
    if (this == &o) return *this;
    delete _impl;
    _impl = o._impl; o._impl = nullptr;
    return *this;
}

void VertexBufferLayout::_finalize(const std::vector<VertexAttributeDesc>& attrs,
                                   std::size_t stride,
                                   VertexStepMode mode)
{
    if (!_impl) _impl = new Impl{};
    _impl->wgpuAttrs.clear();
    _impl->wgpuAttrs.reserve(attrs.size());

    for (const auto& a : attrs) {
        wgpu::VertexAttribute wa{};
        wa.format         = ToWGPU(a.format);
        wa.offset         = a.offset;
        wa.shaderLocation = a.location;
        _impl->wgpuAttrs.push_back(wa);
    }

    _impl->wgpuLayout.attributeCount = static_cast<uint32_t>(_impl->wgpuAttrs.size());
    _impl->wgpuLayout.attributes     = _impl->wgpuAttrs.data();
    _impl->wgpuLayout.arrayStride    = static_cast<uint64_t>(stride);
    _impl->wgpuLayout.stepMode       = ToWGPU(mode);

    _impl->stride   = stride;
    _impl->stepMode = mode;
}

void VertexBufferLayout::SetStepMode(VertexStepMode mode) {
    if (!_impl) _impl = new Impl{};
    _impl->stepMode = mode;
    _impl->wgpuLayout.stepMode = ToWGPU(mode);
}

std::size_t VertexBufferLayout::Stride() const {
    return _impl ? _impl->stride : 0;
}

std::uint32_t VertexBufferLayout::AttributeCount() const {
    return _impl ? static_cast<std::uint32_t>(_impl->wgpuAttrs.size()) : 0u;
}

const void* VertexBufferLayout::GetBackendLayout() const {
    return _impl ? static_cast<const void*>(&_impl->wgpuLayout) : nullptr;
}

const void* VertexBufferLayout::GetBackendAttributesData() const {
    return _impl && !_impl->wgpuAttrs.empty()
        ? static_cast<const void*>(_impl->wgpuAttrs.data())
        : nullptr;
}

std::uint32_t VertexBufferLayout::GetBackendAttributesCount() const {
    return _impl ? static_cast<std::uint32_t>(_impl->wgpuAttrs.size()) : 0u;
}
