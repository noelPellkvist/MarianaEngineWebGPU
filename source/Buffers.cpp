#include <Buffers.hpp>

bool Buffers::contains(const std::string& key) const {
    return storage_.find(key) != storage_.end();
}

void Buffers::erase(const std::string& key) {
    storage_.erase(key);
}

void Buffers::clear() {
    storage_.clear();
}
