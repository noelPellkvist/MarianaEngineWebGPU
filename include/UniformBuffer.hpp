// UniformBuffer_min.hpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// ---------- WGSL uniform layout (std140-ish) ----------
enum class Kind {
    // scalars
    f32, i32, u32, b32,
    // vectors (shape-only; scalar type doesn’t change size/alignment in uniforms)
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

// WGSL uniform-space alignment
static inline std::size_t align_of(Kind k) {
    switch (k) {
        // scalars & bool (32-bit in buffers)
        case Kind::f32:
        case Kind::i32:
        case Kind::u32:
        case Kind::b32: return 4;

        case Kind::vec2: return 8;
        case Kind::vec3: [[fallthrough]];
        case Kind::vec4: return 16;

        // matrices: base alignment is 16 (columns have 16-byte stride)
        case Kind::mat2x2: case Kind::mat2x3: case Kind::mat2x4:
        case Kind::mat3x2: case Kind::mat3x3: case Kind::mat3x4:
        case Kind::mat4x2: case Kind::mat4x3: case Kind::mat4x4:
            return 16;
    }
    return 16;
}

// WGSL uniform-space *padded* size
static inline std::size_t size_of(Kind k) {
    switch (k) {
        case Kind::f32:
        case Kind::i32:
        case Kind::u32:
        case Kind::b32: return 4;

        case Kind::vec2: return 8;
        case Kind::vec3: [[fallthrough]];
        case Kind::vec4: return 16;

        // matrices: C columns, each with 16-byte stride => size = C * 16
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
        case Kind::f32:
        case Kind::i32:
        case Kind::u32:
        case Kind::b32: return 4;

        case Kind::vec2: return 8;
        case Kind::vec3: return 12;
        case Kind::vec4: return 16;

        // matrices: sizeof(float) * rows * cols
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

// scalars
template<> struct map_kind<float>        { static constexpr Kind value = Kind::f32; };
template<> struct map_kind<int>          { static constexpr Kind value = Kind::i32; };
template<> struct map_kind<unsigned int> { static constexpr Kind value = Kind::u32; };
template<> struct map_kind<bool>         { static constexpr Kind value = Kind::b32; };

// vectors (shape-only; i/u/f variants map to same shape)
template<> struct map_kind<glm::vec2>    { static constexpr Kind value = Kind::vec2; };
template<> struct map_kind<glm::vec3>    { static constexpr Kind value = Kind::vec3; };
template<> struct map_kind<glm::vec4>    { static constexpr Kind value = Kind::vec4; };

template<> struct map_kind<glm::ivec2>   { static constexpr Kind value = Kind::vec2; };
template<> struct map_kind<glm::ivec3>   { static constexpr Kind value = Kind::vec3; };
template<> struct map_kind<glm::ivec4>   { static constexpr Kind value = Kind::vec4; };

template<> struct map_kind<glm::uvec2>   { static constexpr Kind value = Kind::vec2; };
template<> struct map_kind<glm::uvec3>   { static constexpr Kind value = Kind::vec3; };
template<> struct map_kind<glm::uvec4>   { static constexpr Kind value = Kind::vec4; };

// matrices – float only (WGSL requirement). Use GLM aliases and the primary template.
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

// Convenience aliases (GLM typedefs)
template<> struct map_kind<glm::mat2>    { static constexpr Kind value = Kind::mat2x2; };
template<> struct map_kind<glm::mat3>    { static constexpr Kind value = Kind::mat3x3; };
template<> struct map_kind<glm::mat4>    { static constexpr Kind value = Kind::mat4x4; };

template<> struct map_kind<glm::mat2x3>  { static constexpr Kind value = Kind::mat2x3; };
template<> struct map_kind<glm::mat2x4>  { static constexpr Kind value = Kind::mat2x4; };
template<> struct map_kind<glm::mat3x2>  { static constexpr Kind value = Kind::mat3x2; };
template<> struct map_kind<glm::mat3x4>  { static constexpr Kind value = Kind::mat3x4; };
template<> struct map_kind<glm::mat4x2>  { static constexpr Kind value = Kind::mat4x2; };
template<> struct map_kind<glm::mat4x3>  { static constexpr Kind value = Kind::mat4x3; };

// ---------- UniformBuffer with your constructor ----------
template <typename T>
class UniformBuffer {
public:
    UniformBuffer() = default;

    // EXACT signature you asked for:
    template <typename... Ms>
    explicit UniformBuffer(const T& base, const Ms&... fields) {
        static_assert(sizeof...(Ms) > 0, "Provide at least one field.");
        build_layout_and_pack(base, fields...);
        // For safety, round up to 16 bytes at the end (std140-ish)
        buffer_.resize(align_to(buffer_.size(), 16));
    }

    // Update from another instance of the same struct T
    void updateValue(const T& obj) {
        const std::byte* base = reinterpret_cast<const std::byte*>(&obj);
        for (std::size_t i = 0; i < table_.size(); ++i) {
            const auto& e = table_[i];
            const std::byte* src = base + e.src_offset;
            std::byte* dst = buffer_.data() + e.dst_offset;

            if (!is_matrix(e.kind)) {
                std::memcpy(dst, src, e.copy_bytes);
            } else {
                // column-major: rows first in memory, per GLM
                const int C = mat_cols(e.kind);
                const int R = mat_rows(e.kind);
                const std::size_t src_col_bytes = 4 * R;   // float * rows
                const std::size_t dst_stride     = 16;     // WGSL/std140 column stride

                for (int c = 0; c < C; ++c) {
                    std::memcpy(dst + c * dst_stride, src + c * src_col_bytes, src_col_bytes);
                }
            }
        }
    }


    // Write a single field by index with raw bytes (optional convenience)
    void write(std::size_t index, const void* src, std::size_t bytes) {
        if (index >= table_.size()) throw std::out_of_range("field index");
        const auto& e = table_[index];
        if (bytes > e.copy_bytes) throw std::runtime_error("too many bytes for this field");
        std::memcpy(buffer_.data() + e.dst_offset, src, bytes);
    }

    // Offsets/sizes for debugging or binding
    const std::vector<Field>& fieldTable() const { return layout_; }

    // Upload helpers
    const void* data() const { return buffer_.data(); }
    std::size_t size() const { return buffer_.size(); }

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
            // Likely the user passed a value not belonging to 'base'
            throw std::runtime_error("Field does not belong to the provided base object.");
        }
        return static_cast<std::size_t>(d);
    }

    template<typename U>
    void add_one(const T& base, const U& field_ref) {
        // Deduce kind and sizes
        const Kind k = deduce_kind(field_ref);
        const std::size_t A = align_of(k);
        const std::size_t S = size_of(k);
        const std::size_t C = raw_copy_bytes(k);

        // WGSL destination offset
        current_dst_ = align_to(current_dst_, A);
        if (buffer_.size() < current_dst_ + S) buffer_.resize(current_dst_ + S);

        // Source offset inside T
        std::size_t src_off = offset_in_T(base, field_ref);

        // Record tables
        layout_.push_back(Field{ k, current_dst_, S });
        table_.push_back(Entry{ current_dst_, src_off, C, k });

        current_dst_ += S;
    }

    template<typename U, typename... Rest>
    void add_all(const T& base, const U& first, const Rest&... rest) {
        add_one(base, first);
        if constexpr (sizeof...(rest) > 0) add_all(base, rest...);
    }

    template<typename... Ms>
    void build_layout_and_pack(const T& base, const Ms&... fields) {
        // Build layout
        add_all(base, fields...);

        // Initial pack from 'base'
        updateValue(base);
    }

private:
    std::vector<std::byte> buffer_;
    std::vector<Field> layout_; // WGSL offsets/sizes
    std::vector<Entry> table_;  // mapping to T
    std::size_t current_dst_ = 0;
};
