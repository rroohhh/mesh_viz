#include "inverted_index.h"

#include <ranges>

template<class T>
using simtime_t = InvertedIndex<T>::simtime_t;

template<class T>
std::unordered_map<T, WaveDatabase> gen_posting_list(std::span<const T> values, std::span<const simtime_t<T>> times) {
  std::unordered_map<T, std::vector<WaveValue>> inverted_index;

  assert(values.size() == times.size());

  for (auto [v, t] : std::views::zip(values, times)) {
    auto [entry, _] = inverted_index.try_emplace(v);
    entry->second.push_back(WaveValue{.timestamp = t, .type = WaveValueType::Zero /*hack, we do not care about this value*/});
  }

  std::unordered_map<T, WaveDatabase> posting_list;
  for (auto & [value, times] : inverted_index) {
    posting_list.emplace(std::piecewise_construct, std::forward_as_tuple(value), std::forward_as_tuple(times, true /* jumpy */));
  }

  return posting_list;
}

template<class T>
std::vector<T> get_keys(const std::unordered_map<T, WaveDatabase> & posting_list) {
  std::vector<T> res;
  for (auto & [key, _] : posting_list) {
    res.push_back(key);
  }
  return res;
}


template<class T>
InvertedIndex<T>::InvertedIndex(std::span<const T> values, std::span<const InvertedIndex::simtime_t> times) : posting_list(gen_posting_list(values, times)), keys(get_keys(posting_list)) {}

template struct InvertedIndex<uint32_t>;
