#pragma once

#include <vector>
#include <format>
#include <memory>
#include <utility>
//
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

// #include <zlib.h>


using byte_t = uint8_t;

namespace bip = boost::interprocess;

enum class FstBlockType : uint8_t
{
	Header,
	VCData,
	Blackout,
	Geometry,
	Hierarchy,
	VCDataDynAlias,
	HierarchyLZ4,
	HierarchyLZ4Duo,
	VCDataDynAlias2,
	ZWrapper,
	Skip,
};

template <>
struct std::formatter<FstBlockType, char>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	auto format(const auto& s, auto& ctx) const
	{
		switch (s) {
			case FstBlockType::Header:
				return std::format_to(ctx.out(), "Header");
			case FstBlockType::VCData:
				return std::format_to(ctx.out(), "VCData");
			case FstBlockType::Blackout:
				return std::format_to(ctx.out(), "Blackout");
			case FstBlockType::Geometry:
				return std::format_to(ctx.out(), "Geometry");
			case FstBlockType::Hierarchy:
				return std::format_to(ctx.out(), "Hierarchy");
			case FstBlockType::VCDataDynAlias:
				return std::format_to(ctx.out(), "VCDataDynAlias");
			case FstBlockType::HierarchyLZ4:
				return std::format_to(ctx.out(), "HierarchyLZ4");
			case FstBlockType::HierarchyLZ4Duo:
				return std::format_to(ctx.out(), "HierarchyLZ4Duo");
			case FstBlockType::VCDataDynAlias2:
				return std::format_to(ctx.out(), "VCDataDynAlias2");
			case FstBlockType::ZWrapper:
				return std::format_to(ctx.out(), "ZWrapper");
			case FstBlockType::Skip:
				return std::format_to(ctx.out(), "Skip");
			default:
				return std::format_to(ctx.out(), "unknown");
		}
		std::unreachable();
	}
};

class FstBlock
{
public:
	FstBlockType ty;
	uint64_t len;

	uint64_t data_start()
	{
		return offset;
	}
	uint64_t data_end()
	{
		return offset + len - 8;
	}

	FstBlock(FstBlockType ty, uint64_t len, uint64_t offset) : ty(ty), len(len), offset(offset) {}

private:
	// this is the offset to the data -> points after length. Note, the actual length of the data is
	// len - 8, because it counts the len field;
	uint64_t offset;
	friend struct std::formatter<FstBlock, char>;
};

template <>
struct std::formatter<FstBlock, char>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	auto format(const auto& s, auto& ctx) const
	{
		return std::format_to(
		    ctx.out(), "FstBlock{{ty={}, len={}, offset={}}}", s.ty, s.len, s.offset);
	}
};

template <typename T>
struct std::formatter<std::vector<T>, char>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	auto format(const auto& s, auto& ctx) const
	{
		std::format_to(ctx.out(), "[");
		bool first = true;
		for (auto elem : s) {
			if (first) {
				std::format_to(ctx.out(), "{}", elem);

			} else {
				std::format_to(ctx.out(), ", {}", elem);
			}
			first = false;
		}
		return std::format_to(ctx.out(), "]");
	}
};

struct FstHeader
{
	uint64_t start_time;
	uint64_t end_time;
	uint64_t num_vars;
};

using GeometryT = std::vector<uint16_t>;
using GeometrySumT = std::vector<uint32_t>;


struct FstVCBlockInfo
{
	std::vector<int32_t> wave_data_offset;
	std::vector<uint32_t> wave_data_compressed_length;
	// std::vector<uint32_t> wave_data_uncompressed_length;

	uint64_t start_time;
	uint64_t end_time;

	uint64_t bits_uncompressed_length;
	uint64_t bits_compressed_length;
	uint64_t bits_count;

	uint64_t time_uncompressed_length;
	uint64_t time_compressed_length;
	uint64_t time_count;
	uint64_t time_data_pos;
	uint64_t wave_data_pos;


	std::vector<uint32_t> read_time_table(const byte_t* data) const;
};

struct FstMetaData
{
	uint64_t start_time;
	uint64_t end_time;
	uint64_t num_ids;

	// 65565 bits should be enough for anybody tm
	GeometryT nbits;
	GeometrySumT nbits_prefix_sum;

	std::vector<FstVCBlockInfo> vcblocks;
};

namespace impl {
	FstMetaData init_metadata(const char * path);
}

// NOTE(robin): this assumes you will always write the files on a little endian system and therefore
// will only ever read the floats as little endian
class FstReader
{
	std::string path;
	// std::shared_ptr<bip::file_mapping> mapping;
	std::shared_ptr<bip::mapped_region> mapped_file;

	std::shared_ptr<FstMetaData> metadata;

public:
	FstReader(const char* path) :
	    path(path),
	    mapped_file(std::make_shared<bip::mapped_region>(bip::file_mapping(path, bip::read_only), bip::read_only)),
		metadata(std::make_shared<FstMetaData>(impl::init_metadata(path)))
	{
	}

	// TODO(robin): parallel version?
	template <std::invocable<const struct FstBlockByBlock &> F>
	void block_by_block(F&& f) const;

	template <std::invocable<uint32_t, const byte_t *, uint16_t> F>
	void read_values(uint32_t facid, F && f) const;

private:
	const byte_t* file_mmap() const;


  friend struct FstBlockByBlock;
};

class FstBlockByBlock
{
	std::vector<uint32_t> time_table;
	FstVCBlockInfo& block;
FstReader& reader;

  friend struct FstReader;
  FstBlockByBlock(std::vector<uint32_t> time_table, FstVCBlockInfo & block, FstReader & reader) : time_table(time_table), block(block), reader(reader) {}

public:
	template <typename T>
	std::pair<std::vector<uint32_t>, std::vector<T>> read_values(uint32_t facid) const;

	template <std::invocable<uint32_t, const byte_t *, uint16_t> F>
	void read_values(uint32_t facid, F && f) const;
};


namespace impl {
uint64_t read_varint(const byte_t*& data);
}

// struct FstDynAlias2 {
//   // bits at start_time
//   std::shared_ptr<const byte_t *> bits;
//   uint64_t start_time;
//   uint64_t end_time;
// };


// stuff I want
// (block by block) read_values<T> (T < uint64_t)
// (block by block) read_non_zero
// read_non_zero

#include "fst_reader.tcc"
