namespace impl {

#include <zlib.h>

template <typename F, bool takes_ownership = false>
void with_maybe_uncompress(F&& f, const byte_t* data, uint64_t data_len, uint64_t uncompressed_len)
{
	auto buf = data;
	if (data_len != uncompressed_len) {
		uLong uncompressed_len_own = uncompressed_len;
		auto decompressed_buf = new byte_t[uncompressed_len_own];
		auto ret = uncompress(decompressed_buf, &uncompressed_len_own, data, data_len);
		assert(ret == Z_OK);
		buf = decompressed_buf;
	}

	f(buf, uncompressed_len);

	if constexpr (not takes_ownership) {
		if (data_len != uncompressed_len) {
			delete[] buf;
		}
	}
}
};

template <std::invocable<const struct FstBlockByBlock&> F>
void FstReader::block_by_block(F&& f) const
{
	for (auto& block : metadata->vcblocks) {
		auto times = block.read_time_table(file_mmap());
		f(FstBlockByBlock{times, block, *this});
	}
}

template <std::invocable<uint32_t, const byte_t *, uint16_t> F>
void FstReader::read_values(uint32_t facid, F && f) const {
    block_by_block([&](auto const & block) {
      block.read_values(facid, f);
    });
}

template <std::invocable<uint32_t, const byte_t*, uint16_t> F>
void FstBlockByBlock::read_values(uint32_t facid, F&& f) const
{
	auto bits = reader.metadata->nbits[facid];

	// this is all a bit involved. Offset gives the offset to the start of the data for this
	// block The data is
	// 1. a varint with the uncompressed length
	// 2. the actual data
	// however length stores the length including the varint, so we need to subtract the length
	// of the varint from the stored length to get the length of the zlib compressed data
	auto offset = block.wave_data_offset[facid];
	const byte_t* data_offset = reader.file_mmap() + offset;

	auto uncompressed_len = impl::read_varint(data_offset);
	auto read = data_offset - reader.file_mmap() - offset;
	auto compressed_len = block.wave_data_compressed_length[facid] - read;

	// if zero, its not actually compressed
	uncompressed_len = uncompressed_len > 0 ? uncompressed_len : compressed_len;

	impl::with_maybe_uncompress(
	    [&](const byte_t* data, auto n) {
		    if (bits == 1) {
			    read_block_single_bit(time_table, data, n, f);
		    } else {
			    read_block_multi_bit(time_table, data, n, (bits + 7) / 8, f);
		    }
	    },
	    data_offset, compressed_len, uncompressed_len);
};

template <typename T>
std::pair<std::vector<uint32_t>, std::vector<T>> FstBlockByBlock::read_values(uint32_t facid) const
{
	assert(sizeof(T) >= (uint32_t) (reader.metadata->nbits[facid] + 7) / 8);
	auto max_changes = block.end_time - block.start_time + 1; // this range is inclusive
	std::vector<uint32_t> times(max_changes);
	std::vector<T> vals(max_changes);
	uint32_t idx = 0;
	read_values(facid, [&](uint32_t time, const byte_t* data, uint16_t bytes) {
		T value{0};
		for (int i = 0; i < bytes; i++) {
			value <<= 8;
			value += (*data++);
		}
		times[idx] = time;
		vals[idx] = value;

		idx += 1;
	});

	times.resize(idx);
	vals.resize(idx);

	return {times, vals};
}

template <class F>
void read_block_single_bit(std::vector<uint32_t> time_table, const byte_t* data, size_t n, F&& f)
{
	auto end = data + n;
	auto time_idx = 0;
	while (data < end) {
		auto combined_value = impl::read_varint(data) >> 1;
		auto time_idx_delta = combined_value >> 1;
		byte_t value = combined_value & 0b1;
		time_idx += time_idx_delta;

		f(time_table[time_idx], &value, 1);
	}
}

template <class F>
void read_block_multi_bit(
    std::vector<uint32_t> time_table, const byte_t* data, size_t n, size_t bytes, F&& f)
{
	auto end = data + n;
	auto time_idx = 0;
	while (data < end) {
		auto time_idx_delta = impl::read_varint(data) >> 1;
		time_idx += time_idx_delta;

		// uint64_t value = 0;
		// for (int i = 0; i < bytes; i++) {
		// 	value << 8;
		// 	value += (*data++);
		// }

		f(time_table[time_idx], data, bytes);
		data += bytes;
	}
}
