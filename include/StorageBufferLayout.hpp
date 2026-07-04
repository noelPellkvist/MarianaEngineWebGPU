#pragma once

#include <UniformLayout.hpp>
#include <algorithm>
#include <limits>

// WGSL storage-space layout. This intentionally mirrors UniformBufferLayout's
// public API, but uses storage-buffer alignment/stride rules.
class StorageBufferLayout {
public:
    template <typename T, typename... Ms>
    StorageBufferLayout(bool isDynamic, const T& base, const Ms&... fields) {
        m_isDynamic = isDynamic;
        static_assert(sizeof...(Ms) > 0, "Provide at least one field.");
        build_layout(base, fields...);
    }

    ~StorageBufferLayout() = default;

    template <typename T>
    std::vector<std::byte>& Pack(T& obj) {
        m_Buffer.resize(total_size_);
        pack_into(obj, m_Buffer.data(), m_Buffer.size());
        return m_Buffer;
    }

    template <typename T>
    void pack_into(const T& obj, void* dst, std::size_t dstBytes) const {
        if (dstBytes < total_size_) throw std::runtime_error("pack_into: dst too small");
        std::byte* buffer = reinterpret_cast<std::byte*>(dst);
        const std::byte* base = reinterpret_cast<const std::byte*>(&obj);

        for (const auto& e : table_) {
            for (std::size_t i = 0; i < e.count; ++i) {
                const std::byte* src = base + e.src_offset + i * e.src_stride;
                std::byte* d = buffer + e.dst_offset + i * e.dst_stride;

                if (!is_matrix(e.kind)) {
                    std::memcpy(d, src, e.copy_bytes);
                } else {
                    const int C = mat_cols(e.kind);
                    const int R = mat_rows(e.kind);
                    const std::size_t src_col_bytes = 4 * R;
                    const std::size_t dst_stride = storage_matrix_stride(e.kind);
                    for (int c = 0; c < C; ++c) {
                        std::memcpy(d + c * dst_stride, src + c * src_col_bytes, src_col_bytes);
                    }
                }
            }
        }
    }

    bool IsDynamic() const { return m_isDynamic; }
    std::size_t total_size() const { return total_size_; }

    uint32_t GetStorageStride() const { return storageStride; }
    uint32_t GetStride() const { return storageStride; }

private:
    struct Entry {
        std::size_t dst_offset;
        std::size_t src_offset;
        std::size_t copy_bytes;
        Kind kind;
        std::size_t count = 1;
        std::size_t src_stride = 0;
        std::size_t dst_stride = 0;
    };

    bool m_isDynamic{false};
    std::vector<Field> layout_;
    std::vector<Entry> table_;
    std::vector<std::byte> m_Buffer;
    uint32_t storageStride = 0;
    std::size_t total_size_ = 0;

    template<typename>
    struct is_std_array : std::false_type {};

    template<typename Elem, std::size_t N>
    struct is_std_array<std::array<Elem, N>> : std::true_type {};

    template<typename U>
    static constexpr Kind deduce_kind(const U&) {
        using Dec = std::decay_t<U>;
        static_assert(!std::is_pointer_v<Dec>, "Fields must be values/references, not pointers.");
        return map_kind<Dec>::value;
    }

    static std::size_t storage_align_of(Kind k) {
        switch (k) {
            case Kind::f32: case Kind::i32: case Kind::u32: case Kind::b32: return 4;
            case Kind::vec2: return 8;
            case Kind::vec3: [[fallthrough]];
            case Kind::vec4: return 16;
            case Kind::mat2x2: case Kind::mat3x2: case Kind::mat4x2: return 8;
            case Kind::mat2x3: case Kind::mat3x3: case Kind::mat4x3: return 16;
            case Kind::mat2x4: case Kind::mat3x4: case Kind::mat4x4: return 16;
        }
        return 0;
    }

    static std::size_t storage_vector_size(int rows) {
        switch (rows) {
            case 2: return 8;
            case 3: return 12;
            case 4: return 16;
            default: return 4;
        }
    }

    static std::size_t storage_vector_align(int rows) {
        switch (rows) {
            case 2: return 8;
            case 3: [[fallthrough]];
            case 4: return 16;
            default: return 4;
        }
    }

    static std::size_t storage_matrix_stride(Kind k) {
        const int R = mat_rows(k);
        return align_to(storage_vector_size(R), storage_vector_align(R));
    }

    static std::size_t storage_size_of(Kind k) {
        switch (k) {
            case Kind::f32: case Kind::i32: case Kind::u32: case Kind::b32: return 4;
            case Kind::vec2: return 8;
            case Kind::vec3: return 12;
            case Kind::vec4: return 16;
            case Kind::mat2x2: case Kind::mat2x3: case Kind::mat2x4:
            case Kind::mat3x2: case Kind::mat3x3: case Kind::mat3x4:
            case Kind::mat4x2: case Kind::mat4x3: case Kind::mat4x4: {
                const int C = mat_cols(k);
                const int R = mat_rows(k);
                return storage_matrix_stride(k) * (C - 1) + storage_vector_size(R);
            }
        }
        return 0;
    }

