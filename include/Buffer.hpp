#pragma once
#include <memory>
#include <variant>
#include <UniformLayout.hpp>
#include <StorageBufferLayout.hpp>

class Shader2;

enum class BufferType
{
    Uniform,
    Storage
};

class Buffer
{
    friend class Shader2;
public:
    using BufferLayout = std::variant<UniformBufferLayout, StorageBufferLayout, StorageArrayLayout>;

    Buffer(UniformBufferLayout layout);
    Buffer(StorageBufferLayout layout);
    Buffer(StorageArrayLayout layout);
    ~Buffer() = default;

    void Build();

    const BufferLayout& GetLayout() const { return m_Layout; }
    BufferType GetType() const { return m_Type; }
    bool IsDynamic() const { return m_IsDynamic; }
    uint64_t GetSize() const { return m_Size; }
    uint64_t GetStride() const;

    void WriteRaw(const void* data, uint64_t size, uint64_t index = 0);
    void WriteRawBytes(const void* data, uint64_t size, uint64_t byteOffset = 0);

    template <typename T>
    void Write(const T& obj, uint64_t index = 0)
    {
        std::visit(
            [&](auto& layout) {
                using LayoutT = std::decay_t<decltype(layout)>;

                if constexpr (std::is_same_v<LayoutT, StorageArrayLayout>) {
                    std::vector<std::byte>* data = nullptr;

                    if constexpr (requires { layout.PackElements(obj); }) {
                        data = &layout.PackElements(obj);
                    } else if constexpr (requires { layout.PackElement(obj); }) {
                        data = &layout.PackElement(obj);
                    } else {
                        throw std::runtime_error("Buffer::Write: object type is not compatible with StorageArrayLayout");
                    }

                    const uint64_t elementStride = layout.GetElementStride();
                    const uint64_t elementCount = elementStride == 0 ? 0 : data->size() / elementStride;
                    if (index + elementCount > layout.GetElementCount()) {
                        throw std::runtime_error("Buffer::Write: storage array write exceeds buffer element count");
                    }
                    if (!data->empty()) {
                        WriteRawBytes(data->data(), data->size(), index * elementStride);
                    }
                } else {
                    std::vector<std::byte>* data = nullptr;

                    if constexpr (requires { layout.Pack(const_cast<T&>(obj)); }) {
                        data = &layout.Pack(const_cast<T&>(obj));
                    } else if constexpr (requires { layout.Pack(obj); }) {
                        data = &layout.Pack(obj);
                    } else {
                        throw std::runtime_error("Buffer::Write: object type is not compatible with this buffer layout");
                    }

                    WriteRaw(data->data(), data->size(), index);
                }
            },
            m_Layout);
    }

private:
    struct Impl;
    std::shared_ptr<Impl> _impl;
    BufferLayout m_Layout;
    BufferType m_Type{BufferType::Uniform};

    uint64_t m_Size{0};
    bool m_IsDynamic{false};

    const void* GetBuffer() const;
    uint64_t GetBindingSize() const { return m_IsDynamic ? GetStride() : m_Size; }
};



