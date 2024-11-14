#pragma once

#include <print>
#include <algorithm>
#include <chrono>
#include <execution>
#include <bit>
#include <vector>
#include <ranges>
#include <folly/compression/elias_fano/EliasFanoCoding.h>

// this provides a database to do fast change lookup. It is not possible to get the actual shit
enum class ValueType {
  Zero,
  NonZero,
  VALUE_TYPE_NUM_VALUES
};

// O(1 billion) timestamps should be enough for anybody tm

struct Value {
  uint32_t timestamp;
  ValueType type;

  static constexpr auto ValueTypeBits = std::bit_width((uint32_t) ValueType::VALUE_TYPE_NUM_VALUES);

  uint32_t pack() const {
    return (timestamp << ValueTypeBits) | (uint32_t) type;
  }

  static Value unpack(uint32_t v) {
    return { v >> ValueTypeBits, (ValueType) (v & ((1 << ValueTypeBits) - 1)) };
  }
};

template<>
struct std::formatter<ValueType, char> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

    auto format(const auto & s, auto & ctx) const {
        return std::format_to(ctx.out(), "Value{{timestamp={}, type={}}}", s.timestamp, s.type);
    }
};

template<>
struct std::formatter<Value, char> {
    constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

    auto format(const auto & s, auto & ctx) const {
        return std::format_to(ctx.out(), "Value{{timestamp={}, type={}}}", s.timestamp, (int) s.type);
    }
};

namespace impl {
template<bool BINARY_SEARCH = 0>
struct UncompressedWaveDatabase {
  std::vector<uint32_t> values;
  size_t internal_idx;

  template<std::ranges::input_range IRange>
  UncompressedWaveDatabase(IRange values) : values(values | std::views::transform(&Value::pack) | std::ranges::to<std::vector>()), internal_idx(0) {}

  Value get(size_t idx) {
    return Value::unpack(values[idx]);
  }

  uint32_t memory_usage() {
    return values.size() * sizeof(values[0]);
  }

  // finds next value geq from current position
  std::optional<Value> skip_to(Value to_find) {
    uint32_t fixed = to_find.timestamp << Value::ValueTypeBits;

    auto current = get(internal_idx);
    auto diff = to_find.timestamp - current.timestamp + 1;
    // we can assume strictly increasing values (all glitches should be filtered out in input processing otherwise)
    // so this is the maximum distance the value we are searching for can be from our current position
    auto alternate_end = std::min(values.begin() + internal_idx + diff, values.end());

    typename decltype(values)::iterator iter;
    if constexpr(BINARY_SEARCH) {
      iter = std::lower_bound(values.begin() + internal_idx, alternate_end, fixed);
    } else {
      iter = std::find_if(std::execution::unseq, values.begin() + internal_idx, alternate_end, [=](uint32_t v){ return v >= fixed; });
    }

    if (iter == values.end()) {
      return std::nullopt;
    } else {
        internal_idx = std::distance(values.begin(), iter);
        return {Value::unpack(values[internal_idx])};
    }

    return std::nullopt;
  }

  std::optional<Value> jump_to(Value to_find) {
    internal_idx = 0;
    return skip_to(to_find);
  }

  std::optional<Value> previous_value() {
    if (internal_idx > 0) {
        return {Value::unpack(values[internal_idx - 1])};
    }
    return std::nullopt;
  }

  void rewind() {
    internal_idx = 0;
  }
};

struct EliasFanoWaveDatabase {
  // Value, SkipValue, forward quantum, skip quantum
  using EncoderT = folly::compression::EliasFanoEncoder<uint32_t, uint32_t, 16, 0>;
  using ReaderT = folly::compression::EliasFanoReader<EncoderT, folly::compression::instructions::Default, true>;
  EncoderT::CompressedList data;
  ReaderT reader;
  uint32_t max;
  size_t size;

  EliasFanoWaveDatabase(const std::vector<Value> & values) :
    data(init_data(values)),
    reader(data),
    max(values.back().pack()),
    size(EncoderT::Layout::fromUpperBoundAndSize(values.size(), max).bytes()) {}

  Value get(size_t idx) {
    reader.jump(idx);
    return Value::unpack(reader.value());
  }

  uint32_t memory_usage() {
    return size;
  }

  // finds next value geq from current position
  std::optional<Value> skip_to(Value to_find) {
    uint32_t encoded = to_find.timestamp << Value::ValueTypeBits;
    // skip to seems unsafe for too big values
    if (encoded > max) {
      return std::nullopt;
    }
    // TODO(robin): is this not always true?
    if(reader.skipTo(encoded)) {
      return {Value::unpack(reader.value())};
    }
    return std::nullopt;
  }

