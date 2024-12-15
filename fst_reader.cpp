#include "fst_reader.h"

#include <cassert>
#include <fstream>
#include <numeric>
#include <print>

using namespace impl;

namespace impl {
uint64_t read_varint(const byte_t*& data)
{
	uint64_t result = 0;

	int i = 0;
	do {
		result |= ((uint64_t) ((*data) & 0b0111'1111)) << (7 * i++);
	} while (*(data++) & 0b1000'0000);
	return result;
}


struct FstMetadataReader
{
	std::ifstream file;

	FstMetaData metadata;

	FstMetadataReader(const char* path) : file(path) {};

	FstVCBlockInfo read_dyn_alias2(FstBlock block)
	{
		file.seekg(block.data_start());
		auto start_time = read_scalar<uint64_t>();
		auto end_time = read_scalar<uint64_t>();
		[[maybe_unused]]
		auto memory_required = read_scalar<uint64_t>();
		auto bits_uncompressed_length = read_varint();
		auto bits_compressed_length = read_varint();
		auto bits_count = read_varint();

		// std::println("start_time {}, end_time {}, memory_required {}, bits_uncompressed_length
		// {}, bits_compressed_length {}, bits_count {}", start_time, end_time, memory_required,
		// bits_uncompressed_length, bits_compressed_length, bits_count);

		file.seekg(bits_compressed_length, file.cur);

		[[maybe_unused]]
		auto waves_count = read_varint();
		auto wave_data_pos = file.tellg();
		auto waves_packtype = read_scalar<uint8_t>();
		// only zlib for now
		assert(waves_packtype == 'Z');
		// std::println("waves_count {}, packtype {}", waves_count, waves_packtype);


		file.seekg(block.data_end() - 24);
		auto time_uncompressed_length = read_scalar<uint64_t>();
		auto time_compressed_length = read_scalar<uint64_t>();
		auto time_count = read_scalar<uint64_t>();

		auto time_data_pos = block.data_end() - 24 - time_compressed_length;


		// std::println("time_uncompressed_length {}, time_compressed_length {}, time_count {}",
		// time_uncompressed_length, time_compressed_length, time_count);

		// std::println("time_data {}", time_deltas);

		file.seekg(time_data_pos - 8);

		auto position_length = read_scalar<uint64_t>();

		// std::println("position_length {}", position_length);

		auto position_data_pos =
		    block.data_end() - 24 - time_compressed_length - 8 - position_length;

		auto wave_data_len = position_data_pos - wave_data_pos;
		// std::println("wave data len: {}", wave_data_len);
		file.seekg(position_data_pos);

		std::vector<int32_t> positions(metadata.num_ids);
		std::vector<uint32_t> lengths(metadata.num_ids);

		auto var_idx = 0;
		auto bytes_offset = 0;
		auto previous_alias = 0;
		auto previous_actual_var = 0;

		do {
			if (file.peek() & 0b1) {
				auto v = read_svarint();
				// std::println("read sint {}", v);
				int64_t decoded = v >> 1;
				if (decoded == 0) {
					// std::println("var_idx {} is alias of {}", var_idx, -(previous_alias + 1));
					positions[var_idx] = previous_alias;
				} else if (decoded < 0) {
					previous_alias = decoded;
					positions[var_idx] = previous_alias;
					// std::println("var_idx {} is alias of {}", var_idx, -(previous_alias + 1));
				} else {
					bytes_offset += decoded;
					positions[var_idx] = bytes_offset + wave_data_pos;
					if (bytes_offset != decoded) {
						lengths[previous_actual_var] = decoded;
					}
					previous_actual_var = var_idx;
					// std::println("var_idx {} has data at offset {}", var_idx, bytes_offset);
				}
				var_idx += 1;
			} else {
				auto v = read_varint();
				// std::println("read varint {}", v);
				auto run_length = v >> 1;
				// std::println("zero run of len {}, var_idx {}", run_length, var_idx);
				var_idx += run_length;
			}
		} while (static_cast<uint64_t>(file.tellg()) < time_data_pos - 8);

		if (bytes_offset > 0) {
			lengths[previous_actual_var] = wave_data_len - bytes_offset;
		}

		for (uint64_t i = 0; i < metadata.num_ids; i++) {
			auto pos = positions[i];
			if (pos < 0) {
				positions[i] = positions[-(pos + 1)];
				lengths[i] = lengths[-(pos + 1)];
			}
		}

		for (uint64_t i = 0; i < metadata.num_ids; i++) {
			if (positions[i] > 0) {
				assert(lengths[i] > 0);
			}
		}

		// std::println("lengths {}", lengths);
		// std::println("positions {}", positions);
		// auto position_length = read_scalar<uint64_t>();
		FstVCBlockInfo ret = {
		    .wave_data_offset = positions,
		    .wave_data_compressed_length = lengths,

		    .start_time = start_time,
		    .end_time = end_time,

		    .bits_uncompressed_length = bits_uncompressed_length,
		    .bits_compressed_length = bits_compressed_length,
		    .bits_count = bits_count,

		    .time_uncompressed_length = time_uncompressed_length,
		    .time_compressed_length = time_compressed_length,
		    .time_count = time_count,
		    .time_data_pos = time_data_pos,
		    .wave_data_pos = static_cast<uint64_t>(wave_data_pos),
		};
		return ret;
	}

	GeometryT read_geometry(FstBlock block)
	{
		file.seekg(block.data_start());
		auto uncompressed_length = read_scalar<uint64_t>();
		auto count = read_scalar<uint64_t>();

		auto compressed_length = block.len - 24;
		auto buf = new char[compressed_length];
		file.read(buf, compressed_length);
		GeometryT geometry(count);
		with_maybe_uncompress(
		    [&](const byte_t* data, uint64_t) {
			    for (uint64_t i = 0; i < count; i++) {
				    geometry[i] = ::read_varint(data);
			    }
		    },
		    (const byte_t *) buf, compressed_length, uncompressed_length);

		delete[] buf;

		return geometry;
	}

	int64_t read_svarint()
	{
		int64_t result = 0;
		unsigned char data;

		int i = 0;
		do {
			file.read((char*) &data, 1);
			result |= ((int64_t) (data & 0b0111'1111)) << (7 * i++);
		} while (data & 0b1000'0000);
		if ((7 * i < 64) and (data & 0b0100'0000)) {
			result |= -(1LL << (7 * i));
		}
		return result;
	}

	uint64_t read_varint()
	{
		uint64_t result = 0;
		unsigned char data;

		int i = 0;
		do {
			file.read((char*) &data, 1);
			result |= ((uint64_t) (data & 0b0111'1111)) << (7 * i++);
		} while (data & 0b1000'0000);
		return result;
	}

	FstHeader read_header(FstBlock block)
	{
		FstHeader header;
		file.seekg(block.data_start());
		header.start_time = read_scalar<uint64_t>();
		header.end_time = read_scalar<uint64_t>();
		[[maybe_unused]]
		auto real_endianess = read_scalar<double>();
		// std::println("endianess: {}", real_endianess);
		[[maybe_unused]]
		auto writer_memory_use = read_scalar<uint64_t>();
		[[maybe_unused]]
		auto num_scopes = read_scalar<uint64_t>();
		[[maybe_unused]]
		auto num_hierarchy_vars = read_scalar<uint64_t>();
		// std::println("hierarchy vars: {}", num_hierarchy_vars);
		header.num_vars = read_scalar<uint64_t>();
		// std::println("vars: {}", header.num_vars);
		[[maybe_unused]]
		auto num_vc_blocs = read_scalar<uint64_t>();
		[[maybe_unused]]
		auto timescale = read_scalar<int8_t>();
		char writer[129];
		file.read(writer, 128);
		writer[128] = '\0';
		// std::println("writer: `{}`", writer);
		char date[26];
		file.read(date, sizeof(date));
		char dummy[3];
		file.read(dummy, sizeof(date));
		[[maybe_unused]]
		auto filetype = read_scalar<uint8_t>();
		// std::println("filetype {}", filetype);
		[[maybe_unused]]
		auto timezero = read_scalar<int64_t>();
		// std::println("timezero {}", timezero);
		return header;
	}

	void read_blocks()
	{
		file.seekg(0, file.beg);
		while (true) {
			auto block = read_block();
			// std::println("found block {}", block);

			if (block.ty == FstBlockType::Header) {
				auto header = read_header(block);
				metadata.start_time = header.start_time;
				metadata.end_time = header.end_time;
				metadata.num_ids = header.num_vars;
			}

			if (block.ty == FstBlockType::Geometry) {
				metadata.nbits = read_geometry(block);
				metadata.nbits_prefix_sum = GeometrySumT(metadata.nbits.size());
				std::partial_sum(
				    metadata.nbits.begin(), metadata.nbits.end(),
				    metadata.nbits_prefix_sum.begin());
				// std::println("metadata.nbits {}", metadata.nbits);
			}

			if (block.ty == FstBlockType::VCDataDynAlias2) {
				metadata.vcblocks.push_back(read_dyn_alias2(block));
			}

			file.seekg(block.data_end());

			if (file.peek() == EOF)
				break;
		}
	}

	FstBlock read_block()
	{
		FstBlockType type = static_cast<FstBlockType>(read_scalar<uint8_t>());
		uint64_t len = read_scalar<uint64_t>();
		return {type, len, static_cast<uint64_t>(file.tellg())};
	}

	template <typename T>
	T read_scalar()
	{
		T value{0};
		file.read(reinterpret_cast<char*>(&value), sizeof(T));
		if (file.gcount() != sizeof(T)) {
			assert(false);
		}
		if constexpr (std::is_same<T, float>() or std::is_same<T, double>()) {
			return value;
		} else {
			return std::byteswap(value);
		}
	}
};

FstMetaData init_metadata(const char* path)
{
	FstMetadataReader reader(path);
	return reader.metadata;
}

}

std::vector<uint32_t> FstVCBlockInfo::read_time_table(const byte_t* data) const
{
	std::vector<uint32_t> time(time_count);
	with_maybe_uncompress(
	    [&](const byte_t* data, auto) {
		    auto last = 0;
		    for (uint64_t i = 0; i < time_count; i++) {
			    auto v = read_varint(data);
			    last += v;
			    time[i] = last;
		    }
	    },
	    data + time_data_pos, time_compressed_length, time_uncompressed_length);
	return time;
}

const byte_t* FstReader::file_mmap() const
{
	return static_cast<const byte_t*>(mapped_file->get_address());
}
