// UniformBuffer_min.hpp (layout-only)
// Keep all your existing includes except WebGPU ones are no longer needed here.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <webgpu/webgpu_cpp.h>
#include <Init.hpp>

struct IUniformLayout {
    virtual ~IUniformLayout() = default;

    virtual void Init() = 0;

    virtual const wgpu::BindGroup&        GetBindGroup() const = 0;
    virtual const wgpu::BindGroupLayout&  GetBindGroupLayout() const = 0;
};

// ---------- WGSL uniform layout (std140-ish) ----------
enum class Kind {
    // scalars
    f32, i32, u32, b32,
    // vectors (shape-only)
    vec2, vec3, vec4,
    // matrices (columns x rows), float-only in WGSL
    mat2x2, mat2x3, mat2x4,
    mat3x2, mat3x3, mat3x4,
    mat4x2, mat4x3, mat4x4
};

struct Field {
    Kind kind;
    std::size_t offset; // WGSL byte offset in the CPU buffer
    std::size_t size;   // WGSL padded size (e.g., vec3 -> 16)
};

static inline std::size_t align_to(std::size_t o, std::size_t a) {
    return (o + (a - 1)) / a * a;
}

static inline bool is_matrix(Kind k){
    switch(k){
        case Kind::mat2x2: case Kind::mat2x3: case Kind::mat2x4:
        case Kind::mat3x2: case Kind::mat3x3: case Kind::mat3x4:
        case Kind::mat4x2: case Kind::mat4x3: case Kind::mat4x4:
            return true;
        default: return false;
    }
}
static inline int mat_cols(Kind k){
    switch(k){
        case Kind::mat2x2: case Kind::mat2x3: case Kind::mat2x4: return 2;
        case Kind::mat3x2: case Kind::mat3x3: case Kind::mat3x4: return 3;
        case Kind::mat4x2: case Kind::mat4x3: case Kind::mat4x4: return 4;
        default: return 0;
    }
}
static inline int mat_rows(Kind k){
    switch(k){
        case Kind::mat2x2: case Kind::mat3x2: case Kind::mat4x2: return 2;
        case Kind::mat2x3: case Kind::mat3x3: case Kind::mat4x3: return 3;
        case Kind::mat2x4: case Kind::mat3x4: case Kind::mat4x4: return 4;
        default: return 0;
    }
}

// WGSL uniform-space alignment and padded size
static inline std::size_t align_of(Kind k) {
    switch (k) {
        case Kind::f32: case Kind::i32: case Kind::u32: case Kind::b32: return 4;
        case Kind::vec2: return 8;
        case Kind::vec3: [[fallthrough]];
        case Kind::vec4: return 16;
        default: return 16; // matrices: 16
    }
}
static inline std::size_t size_of(Kind k) {
    switch (k) {
        case Kind::f32: case Kind::i32: case Kind::u32: case Kind::b32: return 4;
        case Kind::vec2: return 8;
        case Kind::vec3: [[fallthrough]];
        case Kind::vec4: return 16;
        case Kind::mat2x2: return 2 * 16;
        case Kind::mat2x3: return 2 * 16;
        case Kind::mat2x4: return 2 * 16;
        case Kind::mat3x2: return 3 * 16;
        case Kind::mat3x3: return 3 * 16;
        case Kind::mat3x4: return 3 * 16;
        case Kind::mat4x2: return 4 * 16;
        case Kind::mat4x3: return 4 * 16;
        case Kind::mat4x4: return 4 * 16;
    }
    return 0;
}

// How many source bytes to copy (no padding)
static inline std::size_t raw_copy_bytes(Kind k) {
    switch (k) {
        case Kind::f32: case Kind::i32: case Kind::u32: case Kind::b32: return 4;
        case Kind::vec2: return 8;
        case Kind::vec3: return 12;
        case Kind::vec4: return 16;
        case Kind::mat2x2: return 4 * 2 * 2;  // 16
        case Kind::mat2x3: return 4 * 3 * 2;  // 24
        case Kind::mat2x4: return 4 * 4 * 2;  // 32
        case Kind::mat3x2: return 4 * 2 * 3;  // 24
        case Kind::mat3x3: return 4 * 3 * 3;  // 36
        case Kind::mat3x4: return 4 * 4 * 3;  // 48
        case Kind::mat4x2: return 4 * 2 * 4;  // 32
        case Kind::mat4x3: return 4 * 3 * 4;  // 48
        case Kind::mat4x4: return 4 * 4 * 4;  // 64
    }
    return 0;
}

// ---------- Type-to-Kind (GLM + scalars) ----------
template<class U> struct map_kind;

template<> struct map_kind<float>        { static constexpr Kind value = Kind::f32; };
template<> struct map_kind<int>          { static constexpr Kind value = Kind::i32; };
template<> struct map_kind<unsigned int> { static constexpr Kind value = Kind::u32; };
template<> struct map_kind<bool>         { static constexpr Kind value = Kind::b32; };

