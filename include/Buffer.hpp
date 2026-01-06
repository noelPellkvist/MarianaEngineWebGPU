#pragma once
#include <memory>
#include <UniformLayout.hpp>

class Shader2;

class Buffer
{
    friend class Shader2;
public:
    Buffer(UniformBufferLayout layout);
    ~Buffer() = default;

    void Build();

    const UniformBufferLayout& GetLayout() const { return m_Layout; }

    void WriteRaw(const void* data, uint64_t size, uint64_t index = 0);

    template <typename T>
    void Write(const T& obj, uint64_t index = 0)
    {
        std::vector<std::byte> data = m_Layout.Pack(const_cast<T&>(obj));
        WriteRaw(data.data(), data.size(), index);
    }

private:
    struct Impl;
    std::shared_ptr<Impl> _impl;
    UniformBufferLayout m_Layout;

    uint64_t m_Size{0};
    bool m_IsDynamic{false};

    const void* GetBuffer() const;
    const uint64_t GetBindingSize() const { return m_Layout.IsDynamic() ? m_Layout.GetUniformStride() : m_Size; }
};