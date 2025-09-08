#pragma once

#include <any>
#include <string>
#include <unordered_map>

class Buffers {
public:
    // Insert by copy/move of object
    template <class T>
    void set(const std::string& key, T&& value);

    // Construct in place (avoids extra copies)
    template <class T, class... Args>
    T& emplace(const std::string& key, Args&&... args);

    // Try to get value (returns nullptr if type/key mismatch)
    template <class T>
    T* get(const std::string& key);

    template <class T>
    const T* get(const std::string& key) const;

    // Check if key exists
    bool contains(const std::string& key) const;

    // Remove key
    void erase(const std::string& key);

    // Clear all buffers
    void clear();

private:
    std::unordered_map<std::string, std::any> storage_;
};

// ---------------- Template definitions ----------------

template <class T>
void Buffers::set(const std::string& key, T&& value) {
    storage_[key] = std::any(std::forward<T>(value));
}

template <class T, class... Args>
T& Buffers::emplace(const std::string& key, Args&&... args) {
    auto [it, _] = storage_.emplace(key, std::any{});
    it->second = T(std::forward<Args>(args)...);
    return *std::any_cast<T>(&it->second);
}

template <class T>
T* Buffers::get(const std::string& key) {
    auto it = storage_.find(key);
    if (it == storage_.end()) return nullptr;
    return std::any_cast<T>(&it->second);
}

template <class T>
const T* Buffers::get(const std::string& key) const {
    auto it = storage_.find(key);
    if (it == storage_.end()) return nullptr;
    return std::any_cast<const T>(&it->second);
}
