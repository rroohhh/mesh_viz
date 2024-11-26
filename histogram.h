#include "inverted_index.h"
#include "node.h"
#include "wave_data_base.h"
#include <future>
#include <optional>
#include <vector>

#include "implot.h"

struct FstFile;
struct Highlights;

struct Histogram
{
private:
	Highlights* highlights;
	NodeVar var;
	bool open = true;
	using DataT = InvertedIndex<uint64_t>;
	std::future<DataT> data_future;
	std::optional<DataT> data = std::nullopt;
	std::optional<ImPlotRect> query = std::nullopt;
	std::optional<std::shared_ptr<std::vector<WaveDatabase*>>> highlighted = std::nullopt;
	friend Histograms;

	void set_query(decltype(query) new_value);
	void update_query();

public:
	Histogram(const NodeVar& var, Highlights* highlights, FstFile* fstfile, const NodeVar& sampling_var, std::span<const NodeVar> conditions, std::span<const NodeVar> masks);

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
	void add(const NodeVar& var, const NodeVar& sampling_var, std::span<const NodeVar> conditions, std::span<const NodeVar> masks);

	void render();
};
