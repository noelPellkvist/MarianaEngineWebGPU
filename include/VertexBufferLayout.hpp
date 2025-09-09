#pragma once
#include <webgpu/webgpu_cpp.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp> // i8vec*, u8vec*, i16vec*, u16vec*
#include <vector>
#include <type_traits>
#include <cstddef>
#include <cstdint>
#include <string>

#include <Logger.hpp>

template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

// ================= GLM -> wgpu::VertexFormat (constexpr) =================
// Primary: intentionally undefined to force a compile error when unmapped.
template <typename T> struct VertexFormatOfGLM;

// --- 32-bit float vectors ---
template <> struct VertexFormatOfGLM<glm::vec2> { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Float32x2; };
template <> struct VertexFormatOfGLM<glm::vec3> { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Float32x3; };
template <> struct VertexFormatOfGLM<glm::vec4> { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Float32x4; };

// --- 32-bit signed/unsigned int vectors ---
template <> struct VertexFormatOfGLM<glm::ivec2> { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Sint32x2; };
template <> struct VertexFormatOfGLM<glm::ivec3> { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Sint32x3; };
template <> struct VertexFormatOfGLM<glm::ivec4> { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Sint32x4; };
template <> struct VertexFormatOfGLM<glm::uvec2> { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Uint32x2; };
template <> struct VertexFormatOfGLM<glm::uvec3> { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Uint32x3; };
template <> struct VertexFormatOfGLM<glm::uvec4> { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Uint32x4; };

// --- 16-bit signed/unsigned (NO x3 in WebGPU) ---
template <> struct VertexFormatOfGLM<glm::i16vec2> { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Sint16x2; };
template <> struct VertexFormatOfGLM<glm::i16vec4> { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Sint16x4; };
template <> struct VertexFormatOfGLM<glm::u16vec2> { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Uint16x2; };
template <> struct VertexFormatOfGLM<glm::u16vec4> { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Uint16x4; };

// --- 8-bit signed/unsigned (NO x3 in WebGPU) ---
template <> struct VertexFormatOfGLM<glm::i8vec2>  { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Sint8x2;  };
template <> struct VertexFormatOfGLM<glm::i8vec4>  { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Sint8x4;  };
template <> struct VertexFormatOfGLM<glm::u8vec2>  { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Uint8x2;  };
template <> struct VertexFormatOfGLM<glm::u8vec4>  { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Uint8x4;  };

// --- Optional: scalar forms (handy if someone passes scalars) ---
template <> struct VertexFormatOfGLM<float>    { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Float32; };
template <> struct VertexFormatOfGLM<int32_t>  { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Sint32;  };
template <> struct VertexFormatOfGLM<uint32_t> { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Uint32;  };
template <> struct VertexFormatOfGLM<int16_t>  { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Sint16;  };
template <> struct VertexFormatOfGLM<uint16_t> { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Uint16;  };
template <> struct VertexFormatOfGLM<int8_t>   { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Sint8;   };
template <> struct VertexFormatOfGLM<uint8_t>  { static constexpr wgpu::VertexFormat value = wgpu::VertexFormat::Uint8;   };

template <typename T>
constexpr wgpu::VertexFormat DeduceVertexFormat() {
    return VertexFormatOfGLM<remove_cvref_t<T>>::value;
}

// Explicitly reject 8/16-bit vec3 (not in spec)
template <typename T> struct IsUnsupported8or16Vec3 : std::false_type {};
template <> struct IsUnsupported8or16Vec3<glm::i8vec3>  : std::true_type {};
template <> struct IsUnsupported8or16Vec3<glm::u8vec3>  : std::true_type {};
template <> struct IsUnsupported8or16Vec3<glm::i16vec3> : std::true_type {};
template <> struct IsUnsupported8or16Vec3<glm::u16vec3> : std::true_type {};

// ================= Override for normalized/packed formats =================
// Use when the C++ type can't express the normalized/packed intent.
template <typename M>
struct FormatOverride { const M* ptr; wgpu::VertexFormat fmt; };

template <typename M>
constexpr FormatOverride<M> WithFormat(const M& m, wgpu::VertexFormat f) {
    return FormatOverride<M>{ &m, f };
}

// ============================ VertexBufferLayout ============================
struct VertexBufferLayout
{
    size_t stride{};
    std::vector<wgpu::VertexAttribute> attributes;
    wgpu::VertexBufferLayout vertexBufferLayout;

    VertexBufferLayout() = default;

    template <typename T, typename... Ms>
    explicit VertexBufferLayout(const T& base, const Ms&... fields) {
        static_assert(std::is_standard_layout_v<T>, "Vertex struct must be standard-layout.");

        stride = sizeof(T);
        attributes.reserve(sizeof...(Ms));

        constexpr std::size_t kFieldCount = sizeof...(Ms);

        uint32_t location = 0;
        (addField(base, fields, location++), ...);

        vertexBufferLayout.attributeCount = static_cast<uint32_t>(attributes.size());
        vertexBufferLayout.attributes     = attributes.data();
        vertexBufferLayout.arrayStride    = static_cast<uint64_t>(stride);
        vertexBufferLayout.stepMode       = wgpu::VertexStepMode::Vertex;

    }

private:
    // Default path: deduce format from GLM type
    template <typename T, typename M>
    void addField(const T& base, const M& memberRef, uint32_t location) {
        using CleanM = remove_cvref_t<M>;

        static_assert(!IsUnsupported8or16Vec3<CleanM>::value,
            "WebGPU has no 8/16-bit x3 vertex formats. Use vec4 and ignore W, or pack differently.");

        // Enforce that a mapping exists (nice error if not).
        (void)VertexFormatOfGLM<CleanM>::value;

        const auto* pBase  = reinterpret_cast<const std::uint8_t*>(&base);
        const auto* pField = reinterpret_cast<const std::uint8_t*>(&memberRef);
        const std::size_t offset = static_cast<std::size_t>(pField - pBase);

        wgpu::VertexAttribute attr{};            // avoid aggregate-init order issues
        attr.format         = DeduceVertexFormat<CleanM>();
        attr.offset         = static_cast<uint64_t>(offset);
        attr.shaderLocation = location;
        attributes.push_back(attr);
    }

    template <typename T, typename M>
    void addField(const T& base, const FormatOverride<M>& ov, uint32_t location) {
        const auto* pBase  = reinterpret_cast<const std::uint8_t*>(&base);
        const auto* pField = reinterpret_cast<const std::uint8_t*>(ov.ptr);
        const std::size_t offset = static_cast<std::size_t>(pField - pBase);

        wgpu::VertexAttribute attr{};
        attr.format         = ov.fmt;
        attr.offset         = static_cast<uint64_t>(offset);
        attr.shaderLocation = location;
        attributes.push_back(attr);
    }
};
