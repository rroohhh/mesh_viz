#pragma once

#include "libfst/fstapi.h"
#include <vector>

using handle_t = fstHandle;
using simtime_t = uint32_t;
using simtimedelta_t = int64_t;

#include "node.h"
#include "wave_data_base.h"

using value_change_cb_t =
    std::function<void(uint64_t time, handle_t facidx, const unsigned char* value)>;

struct FstFile;
struct FstFile
{
	using fstReader = void*;
	fstReader reader;
	std::string filename;
	// value at time requires one passes in a char buffer. We can statically know
	// how long this has to be by reading the nodes.
	mutable std::vector<char> value_buffer;

	~FstFile();

	FstFile(const char* path);
	// copies the context for use on another thread for example
	FstFile(const FstFile & other);

	std::vector<std::shared_ptr<Node>> read_nodes();

	static void value_change_callback(
	    void* user_callback_data_pointer,
	    uint64_t time,
	    handle_t facidx,
	    const unsigned char* value);

	static void value_change_callback2(
	    void* user_callback_data_pointer,
	    uint64_t time,
	    handle_t facidx,
	    const unsigned char* value,
	    uint32_t len);

	void read_changes(
	    uint64_t min_time,
	    uint64_t max_time,
	    const std::vector<NodeVar>& vars,
	    value_change_cb_t cb) const;

	WaveDatabase read_wave_db(NodeVar var) const;

	uint64_t min_time() const;

	uint64_t max_time() const;

	char* get_value_at(handle_t handle, uint64_t time) const;

	template<class T>
	std::vector<T> read_values(NodeVar var) const;

	template<class T>
	std::pair<std::vector<simtime_t>, std::vector<T>> read_values(const NodeVar& var, const NodeVar& sampling_var, std::span<const NodeVar> conditions, std::span<const NodeVar> masks) const;
};