    static std::size_t storage_array_stride(Kind k) {
        return align_to(storage_size_of(k), storage_align_of(k));
    }

    template<typename T, typename U>
    static std::size_t offset_in_T(const T& base, const U& field_ref) {
        const auto* base_b = reinterpret_cast<const std::byte*>(&base);
        const auto* field_b = reinterpret_cast<const std::byte*>(&field_ref);
        std::ptrdiff_t d = field_b - base_b;
        if (d < 0 || static_cast<std::size_t>(d) >= sizeof(T)) {
            throw std::runtime_error("Field does not belong to the provided base object.");
        }
        return static_cast<std::size_t>(d);
    }

    template<typename T, typename U, typename Elem, std::size_t N>
    void add_array_one(const T& base, const U& field_ref, std::size_t& current_dst) {
        using DecElem = std::remove_cv_t<Elem>;
        static_assert(!std::is_pointer_v<DecElem>, "Array elements must be values, not pointers.");

        const Kind k = map_kind<DecElem>::value;
        const std::size_t A = storage_align_of(k);
        const std::size_t C = raw_copy_bytes(k);
        const std::size_t arrayStride = storage_array_stride(k);

        current_dst = align_to(current_dst, A);

        const std::size_t src_off = offset_in_T(base, field_ref);
        const std::size_t totalArraySize = arrayStride * N;

        layout_.push_back(Field{ k, current_dst, totalArraySize });
        table_.push_back(Entry{ current_dst, src_off, C, k, N, sizeof(DecElem), arrayStride });

        current_dst += totalArraySize;
    }

    template<typename T, typename U>
    void add_one(const T& base, const U& field_ref, std::size_t& current_dst) {
        using FieldT = std::remove_cv_t<std::remove_reference_t<U>>;
        if constexpr (is_std_array<FieldT>::value) {
            using Elem = typename FieldT::value_type;
            add_array_one<T, U, Elem, std::tuple_size_v<FieldT>>(base, field_ref, current_dst);
        }
        else if constexpr (std::is_array_v<FieldT>) {
            using Elem = std::remove_extent_t<FieldT>;
            add_array_one<T, U, Elem, std::extent_v<FieldT>>(base, field_ref, current_dst);
        }
        else {
            const Kind k = deduce_kind(field_ref);
            const std::size_t A = storage_align_of(k);
            const std::size_t S = storage_size_of(k);
            const std::size_t C = raw_copy_bytes(k);

            current_dst = align_to(current_dst, A);

            const std::size_t src_off = offset_in_T(base, field_ref);

            layout_.push_back(Field{ k, current_dst, S });
            table_.push_back(Entry{ current_dst, src_off, C, k, 1, 0, 0 });

            current_dst += S;
        }
    }

    template<typename T, typename U, typename... Rest>
    void add_all(const T& base, std::size_t& current_dst, const U& first, const Rest&... rest) {
        add_one(base, first, current_dst);
        if constexpr (sizeof...(rest) > 0) add_all(base, current_dst, rest...);
    }

    template <typename T, typename... Ms>
    void build_layout(const T& base, const Ms&... fields) {
        std::size_t current_dst = 0;
        add_all(base, current_dst, fields...);
        total_size_ = align_to(current_dst, 16);
        storageStride = static_cast<uint32_t>(align_to(total_size_, 256));
    }
};
// Layout for root storage arrays, e.g.:
// @group(0) @binding(0) var<storage, read> items: array<Item>;
class StorageArrayLayout {
public:
    template <typename T, typename... Ms>
    StorageArrayLayout(std::size_t count, const T& base, const Ms&... fields) {
        static_assert(sizeof...(Ms) > 0, "Provide at least one field for the array element.");
        if (count == 0) {
            throw std::invalid_argument("StorageArrayLayout: element count must be greater than zero");
        }
        m_ElementCount = count;
        build_element_layout(base, fields...);
        if (m_ElementStride > std::numeric_limits<std::size_t>::max() / m_ElementCount) {
            throw std::overflow_error("StorageArrayLayout: total buffer size overflows size_t");
        }
        total_size_ = m_ElementStride * m_ElementCount;
    }

    ~StorageArrayLayout() = default;

        template <typename T>
    std::vector<std::byte>& PackElement(const T& object) {
        m_Buffer.assign(m_ElementStride, std::byte{0});
        pack_element(object, m_Buffer.data(), m_ElementStride);
        return m_Buffer;
    }

