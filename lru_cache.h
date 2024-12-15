#include <unordered_map>

template <class KeyT, class DataT>
class LruCache {
  using age_t = uint64_t;

  size_t cap;
  age_t clock = 1;
  std::vector<KeyT> usage;
  std::unordered_map<KeyT, std::pair<age_t, DataT>> data;

public:
  LruCache(size_t cap) : cap(cap) {}

  const DataT* get(KeyT key) {
    auto it = data.find(key);
    if (it != data.end()) {
      it->second.first = clock++;
      return &it->second.second;
    } else {
      return nullptr;
    }
  }

  void add(KeyT key, DataT new_data) {
    if (usage.size() > cap) {
      KeyT to_remove;
      age_t oldest = std::numeric_limits<age_t>::max();
      for (auto & [key, value] : data) {
        auto & [age, _] = value;
        if (age < oldest) {
          to_remove = key;
          oldest = age;
        }
      }
      data.erase(to_remove);
    }
    data.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(0, new_data));
  }
};
