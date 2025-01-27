#pragma once

#include "inverted_index.h"
#include "node_var.h"
#include <future>
#include <optional>
#include <utility>
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
	std::optional<NodeVar> var;
	std::optional<NodeVar> sampling_var;
	std::vector<NodeVar> conditions;
	std::vector<NodeVar> masks;

	// this is for a histogram that is purely based on a list of times and values, maybe calculated by python or something
	std::vector<NodeVar> extra;
	std::string extra_name;

	std::unordered_map<NodeID, bool> to_highlight;

	bool open = true;
	using DataT = InvertedIndex<uint32_t>;
	std::future<DataT> data_future;
	std::optional<DataT> data = std::nullopt;
	std::optional<ImPlotRect> query = std::nullopt;
	std::optional<std::shared_ptr<HighlightEntries>> highlighted = std::nullopt;
	friend struct Histograms;

	std::unique_ptr<WaveDatabase> small_wdb_opt;

	ImVec4 color = ImVec4(0.35, 0.16, 0.93, 0.5);

	void set_query(decltype(query) new_value);
	void update_query();

	bool HighlightCheckbox(const NodeVar & var, const char * category, bool default_open = false);

	void update_highlights();

public:
	Histogram(Highlights* highlights, std::shared_ptr<FstFile> fstfile, const NodeVar& var, const NodeVar& sampling_var, std::vector<NodeVar> conditions, std::vector<NodeVar> masks, bool negedge);
	Histogram(Highlights* highlights, std::shared_ptr<FstFile> fstfile, std::string name, std::vector<NodeVar> used, std::span<const DataT::simtime_t> times, std::span<const DataT::value_t> values);

	bool render(int id);
};

struct Histograms
{
private:
	std::vector<std::pair<Histogram, int>> histograms;
	int id_gen = 0;
	std::shared_ptr<FstFile> fstfile;
	Highlights* highlights;

public:
	Histograms(std::shared_ptr<FstFile> fstfile, Highlights* highlights);

	using DataT = Histogram::DataT;

	template<class... Args>
	void add(Args && ...args) {
		histograms.emplace_back(std::piecewise_construct, std::forward_as_tuple(highlights, fstfile, std::forward<Args>(args)...), std::forward_as_tuple(id_gen++));
	}

	void render();
};
