#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>
#include <vector>
#include <type_traits>
#include <cstdint>
#include <cstddef>

// ---------- small utils ----------
template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

// ---------- engine-visible enums / structs ----------
enum class VertexFormat : uint32_t {
    // float32
    Float32, Float32x2, Float32x3, Float32x4,
    // sint32
    Sint32,  Sint32x2,  Sint32x3,  Sint32x4,
    // uint32
    Uint32,  Uint32x2,  Uint32x3,  Uint32x4,
    // 16-bit
    Sint16, Sint16x2, Sint16x4,
    Uint16, Uint16x2, Uint16x4,
    // 8-bit
    Sint8,  Sint8x2,  Sint8x4,
    Uint8,  Uint8x2,  Uint8x4,
};

enum class VertexStepMode : uint32_t { Vertex, Instance };

struct VertexAttributeDesc {
    VertexFormat  format{};
    std::uint64_t offset{};
    std::uint32_t location{};
};

template <typename T> struct VertexFormatOfGLM;

template <> struct VertexFormatOfGLM<glm::vec2> { static constexpr VertexFormat value = VertexFormat::Float32x2; };
template <> struct VertexFormatOfGLM<glm::vec3> { static constexpr VertexFormat value = VertexFormat::Float32x3; };
template <> struct VertexFormatOfGLM<glm::vec4> { static constexpr VertexFormat value = VertexFormat::Float32x4; };

template <> struct VertexFormatOfGLM<glm::ivec2>{ static constexpr VertexFormat value = VertexFormat::Sint32x2; };
template <> struct VertexFormatOfGLM<glm::ivec3>{ static constexpr VertexFormat value = VertexFormat::Sint32x3; };
template <> struct VertexFormatOfGLM<glm::ivec4>{ static constexpr VertexFormat value = VertexFormat::Sint32x4; };

template <> struct VertexFormatOfGLM<glm::uvec2>{ static constexpr VertexFormat value = VertexFormat::Uint32x2; };
template <> struct VertexFormatOfGLM<glm::uvec3>{ static constexpr VertexFormat value = VertexFormat::Uint32x3; };
template <> struct VertexFormatOfGLM<glm::uvec4>{ static constexpr VertexFormat value = VertexFormat::Uint32x4; };

template <> struct VertexFormatOfGLM<glm::i16vec2>{ static constexpr VertexFormat value = VertexFormat::Sint16x2; };
template <> struct VertexFormatOfGLM<glm::i16vec4>{ static constexpr VertexFormat value = VertexFormat::Sint16x4; };
template <> struct VertexFormatOfGLM<glm::u16vec2>{ static constexpr VertexFormat value = VertexFormat::Uint16x2; };
template <> struct VertexFormatOfGLM<glm::u16vec4>{ static constexpr VertexFormat value = VertexFormat::Uint16x4; };

template <> struct VertexFormatOfGLM<glm::i8vec2> { static constexpr VertexFormat value = VertexFormat::Sint8x2;  };
template <> struct VertexFormatOfGLM<glm::i8vec4> { static constexpr VertexFormat value = VertexFormat::Sint8x4;  };
template <> struct VertexFormatOfGLM<glm::u8vec2> { static constexpr VertexFormat value = VertexFormat::Uint8x2;  };
template <> struct VertexFormatOfGLM<glm::u8vec4> { static constexpr VertexFormat value = VertexFormat::Uint8x4;  };

template <> struct VertexFormatOfGLM<float>      { static constexpr VertexFormat value = VertexFormat::Float32;  };
template <> struct VertexFormatOfGLM<int32_t>    { static constexpr VertexFormat value = VertexFormat::Sint32;   };
template <> struct VertexFormatOfGLM<uint32_t>   { static constexpr VertexFormat value = VertexFormat::Uint32;   };
template <> struct VertexFormatOfGLM<int16_t>    { static constexpr VertexFormat value = VertexFormat::Sint16;   };
template <> struct VertexFormatOfGLM<uint16_t>   { static constexpr VertexFormat value = VertexFormat::Uint16;   };
template <> struct VertexFormatOfGLM<int8_t>     { static constexpr VertexFormat value = VertexFormat::Sint8;    };
template <> struct VertexFormatOfGLM<uint8_t>    { static constexpr VertexFormat value = VertexFormat::Uint8;    };