template<> struct map_kind<glm::vec2>    { static constexpr Kind value = Kind::vec2; };
template<> struct map_kind<glm::vec3>    { static constexpr Kind value = Kind::vec3; };
template<> struct map_kind<glm::vec4>    { static constexpr Kind value = Kind::vec4; };

template<> struct map_kind<glm::ivec2>   { static constexpr Kind value = Kind::vec2; };
template<> struct map_kind<glm::ivec3>   { static constexpr Kind value = Kind::vec3; };
template<> struct map_kind<glm::ivec4>   { static constexpr Kind value = Kind::vec4; };

template<> struct map_kind<glm::uvec2>   { static constexpr Kind value = Kind::vec2; };
template<> struct map_kind<glm::uvec3>   { static constexpr Kind value = Kind::vec3; };
template<> struct map_kind<glm::uvec4>   { static constexpr Kind value = Kind::vec4; };

template<int C, int R, typename T, glm::qualifier Q>
struct map_kind<glm::mat<C,R,T,Q>> {
    static_assert(std::is_same_v<T,float>, "WGSL matrices must be float (f32).");
    static constexpr Kind value =
        (C==2 && R==2) ? Kind::mat2x2 :
        (C==2 && R==3) ? Kind::mat2x3 :
        (C==2 && R==4) ? Kind::mat2x4 :
        (C==3 && R==2) ? Kind::mat3x2 :
        (C==3 && R==3) ? Kind::mat3x3 :
        (C==3 && R==4) ? Kind::mat3x4 :
        (C==4 && R==2) ? Kind::mat4x2 :
        (C==4 && R==3) ? Kind::mat4x3 :
        /*C==4 && R==4*/ Kind::mat4x4;
};

template<> struct map_kind<glm::mat2>    { static constexpr Kind value = Kind::mat2x2; };
template<> struct map_kind<glm::mat3>    { static constexpr Kind value = Kind::mat3x3; };
template<> struct map_kind<glm::mat4>    { static constexpr Kind value = Kind::mat4x4; };

template<> struct map_kind<glm::mat2x3>  { static constexpr Kind value = Kind::mat2x3; };
template<> struct map_kind<glm::mat2x4>  { static constexpr Kind value = Kind::mat2x4; };
template<> struct map_kind<glm::mat3x2>  { static constexpr Kind value = Kind::mat3x2; };
template<> struct map_kind<glm::mat3x4>  { static constexpr Kind value = Kind::mat3x4; };
template<> struct map_kind<glm::mat4x2>  { static constexpr Kind value = Kind::mat4x2; };
template<> struct map_kind<glm::mat4x3>  { static constexpr Kind value = Kind::mat4x3; };

// ---------- Layout-only builder ----------
template <typename T>
class UniformLayout : public IUniformLayout {
public:
    UniformLayout() = default;

    const wgpu::BindGroup& GetBindGroup() const override { return bindGroup; }

    const wgpu::BindGroupLayout& GetBindGroupLayout() const override { return bindGroupLayout; }

    template <typename... Ms>
    explicit UniformLayout(bool isDynamic, const T& base, const Ms&... fields) {
        this->m_isDynamic = isDynamic;
        static_assert(sizeof...(Ms) > 0, "Provide at least one field.");
        build_layout(base, fields...);
    }

    void Init() override
    {
        wgpu::BufferDescriptor bufferDesc;
        if(m_isDynamic)
        {
            uniformStride = ceilToNextMultiple(total_size_);
            bufferDesc.size = uniformStride * 256;
        }
        else
            bufferDesc.size = total_size_;
        
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        bufferDesc.mappedAtCreation = false;
        m_GPUBuffer = device.CreateBuffer(&bufferDesc);

        device.GetQueue().WriteBuffer(m_GPUBuffer, 0, m_Buffer.data(), m_Buffer.size());

        bindingLayout.binding = 0;
        bindingLayout.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
        bindingLayout.buffer.type = wgpu::BufferBindingType::Uniform;
        bindingLayout.buffer.minBindingSize = total_size_;
        bindingLayout.buffer.hasDynamicOffset = m_isDynamic;

        wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc{};
        bindGroupLayoutDesc.entryCount = 1;
        bindGroupLayoutDesc.entries = &bindingLayout;
        bindGroupLayout = device.CreateBindGroupLayout(&bindGroupLayoutDesc);

        wgpu::BindGroupEntry binding{};
        binding.binding = 0;
        binding.buffer = m_GPUBuffer;
        binding.offset = 0;
        binding.size = m_isDynamic ? uniformStride : total_size_;

        wgpu::BindGroupDescriptor bindGroupDesc{};
        bindGroupDesc.layout = bindGroupLayout;
        bindGroupDesc.entryCount = 1;
        bindGroupDesc.entries = &binding;
        bindGroup = device.CreateBindGroup(&bindGroupDesc);
    }

