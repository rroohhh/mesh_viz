#include "highlights.h"

#include <print>

template<class T>
bool owner_equals(std::weak_ptr<T> &lhs, std::weak_ptr<T> &rhs) {
    return !lhs.owner_before(rhs) && !rhs.owner_before(lhs);
}

Highlight Highlights::highlight_for(const NodeVar& var)
{
	auto it = highlights.find(var.stable_id());
	if (it == highlights.end()) {
		return Highlight{{}};
	} else {
		std::vector<std::shared_ptr<HighlightEntries>> var_highlights;
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

void Highlights::remove(const NodeVar& var, EntryT var_highlights)
{
  std::println("removing from {}", var.pretty_name());
  remove(var.stable_id(), var_highlights);
}

void Highlights::remove(NodeID handle, EntryT var_highlights)
{
	auto [it, _] = highlights.try_emplace(handle);
	std::erase_if(it->second, [&](auto highlight) {
      return owner_equals(highlight, var_highlights);
    });
}

void Highlights::add(const NodeVar& var, EntryT var_highlights)
{
  std::println("adding to {}", var.pretty_name());
  add(var.stable_id(), var_highlights);
}

void Highlights::add(NodeID id, EntryT var_highlights)
{
	auto [it, _] = highlights.try_emplace(id);
	it->second.push_back(var_highlights);
}

Highlight::Highlight(const decltype(highlights)& highlights) : highlights(highlights) {}

std::tuple<bool, simtime_t, simtime_t> Highlight::should_highlight(simtime_t start, simtime_t end)
{
	simtime_t min = std::numeric_limits<simtime_t>::max();
	for (auto& batch : highlights) {
		for (auto& highlight : batch->dbs) {
			auto s = highlight->jump_to(WaveValue{.timestamp = start, .type = WaveValueType::Zero});
			if (s and s->timestamp < end) {
				return {true, batch->color, min};
			} else {
				min = min > s->timestamp ? s->timestamp : min;
			}
		}
	}
	return {false, 0, min};
}