template <typename T>
constexpr VertexFormat DeduceVertexFormat() { return VertexFormatOfGLM<remove_cvref_t<T>>::value; }

template <typename T> struct IsUnsupported8or16Vec3 : std::false_type {};
template <> struct IsUnsupported8or16Vec3<glm::i8vec3>  : std::true_type {};
template <> struct IsUnsupported8or16Vec3<glm::u8vec3>  : std::true_type {};
template <> struct IsUnsupported8or16Vec3<glm::i16vec3> : std::true_type {};
template <> struct IsUnsupported8or16Vec3<glm::u16vec3> : std::true_type {};

template <typename M>
struct FormatOverride { const M* ptr; VertexFormat fmt; };

template <typename M>
constexpr FormatOverride<M> WithFormat(const M& m, VertexFormat f) { return { &m, f }; }

// ------------------ VertexBufferLayout (PIMPL, Dawn hidden) ------------------
class VertexBufferLayout {
public:
    VertexBufferLayout();
    ~VertexBufferLayout();
    VertexBufferLayout(const VertexBufferLayout&);
    VertexBufferLayout& operator=(const VertexBufferLayout&);
    VertexBufferLayout(VertexBufferLayout&&) noexcept;
    VertexBufferLayout& operator=(VertexBufferLayout&&) noexcept;

    // templated builder that records attributes and finalizes backend
    template <typename T, typename... Ms>
    explicit VertexBufferLayout(const T& base, const Ms&... fields)
    {
        static_assert(std::is_standard_layout_v<T>, "Vertex struct must be standard-layout.");

        std::vector<VertexAttributeDesc> attrs;
        attrs.reserve(sizeof...(Ms));

        std::uint32_t loc = 0;
        (attrs.push_back(MakeAttr(base, fields, loc++)), ...);

        _finalize(attrs, sizeof(T), VertexStepMode::Vertex);
    }

    void SetStepMode(VertexStepMode mode); // will update backend too
    std::size_t Stride() const;
    std::uint32_t AttributeCount() const;

    // ---------------- backend access (Dawn/WGPU), still hidden from headers ----------------
    // Returns a stable pointer to the internal backend layout (e.g., wgpu::VertexBufferLayout).
    // The type is purposely opaque to keep headers Dawn-free.
    const void* GetBackendLayout() const;
    // Returns pointer to the backend attribute array (e.g., wgpu::VertexAttribute[]) and count.
    const void* GetBackendAttributesData() const;
    std::uint32_t GetBackendAttributesCount() const;

private:
    struct Impl;         // defined in .cpp, holds wgpu:: vector + layout
    Impl* _impl = nullptr;

    // helpers used by the templated ctor (header-only)
    template <typename T, typename M>
    static VertexAttributeDesc MakeAttr(const T& base, const M& memberRef, std::uint32_t location) {
        using CleanM = remove_cvref_t<M>;
        static_assert(!IsUnsupported8or16Vec3<CleanM>::value,
            "No 8/16-bit x3 vertex formats. Use *x4 or pack manually.");
        (void)VertexFormatOfGLM<CleanM>::value; // force mapping to exist

        const auto* pBase  = reinterpret_cast<const std::uint8_t*>(&base);
        const auto* pField = reinterpret_cast<const std::uint8_t*>(&memberRef);
        const std::size_t offset = static_cast<std::size_t>(pField - pBase);

        VertexAttributeDesc a{};
        a.format   = DeduceVertexFormat<CleanM>();
        a.offset   = static_cast<std::uint64_t>(offset);
        a.location = location;
        return a;
    }

    template <typename T, typename M>
    static VertexAttributeDesc MakeAttr(const T& base, const FormatOverride<M>& ov, std::uint32_t location) {
        const auto* pBase  = reinterpret_cast<const std::uint8_t*>(&base);
        const auto* pField = reinterpret_cast<const std::uint8_t*>(ov.ptr);
        const std::size_t offset = static_cast<std::size_t>(pField - pBase);
        VertexAttributeDesc a{ ov.fmt, static_cast<std::uint64_t>(offset), location };
        return a;
    }

    // implemented in .cpp, builds/stores backend (wgpu) objects in Impl
    void _finalize(const std::vector<VertexAttributeDesc>& attrs, std::size_t stride, VertexStepMode mode);
};