    // Pack into a freshly allocated vector (returns padded-to-16B size)
    inline void pack(const T& obj) {
        assert(m_isDynamic == false);
        m_Buffer.resize(total_size_);
        pack_into(obj, m_Buffer.data(), m_Buffer.size());
        device.GetQueue().WriteBuffer(m_GPUBuffer, 0, m_Buffer.data(), m_Buffer.size());
    }

    inline void pack(const T& obj, uint32_t index) {
        assert(m_isDynamic == true);
        m_Buffer.resize(total_size_);
        pack_into(obj, m_Buffer.data(), m_Buffer.size());
        device.GetQueue().WriteBuffer(m_GPUBuffer, uniformStride * index, m_Buffer.data(), m_Buffer.size());
    }

    // Pack into caller-provided memory (must be at least total_size())
    void pack_into(const T& obj, void* dst, std::size_t dstBytes) const {
        if (dstBytes < total_size_) throw std::runtime_error("pack_into: dst too small");
        std::byte* buffer = reinterpret_cast<std::byte*>(dst);
        const std::byte* base = reinterpret_cast<const std::byte*>(&obj);

        for (const auto& e : table_) {
            const std::byte* src = base + e.src_offset;
            std::byte*       d   = buffer + e.dst_offset;

            if (!is_matrix(e.kind)) {
                std::memcpy(d, src, e.copy_bytes);
            } else {
                const int C = mat_cols(e.kind);
                const int R = mat_rows(e.kind);
                const std::size_t src_col_bytes = 4 * R;   // float * rows
                const std::size_t dst_stride     = 16;     // WGSL/std140 column stride
                for (int c = 0; c < C; ++c) {
                    std::memcpy(d + c * dst_stride, src + c * src_col_bytes, src_col_bytes);
                }
            }
        }
    }

    // Introspection
    const std::vector<Field>& fieldTable() const { return layout_; }
    std::size_t total_size() const { return total_size_; }

private:
    struct Entry {
        std::size_t dst_offset; // where in the WGSL buffer to write
        std::size_t src_offset; // where in T the field starts
        std::size_t copy_bytes; // raw bytes to copy (no padding)
        Kind kind;
    };

    template<typename U>
    static constexpr Kind deduce_kind(const U&) {
        using Dec = std::decay_t<U>;
        static_assert(!std::is_pointer_v<Dec>, "Fields must be values/references, not pointers.");
        return map_kind<Dec>::value; // will static-assert if unsupported type
    }

    // Compute member offset in T by pointer arithmetic on the arguments
    template<typename U>
    static std::size_t offset_in_T(const T& base, const U& field_ref) {
        const auto* base_b  = reinterpret_cast<const std::byte*>(&base);
        const auto* field_b = reinterpret_cast<const std::byte*>(&field_ref);
        std::ptrdiff_t d = field_b - base_b;
        if (d < 0 || static_cast<std::size_t>(d) >= sizeof(T)) {
            throw std::runtime_error("Field does not belong to the provided base object.");
        }
        return static_cast<std::size_t>(d);
    }

    template<typename U>
    void add_one(const T& base, const U& field_ref, std::size_t& current_dst) {
        const Kind k = deduce_kind(field_ref);
        const std::size_t A = align_of(k);
        const std::size_t S = size_of(k);
        const std::size_t C = raw_copy_bytes(k);

        current_dst = align_to(current_dst, A);

        const std::size_t src_off = offset_in_T(base, field_ref);

        layout_.push_back(Field{ k, current_dst, S });
        table_.push_back(Entry{ current_dst, src_off, C, k });

        current_dst += S;
    }

    template<typename U, typename... Rest>
    void add_all(const T& base, std::size_t& current_dst, const U& first, const Rest&... rest) {
        add_one(base, first, current_dst);
        if constexpr (sizeof...(rest) > 0) add_all(base, current_dst, rest...);
    }

    template<typename... Ms>
    void build_layout(const T& base, const Ms&... fields) {
        std::size_t current_dst = 0;
        add_all(base, current_dst, fields...);
        total_size_ = align_to(current_dst, 16);
    }

private:
    std::vector<Field> layout_;
    std::vector<Entry> table_;
    std::size_t total_size_ = 0;
    std::vector<std::byte> m_Buffer;

    wgpu::Buffer m_GPUBuffer;
    bool m_isDynamic = false;
    uint32_t uniformStride;

    wgpu::BindGroupLayout bindGroupLayout;

    wgpu::BindGroupLayoutEntry bindingLayout{};
    wgpu::BindGroup bindGroup{};
};

