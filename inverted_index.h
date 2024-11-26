#include <unordered_map>
#include <vector>
#include "wave_data_base.h"

template<class T >
struct InvertedIndex {
public:
  using simtime_t = uint32_t;
  // TODO(robin): this abuses WaveDatabase a bit, make a specialized version with only the times?
  std::unordered_map<T, WaveDatabase> posting_list;

public:
  InvertedIndex(const std::vector<T> & values, const std::vector<simtime_t> &times);

private:
  std::unordered_map<T, WaveDatabase> gen_posting_list(const std::vector<T> & values, const std::vector<simtime_t> &times);
};
