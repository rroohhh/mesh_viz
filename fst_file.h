#pragma once

#include "libfst/fstapi.h"
#include "node_var.h"
#include "wave_data_base.h"
#include "core.h"
#include "lru_cache.h"
#include "fst_reader.h"

#include <vector>

using handle_t = fstHandle;

struct WaveformViewer;
struct Histograms;
struct SignalFlowTraces;
struct AsyncRunner;
struct Node;

struct FstFile: public std::enable_shared_from_this<FstFile>
{
	using fstReader = void*;
	fstReader reader;
	std::string filename;
	// value at time requires one passes in a char buffer. We can statically know
	// how long this has to be by reading the nodes.
	mutable std::vector<char> value_buffer;

	// using bit_type_t = uint8_t;
	using bit_type_t = uint8_t;
	// TODO(robin): this is quite hacky
	mutable LruCache<handle_t, std::valarray<bit_type_t>> cache;

	FstReader fast_reader;

	~FstFile();

	FstFile(const char* path);

	// TODO(robin): consider making this deleted to avoid implicit copies
	// copies the context for use on another thread for example
	// warning: base class ‘class std::enable_shared_from_this<FstFile>’ should be explicitly initialized in the copy constructor [-Wextra]
	// 168 | FstFile::FstFile(const FstFile& other) :
	//     | ^~~~~~~
	FstFile(const FstFile & other);

	std::vector<std::shared_ptr<Node>> read_nodes(WaveformViewer * waveform_viewer, Histograms * histograms, SignalFlowTraces *signal_flow_traces, AsyncRunner * async_runner);

	WaveDatabase read_wave_db(NodeVar var) const;

	uint64_t min_time() const;

	uint64_t max_time() const;

	char* get_value_at(const NodeVar & var, uint64_t time) const;

	template<class T, class O = std::vector<T>>
	O read_values(const NodeVar & var) const;


	template<class T>
	std::pair<std::vector<simtime_t>, std::vector<T>> read_values(const NodeVar& var, const NodeVar& sampling_var, std::vector<NodeVar> conditions, std::vector<NodeVar> masks, bool negedge = false) const;

	private:
	template<class T, class O = std::vector<T>, int nbits = 0>
	O read_values_inner(const NodeVar & var) const;
};
