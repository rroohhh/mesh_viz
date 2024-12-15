#pragma once

#include <unordered_map>
#include <span>
#include "wave_data_base.h"

template<class T >
struct InvertedIndex {
public:
  using simtime_t = uint32_t;
  using value_t = T;
  // TODO(robin): this abuses WaveDatabase a bit, make a specialized version with only the times?
  std::unordered_map<T, WaveDatabase> posting_list;
  std::vector<T> keys;
public:
  InvertedIndex(std::span<const T> values, const std::span<const simtime_t> times);
};
