#pragma once

#include <cereal/access.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/vector.hpp>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace caramel {

// Open-addressing hash map (linear probing, power-of-two capacity). Three flat
// arrays instead of a node per entry: building, probing and freeing millions
// of entries costs a few ns each, where std::unordered_map allocates and
// chases a pointer per entry.
template <typename K, typename V> class FlatMap {
public:
  FlatMap() = default;

  size_t size() const { return _size; }
  bool empty() const { return _size == 0; }

  void reserve(size_t n) {
    size_t cap = 16;
    while (cap * 7 / 8 < n) {
      cap *= 2;
    }
    if (cap > _keys.size()) {
      rehash(cap);
    }
  }

  void clear() { *this = FlatMap(); }

  V &operator[](const K &key) {
    if ((_size + 1) * 8 > _keys.size() * 7) {
      rehash(_keys.empty() ? 16 : _keys.size() * 2);
    }
    size_t i = slot(key);
    if (!_used[i]) {
      _used[i] = 1;
      _keys[i] = key;
      _values[i] = V{};
      _size++;
    }
    return _values[i];
  }

  const V *find(const K &key) const {
    if (_keys.empty()) {
      return nullptr;
    }
    const size_t i = slot(key);
    return _used[i] ? &_values[i] : nullptr;
  }

  const V &at(const K &key) const {
    const V *v = find(key);
    if (!v) {
      throw std::out_of_range("FlatMap::at: key not found");
    }
    return *v;
  }

  size_t count(const K &key) const { return find(key) ? 1 : 0; }

  bool operator==(const FlatMap &other) const {
    if (_size != other._size) {
      return false;
    }
    bool equal = true;
    forEach([&](const K &k, const V &v) {
      const V *o = other.find(k);
      equal = equal && o && *o == v;
    });
    return equal;
  }

  // Calls f(key, value) for every entry, in slot order.
  template <typename F> void forEach(F &&f) const {
    for (size_t i = 0; i < _keys.size(); i++) {
      if (_used[i]) {
        f(_keys[i], _values[i]);
      }
    }
  }

  template <typename F> void forEachMutable(F &&f) {
    for (size_t i = 0; i < _keys.size(); i++) {
      if (_used[i]) {
        f(_keys[i], _values[i]);
      }
    }
  }

  std::vector<std::pair<K, V>> entries() const {
    std::vector<std::pair<K, V>> out;
    out.reserve(_size);
    forEach([&](const K &k, const V &v) { out.emplace_back(k, v); });
    return out;
  }

  // The slot a key hashes to: equal keys of different maps with the same
  // capacity agree, which lets callers partition entries by hash.
  static uint64_t hash(const K &key) {
    return static_cast<uint64_t>(std::hash<K>{}(key)) * 0x9E3779B97F4A7C15ULL;
  }

private:
  size_t slot(const K &key) const {
    const size_t mask = _keys.size() - 1;
    size_t i = static_cast<size_t>(hash(key) >> _shift) & mask;
    while (_used[i] && !(_keys[i] == key)) {
      i = (i + 1) & mask;
    }
    return i;
  }

  void rehash(size_t cap) {
    std::vector<K> keys(cap);
    std::vector<V> values(cap);
    std::vector<uint8_t> used(cap, 0);
    keys.swap(_keys);
    values.swap(_values);
    used.swap(_used);
    _shift = 64 - __builtin_ctzll(cap);
    for (size_t i = 0; i < used.size(); i++) {
      if (used[i]) {
        const size_t j = slot(keys[i]);
        _used[j] = 1;
        _keys[j] = std::move(keys[i]);
        _values[j] = std::move(values[i]);
      }
    }
  }

  friend class cereal::access;
  template <class Archive> void save(Archive &archive) const {
    archive(entries());
  }
  template <class Archive> void load(Archive &archive) {
    std::vector<std::pair<K, V>> e;
    archive(e);
    clear();
    reserve(e.size());
    for (auto &[k, v] : e) {
      (*this)[k] = std::move(v);
    }
  }

  std::vector<K> _keys;
  std::vector<V> _values;
  std::vector<uint8_t> _used;
  size_t _size = 0;
  unsigned _shift = 64;
};

} // namespace caramel