    template <typename T>
    std::vector<std::byte>& PackElements(const std::vector<T>& objects) {
        if (objects.size() > m_ElementCount) throw std::runtime_error("StorageArrayLayout::PackElements: too many elements");
        m_Buffer.assign(m_ElementStride * objects.size(), std::byte{0});
        for (std::size_t i = 0; i < objects.size(); ++i) {
            pack_element(objects[i], m_Buffer.data() + i * m_ElementStride, m_ElementStride);
        }
        return m_Buffer;
    }

    template <typename T, std::size_t N>
    std::vector<std::byte>& PackElements(const std::array<T, N>& objects) {
        static_assert(N > 0, "StorageArrayLayout::PackElements requires at least one element.");
        if (N > m_ElementCount) throw std::runtime_error("StorageArrayLayout::PackElements: too many elements");
        m_Buffer.assign(m_ElementStride * N, std::byte{0});
        for (std::size_t i = 0; i < N; ++i) {
            pack_element(objects[i], m_Buffer.data() + i * m_ElementStride, m_ElementStride);
        }
        return m_Buffer;
    }

    template <typename T>
    std::vector<std::byte>& Pack(const std::vector<T>& objects) {
        return PackElements(objects);
    }

    template <typename T, std::size_t N>
    std::vector<std::byte>& Pack(const std::array<T, N>& objects) {
        return PackElements(objects);
    }

    bool IsDynamic() const { return false; }
    std::size_t total_size() const { return total_size_; }
    uint32_t GetStorageStride() const { return static_cast<uint32_t>(m_ElementStride); }
    uint32_t GetStride() const { return GetStorageStride(); }
    std::size_t GetElementCount() const { return m_ElementCount; }
    std::size_t GetElementStride() const { return m_ElementStride; }

private:
    struct Entry {
        std::size_t dst_offset;
        std::size_t src_offset;
        std::size_t copy_bytes;
        Kind kind;
        std::size_t count = 1;
        std::size_t src_stride = 0;
        std::size_t dst_stride = 0;
        std::size_t src_matrix_column_stride = 0;
    };

    std::vector<Entry> table_;
    std::vector<std::byte> m_Buffer;
    std::size_t m_ElementCount = 0;
    std::size_t m_ElementStride = 0;
    std::size_t total_size_ = 0;

    template<typename>
    struct is_std_array : std::false_type {};

    template<typename Elem, std::size_t N>
    struct is_std_array<std::array<Elem, N>> : std::true_type {};

    template<typename U>
    static constexpr Kind deduce_kind(const U&) {
        using Dec = std::decay_t<U>;
        static_assert(!std::is_pointer_v<Dec>, "Fields must be values/references, not pointers.");
        return map_kind<Dec>::value;
    }

    static std::size_t storage_align_of(Kind k) {
        switch (k) {
            case Kind::f32: case Kind::i32: case Kind::u32: case Kind::b32: return 4;
            case Kind::vec2: return 8;
            case Kind::vec3: [[fallthrough]];
            case Kind::vec4: return 16;
            case Kind::mat2x2: case Kind::mat3x2: case Kind::mat4x2: return 8;
            case Kind::mat2x3: case Kind::mat3x3: case Kind::mat4x3: return 16;
            case Kind::mat2x4: case Kind::mat3x4: case Kind::mat4x4: return 16;
        }
        return 0;
    }

    static std::size_t storage_vector_size(int rows) {
        switch (rows) {
            case 2: return 8;
            case 3: return 12;
            case 4: return 16;
            default: return 4;
        }
    }

    static std::size_t storage_vector_align(int rows) {
        switch (rows) {
            case 2: return 8;
            case 3: [[fallthrough]];
            case 4: return 16;
            default: return 4;
        }
    }

    static std::size_t storage_matrix_stride(Kind k) {
        const int R = mat_rows(k);
        return align_to(storage_vector_size(R), storage_vector_align(R));
    }

    static std::size_t storage_size_of(Kind k) {
        switch (k) {
            case Kind::f32: case Kind::i32: case Kind::u32: case Kind::b32: return 4;
            case Kind::vec2: return 8;
            case Kind::vec3: return 12;
            case Kind::vec4: return 16;
            case Kind::mat2x2: case Kind::mat2x3: case Kind::mat2x4:
            case Kind::mat3x2: case Kind::mat3x3: case Kind::mat3x4:
            case Kind::mat4x2: case Kind::mat4x3: case Kind::mat4x4: {
                const int C = mat_cols(k);
                return storage_matrix_stride(k) * C;
            }
        }
        return 0;
    }

    static std::size_t storage_array_stride(Kind k) {
        return align_to(storage_size_of(k), storage_align_of(k));
    }

