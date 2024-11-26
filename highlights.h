#include <vector>
#include "wave_data_base.h"
#include "node.h"

// highlight for one specific NodeVar
struct Highlight {
private:
  std::vector<std::shared_ptr<std::vector<WaveDatabase *>>> highlights;
public:
  Highlight(std::vector<std::shared_ptr<std::vector<WaveDatabase *>>> highlights) : highlights(highlights) {}

  // returns true if a highlighted value for var lies in [start, end)
  bool should_highlight(simtime_t start, simtime_t end) {
    for (auto & batch : highlights) {
      for (auto & highlight : *batch) {
        auto s = highlight->jump_to(WaveValue{.timestamp = start, .type = WaveValueType::Zero});
        if (s and s->timestamp < end) { return true; }
      }
    }
    return false;
  }
};

struct Highlights {
private:
  using EntryT = std::vector<std::weak_ptr<std::vector<WaveDatabase *>>>;
  std::unordered_map<NodeVar::handle_t, EntryT> highlights;

public:
  void add(const NodeVar & var, std::weak_ptr<std::vector<WaveDatabase *>> var_highlights) {
    auto [it, _] = highlights.try_emplace(var.handle);
    it->second.push_back(var_highlights);
  }

  Highlight highlight_for(const NodeVar & var) {
    auto it = highlights.find(var.handle);
    if (it == highlights.end()) {
      return Highlight{{}};
    } else {
      std::vector<std::shared_ptr<std::vector<WaveDatabase *>>> var_highlights;
      auto write_it = it->second.begin();
      auto read_it = it->second.begin();
      auto end = it->second.end();

      while (read_it != end) {
        if (auto locked = read_it->lock()) {
          var_highlights.push_back(locked);
          *write_it = *read_it;
          write_it++;
        }

        read_it++;
      }

      return Highlight{var_highlights};
    }
  }
};
