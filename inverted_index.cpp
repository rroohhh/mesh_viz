#include "inverted_index.h"

template<class T>
InvertedIndex<T>::InvertedIndex(const std::vector<T> & values, const std::vector<InvertedIndex::simtime_t> &times) : posting_list(gen_posting_list(values, times)), keys(get_keys(posting_list)) {}

template<class T>
std::unordered_map<T, WaveDatabase> InvertedIndex<T>::gen_posting_list(const std::vector<T> & values, const std::vector<simtime_t> &times) {
  std::unordered_map<T, std::vector<WaveValue>> inverted_index;

  assert(values.size() == times.size());

  for (auto [v, t] : std::views::zip(values, times)) {
    auto [entry, _] = inverted_index.try_emplace(v);
    entry->second.push_back(WaveValue{.timestamp = t, .type = WaveValueType::Zero /*hack, we do not care about this value*/});
  }

  std::unordered_map<T, WaveDatabase> posting_list;
  for (auto & [value, times] : inverted_index) {
    posting_list.emplace(value, times);
  }

  return posting_list;
}

template<class T>
std::vector<T> InvertedIndex<T>::get_keys(const std::unordered_map<T, WaveDatabase> & posting_list) {
  std::vector<T> res;
  for (auto & [key, _] : posting_list) {
    res.push_back(key);
  }
  return res;
}

template struct InvertedIndex<uint64_t>;