    template<typename T, typename U>
    static std::size_t offset_in_T(const T& base, const U& field_ref) {
        const auto* base_b = reinterpret_cast<const std::byte*>(&base);
        const auto* field_b = reinterpret_cast<const std::byte*>(&field_ref);
        std::ptrdiff_t d = field_b - base_b;
        if (d < 0 || static_cast<std::size_t>(d) >= sizeof(T)) {
            throw std::runtime_error("Field does not belong to the provided base object.");
        }
        return static_cast<std::size_t>(d);
    }

    template <typename T>
    void pack_element(const T& obj, void* dst, std::size_t dstBytes) const {
        if (dstBytes < m_ElementStride) throw std::runtime_error("StorageArrayLayout::pack_element: dst too small");
        std::byte* buffer = reinterpret_cast<std::byte*>(dst);
        const std::byte* base = reinterpret_cast<const std::byte*>(&obj);

        for (const auto& e : table_) {
            for (std::size_t i = 0; i < e.count; ++i) {
                const std::byte* src = base + e.src_offset + i * e.src_stride;
                std::byte* d = buffer + e.dst_offset + i * e.dst_stride;

                if (!is_matrix(e.kind)) {
                    std::memcpy(d, src, e.copy_bytes);
                } else {
                    const int C = mat_cols(e.kind);
                    const int R = mat_rows(e.kind);
                    const std::size_t src_col_bytes = 4 * R;
                    const std::size_t dst_stride = storage_matrix_stride(e.kind);
                    for (int c = 0; c < C; ++c) {
                        std::memcpy(d + c * dst_stride,
                                    src + c * e.src_matrix_column_stride,
                                    src_col_bytes);
                    }
                }
            }
        }
    }

    template<typename T, typename U, typename Elem, std::size_t N>
    void add_array_one(const T& base, const U& field_ref, std::size_t& current_dst) {
        using DecElem = std::remove_cv_t<Elem>;
        static_assert(!std::is_pointer_v<DecElem>, "Array elements must be values, not pointers.");
        static_assert(!std::is_same_v<DecElem, bool>,
                      "WGSL bool is not host-shareable; use uint32_t/u32 in storage buffers.");

        const Kind k = map_kind<DecElem>::value;
        const std::size_t A = storage_align_of(k);
        const std::size_t C = raw_copy_bytes(k);
        const std::size_t arrayStride = storage_array_stride(k);

        current_dst = align_to(current_dst, A);

        const std::size_t src_off = offset_in_T(base, field_ref);
        const std::size_t srcMatrixColumnStride = is_matrix(k)
            ? sizeof(DecElem) / static_cast<std::size_t>(mat_cols(k))
            : 0;
        table_.push_back(Entry{ current_dst, src_off, C, k, N, sizeof(DecElem),
                                arrayStride, srcMatrixColumnStride });

        current_dst += arrayStride * N;
    }

    template<typename T, typename U>
    void add_one(const T& base, const U& field_ref, std::size_t& current_dst) {
        using FieldT = std::remove_cv_t<std::remove_reference_t<U>>;
        if constexpr (is_std_array<FieldT>::value) {
            using Elem = typename FieldT::value_type;
            add_array_one<T, U, Elem, std::tuple_size_v<FieldT>>(base, field_ref, current_dst);
        }
        else if constexpr (std::is_array_v<FieldT>) {
            using Elem = std::remove_extent_t<FieldT>;
            add_array_one<T, U, Elem, std::extent_v<FieldT>>(base, field_ref, current_dst);
        }
        else {
            static_assert(!std::is_same_v<FieldT, bool>,
                          "WGSL bool is not host-shareable; use uint32_t/u32 in storage buffers.");
            const Kind k = deduce_kind(field_ref);
            const std::size_t A = storage_align_of(k);
            const std::size_t S = storage_size_of(k);
            const std::size_t C = raw_copy_bytes(k);

            current_dst = align_to(current_dst, A);

            const std::size_t src_off = offset_in_T(base, field_ref);
            const std::size_t srcMatrixColumnStride = is_matrix(k)
                ? sizeof(FieldT) / static_cast<std::size_t>(mat_cols(k))
                : 0;
            table_.push_back(Entry{ current_dst, src_off, C, k, 1, 0, 0,
                                    srcMatrixColumnStride });

            current_dst += S;
        }
    }

    template<typename T, typename U, typename... Rest>
    void add_all(const T& base, std::size_t& current_dst, const U& first, const Rest&... rest) {
        add_one(base, first, current_dst);
        if constexpr (sizeof...(rest) > 0) add_all(base, current_dst, rest...);
    }

    template <typename T, typename... Ms>
    void build_element_layout(const T& base, const Ms&... fields) {
        std::size_t current_dst = 0;
        add_all(base, current_dst, fields...);

        std::size_t structAlignment = 1;
        for (const Entry& entry : table_) {
            structAlignment = std::max(structAlignment, storage_align_of(entry.kind));
        }
        m_ElementStride = align_to(current_dst, structAlignment);
    }
};


