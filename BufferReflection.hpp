#pragma once
#include <tuple>
#include <cstddef>
#include <iostream>

template<typename Struct, typename FieldType>
struct BufferEntryInfo {
    const char* name;
    FieldType Struct::* member;
};

#define BUFFER_ENTRY_INFO(StructType, member) BufferEntryInfo<StructType, decltype(StructType::member)>{#member, &StructType::member}

#define REGISTER_BUFFER_ENTRIES(...) \
    static constexpr auto get_entries() { \
        return std::make_tuple(__VA_ARGS__); \
    }

// Move these template definitions to the header!
template<typename Struct, typename FieldType>
constexpr std::size_t member_offset(FieldType Struct::* member) {
    return reinterpret_cast<std::size_t>(
        &(reinterpret_cast<Struct const volatile*>(0)->*member)
    );
}

template<typename Struct, typename... Fields>
void RegisterVertexBuffer(const Struct& s, std::tuple<Fields...> fields) {
    std::cout << "Struct size: " << sizeof(Struct) << " bytes\n";
    std::apply([&](auto... BufferEntryInfos) {
        ((std::cout 
            << "Field name: " << BufferEntryInfos.name
            << ", offset: " << member_offset<Struct>(BufferEntryInfos.member)
            << ", size: " << sizeof(s.*(BufferEntryInfos.member)) << std::endl
        ), ...);
    }, fields);
}