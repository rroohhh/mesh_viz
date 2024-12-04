#include <vector>
#include "wave_data_base.h"
#include "node.h"

struct HighlightEntries {
  uint32_t color;
  std::vector<WaveDatabase *> dbs;
};

// highlight for one specific NodeVar
struct Highlight {
private:
  std::vector<std::shared_ptr<HighlightEntries>> highlights;
public:
	Highlight(const decltype(highlights)& highlights);

	// returns true if a highlighted value for var lies in [start, end) plus the highlight color
	std::pair<bool, uint32_t> should_highlight(simtime_t start, simtime_t end);
};

struct Highlights {
private:
  using EntryT = std::weak_ptr<HighlightEntries>;
  std::unordered_map<NodeID, std::vector<EntryT>> highlights;

public:
	void add(const NodeVar& var, EntryT var_highlights);
	void add(NodeID var, EntryT var_highlights);

	void remove(const NodeVar& var, EntryT var_highlights);
	void remove(NodeID var, EntryT var_highlights);

	Highlight highlight_for(const NodeVar& var);
};