  std::optional<Value> jump_to(Value to_find) {
    uint32_t encoded = to_find.timestamp << Value::ValueTypeBits;
    if (encoded > max) {
      return std::nullopt;
    }
    // TODO(robin): is this not always true?
    if(reader.jumpTo(encoded, true /* assumeDistinct */)) {
      return {Value::unpack(reader.value())};
    }
    return std::nullopt;
  }

  std::optional<Value> previous_value() {
    if (reader.position() != 0) {
      return {Value::unpack(reader.previousValue())};
    }
    return std::nullopt;
  }

  void rewind() {
    reader.reset();
  }
private:
  static EncoderT::CompressedList init_data(const std::vector<Value> & values) {
    EncoderT encoder(values.size(), values.back().pack());
    for (const auto & v : values) {
      encoder.add(v.pack());
    }
    return encoder.finish();
  }
};



std::pair<uint32_t, uint32_t> work(auto & db) {
  db.rewind();
  // 10M points, 2000 pixels -> about 2000 queries per pixel
  // TODO(robin): sweep this
  uint32_t step_per_pixel = 1000;
  uint32_t time = 0;
  uint32_t sum = 0;
  uint32_t ops = 0;
  while (true) {
    auto val = db.skip_to(Value{time, (ValueType) 0});
    ops++;
    auto previous = db.previous_value();
    if(previous) {
      sum += (uint32_t) previous->type;
    }
    if (val) {
      // std::println("time: {}, val: {}, prev: {}", time, *val, *previous);
        time = (val->timestamp + step_per_pixel) / step_per_pixel * step_per_pixel;
        // time += step_per_pixel;
        sum += (uint32_t) val->type;
    } else {
      break;
    }
  }
  return {sum, ops};
}
}

// polymorphism was slower :(
template<class... DBS>
struct BenchmarkingDatabase {
  std::variant<DBS...> the_db;

  BenchmarkingDatabase(const std::vector<Value> & values) : the_db(find_best_db(values)) {}


  auto get(auto idx) {
    return std::visit([&](auto & db) { return db.get(idx); }, the_db);
  }

  auto memory_usage() {
    return std::visit([&](auto & db) { return db.memory_usage(); }, the_db);
  }

  // finds next value geq from current position
  auto skip_to(auto to_find) {
    return std::visit([&](auto & db) { return db.skip_to(to_find); }, the_db);
  }

  auto jump_to(auto to_find) {
    return std::visit([&](auto & db) { return db.jump_to(to_find); }, the_db);
  }

  auto previous_value() {
    return std::visit([&](auto & db) { return db.previous_value(); }, the_db);
  }

  void rewind() {
    std::visit([&](auto & db) { return db.rewind(); }, the_db);
  }

private:
  static std::variant<DBS...> find_best_db(const std::vector<Value> & values) {
    std::tuple<DBS...> dbs{DBS{values}...};

    double best_time;
    int best = -1;
    std::optional<std::variant<DBS...>> ret;
    ([&]<std::size_t... Is>(std::index_sequence<Is...>){
      ([&]{
        auto db = std::get<Is>(dbs);
        auto start = std::chrono::high_resolution_clock::now();
        const auto N = 100;

        uint32_t checksum = 0;
        for (int i = 0; i <= N; i++) {
          const auto & [check, ops] = impl::work(db);
          checksum += check;
        }
        std::chrono::duration<double, std::nano> duration = std::chrono::high_resolution_clock::now() - start;

        double memory_usage_baseline = sizeof(Value{}.pack()) * values.size();
        double memory_factor = ((double) db.memory_usage() / memory_usage_baseline);
        if (memory_usage_baseline < 1e6) {
          // ignore memory usage if the usage is small
          memory_factor = 1.0f;
        }

        double score = memory_factor * duration.count();
        if (best == -1) {
          best = Is;
          best_time = score;
        } else if(score < best_time) {
          best = Is;
          best_time = score;
        }
      }(), ...);
      ((best == Is && (void(ret.emplace(std::move(std::get<Is>(dbs)))), 1)) || ...);
    } (std::make_index_sequence<sizeof...(DBS)>{}));

    return *ret;
  }
};

using WaveDatabase = BenchmarkingDatabase<impl::UncompressedWaveDatabase<true>, impl::UncompressedWaveDatabase<false>, impl::EliasFanoWaveDatabase>;
