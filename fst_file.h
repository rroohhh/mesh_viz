#pragma once

#include "libfst/fstapi.h"
#include "node_var.h"
#include "wave_data_base.h"
#include "core.h"
#include "lru_cache.h"
#include "fst_reader.h"

#include <vector>

using handle_t = fstHandle;

// using value_change_cb_t =
//     std::function<void(uint64_t time, handle_t facidx, const unsigned char* value)>;

struct WaveformViewer;
struct Histograms;
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
	FstFile(const FstFile & other);

	std::vector<std::shared_ptr<Node>> read_nodes(WaveformViewer * waveform_viewer, Histograms * histograms, AsyncRunner * async_runner);

	// template<class T>
	// void read_changes(
	//     uint64_t min_time,
	//     uint64_t max_time,
	//     const std::vector<NodeVar>& vars,
	//     T cb) const {
	// 	fstReaderClrFacProcessMaskAll(reader);
	// 	for (const auto& var : vars) {
	// 		fstReaderSetFacProcessMask(reader, var.handle);
	// 	}
	// 	fstReaderIterBlocksSetNativeDoublesOnCallback(reader, 1);
	// 	fstReaderSetLimitTimeRange(reader, min_time, max_time);

	// 	void (* value_change_callback)(
	// 		void* user_callback_data_pointer,
	// 		uint64_t time,
	// 		handle_t facidx,
	// 		const unsigned char* value) = [](void* user_callback_data_pointer,
	// 		uint64_t time,
	// 		handle_t facidx,
	// 		const unsigned char* value) {
	// 		(*((T *) user_callback_data_pointer))(time, facidx, value);
	// 	};

	// 	void (* value_change_callback2)(
	// 		void* user_callback_data_pointer,
	// 		uint64_t time,
	// 		handle_t facidx,
	// 		const unsigned char* value,
	// 		uint32_t) = [] (void* user_callback_data_pointer,
	// 		uint64_t time,
	// 		handle_t facidx,
	// 		const unsigned char* value,
	// 		uint32_t) {
	// 		(*((T *) user_callback_data_pointer))(time, facidx, value);
	// 	};

	// 	fstReaderIterBlocks2(
	// 		reader, value_change_callback, value_change_callback2, &cb, nullptr);
	// }

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
