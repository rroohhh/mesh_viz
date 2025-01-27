#pragma once

#include <print>
#include <cinttypes>
#include <format>
#include <vector>
#include <folly/compression/elias_fano/EliasFanoCoding.h>

// this provides a database to do fast change lookup. It is not possible to get the actual shit
enum class WaveValueType
{
	Zero,
	NonZero,
	VALUE_TYPE_NUM_VALUES
};

// O(1 billion) timestamps should be enough for anybody tm

struct WaveValue
{
	uint32_t timestamp;
	WaveValueType type;

	static constexpr auto ValueTypeBits =
	    std::bit_width((uint32_t) WaveValueType::VALUE_TYPE_NUM_VALUES - 1);

	auto operator<=>(const WaveValue & other) const = default;

	uint32_t pack() const;

	static WaveValue unpack(uint32_t v);
};

template <>
struct std::formatter<WaveValueType, char>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	auto format(const auto& s, auto& ctx) const
	{
		return std::format_to(ctx.out(), "Value{{timestamp={}, type={}}}", s.timestamp, s.type);
	}
};

template <>
struct std::formatter<WaveValue, char>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	auto format(const auto& s, auto& ctx) const
	{
		return std::format_to(
		    ctx.out(), "Value{{timestamp={}, type={}}}", s.timestamp, (int) s.type);
	}
};

template <>
struct std::formatter<std::optional<WaveValue>, char>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	auto format(const auto& s, auto& ctx) const
	{
		if (s) {
			return std::format_to(
				ctx.out(), "Some({})", *s);
		} else {
			return std::format_to(
				ctx.out(), "None");
		}
	}
};

namespace impl {
template <bool BINARY_SEARCH = 0>
struct UncompressedWaveDatabase
{
	std::vector<uint32_t> values;
	size_t internal_idx;

	UncompressedWaveDatabase(std::span<const WaveValue> values);

	WaveValue get(size_t idx);

	uint32_t memory_usage();

	// finds next value geq from current position
	std::optional<WaveValue> skip_to(WaveValue to_find);

	std::optional<WaveValue> jump_to(WaveValue to_find);

	std::optional<WaveValue> previous_value();

	std::optional<WaveValue> value();

	void rewind();

	WaveValue last();

	uint32_t size();
};

struct EliasFanoWaveDatabase
{
	// TODO(robin): with skip pointers it seems buggy
	// (-> with 16, 0, previous of idx 1 was invalid)
	using EncoderT = folly::compression::EliasFanoEncoder<uint32_t, uint32_t, 0, 0, false>;
	using ReaderT = folly::compression::
	    EliasFanoReader<EncoderT, folly::compression::instructions::Default, true, uint32_t>;

	// Value, SkipValue, forward quantum, skip quantum
	std::optional<EncoderT::MutableCompressedList> data;
	ReaderT reader;
	uint32_t max;
	size_t bytes_size;

	~EliasFanoWaveDatabase() {
		if (data) {
			data->free();
		}
	}

	EliasFanoWaveDatabase(std::span<const WaveValue> values);
	EliasFanoWaveDatabase(EliasFanoWaveDatabase&& other);
	EliasFanoWaveDatabase & operator=(EliasFanoWaveDatabase && other);

	EliasFanoWaveDatabase(const EliasFanoWaveDatabase& other) = delete;
	EliasFanoWaveDatabase & operator=(EliasFanoWaveDatabase & other) = delete;

	WaveValue get(size_t idx);

	uint32_t memory_usage();

	// finds next value geq from current position
	std::optional<WaveValue> skip_to(WaveValue to_find);

	std::optional<WaveValue> jump_to(WaveValue to_find);

	std::optional<WaveValue> previous_value();

	std::optional<WaveValue> value();

	void rewind();

	WaveValue last();

	uint32_t size() const;

private:
	// static EncoderT::CompressedList init_data(std::span<const WaveValue> values);
};

// polymorphism was slower :(
template <class... DBS>
struct BenchmarkingDatabase
{
	std::variant<DBS...> the_db;

	BenchmarkingDatabase(std::span<const WaveValue> values, bool jumpy = false);

	BenchmarkingDatabase(const BenchmarkingDatabase &) = delete;
	BenchmarkingDatabase & operator=(const BenchmarkingDatabase &) = delete;
	BenchmarkingDatabase(BenchmarkingDatabase &&) = default;
	BenchmarkingDatabase & operator=(BenchmarkingDatabase &&) = default;

	WaveValue get(size_t idx);

	uint32_t memory_usage();

	// finds next value geq from current position
	std::optional<WaveValue> skip_to(WaveValue to_find);

	std::optional<WaveValue> jump_to(WaveValue to_find);

	std::optional<WaveValue> previous_value();

	std::optional<WaveValue> value();

	void rewind();

	WaveValue last();

	uint32_t size();

private:
	static std::variant<DBS...> find_best_db(std::span<const WaveValue> values, bool jumpy);
};

template <class DB>
std::pair<uint32_t, uint32_t> work(DB& db, bool);
}


// TODO(robin): partitioned elias fano should probably be even better
// (good for locally clustered data <-> bursty activity)
// Such as the open-source http://github.com/facebook/ folly, http://github.com/simongog/sdsl,
// http:// github.com/ot/succinct, and http://sux.di.unimi.it.
using WaveDatabase = impl::BenchmarkingDatabase<
    impl::UncompressedWaveDatabase<true>,
    // impl::UncompressedWaveDatabase<false>,
    impl::EliasFanoWaveDatabase>;
