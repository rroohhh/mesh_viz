#include "inverted_index.h"
#include "node.h"
#include "wave_data_base.h"
#include <future>
#include <optional>
#include <vector>
#include <unordered_map>

#include "implot.h"

struct FstFile;
struct Highlights;
struct HighlightEntries;

struct Histogram
{
private:
	Highlights* highlights;

	// this is for the normal histogram flow
	NodeVar var;
	NodeVar sampling_var;
	std::vector<NodeVar> conditions;
	std::vector<NodeVar> masks;

	// this is for a histogram that is purely based on a list of times and values, maybe calculated by python or something
	std::vector<NodeVar> extra;

	std::unordered_map<NodeID, bool> to_highlight;

	bool open = true;
	using DataT = InvertedIndex<uint64_t>;
	std::future<DataT> data_future;
	std::optional<DataT> data = std::nullopt;
	std::optional<ImPlotRect> query = std::nullopt;
	std::optional<std::shared_ptr<HighlightEntries>> highlighted = std::nullopt;
	friend Histograms;

	ImVec4 color = ImVec4(0.35, 0.16, 0.93, 0.5);

	void set_query(decltype(query) new_value);
	void update_query();

	bool HighlightCheckbox(const NodeVar & var, const char * category, bool default_open = false);

	void update_highlights();

public:
	Histogram(const NodeVar& var, Highlights* highlights, FstFile* fstfile, const NodeVar& sampling_var, std::vector<NodeVar> conditions, std::vector<NodeVar> masks, bool negedge);

	Histogram(Highlights* highlights, FstFile* fstfile, const NodeVar& sampling_var, std::vector<NodeVar> conditions, std::vector<NodeVar> masks, bool negedge);

	bool render(int id);
};

struct Histograms
{
private:
	std::vector<std::pair<Histogram, int>> histograms;
	int id_gen = 0;
	FstFile* fstfile;
	Highlights* highlights;

public:
	Histograms(FstFile* fstfile, Highlights* highlights);
	void add(const NodeVar& var, const NodeVar& sampling_var, std::vector<NodeVar> conditions, std::vector<NodeVar> masks, bool negedge);

	void render();
};
