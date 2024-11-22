#include "wave_data_base.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <execution>
#include <print>

WaveValue WaveValue::unpack(uint32_t v) {
    return {v >> ValueTypeBits, (WaveValueType)(v & ((1 << ValueTypeBits) - 1))};
}
uint32_t WaveValue::pack() const { return (timestamp << ValueTypeBits) | (uint32_t)type; }

namespace impl {
    template <bool BINARY_SEARCH>
    UncompressedWaveDatabase<BINARY_SEARCH>::UncompressedWaveDatabase(std::span<const WaveValue> values)
        : values(values | std::views::transform(&WaveValue::pack) | std::ranges::to<std::vector>()), internal_idx(0) {}

    template <bool BINARY_SEARCH>
    WaveValue UncompressedWaveDatabase<BINARY_SEARCH>::get(size_t idx) {
        return WaveValue::unpack(values[idx]);
    }

    template <bool BINARY_SEARCH>
    uint32_t UncompressedWaveDatabase<BINARY_SEARCH>::memory_usage() {
        return values.size() * sizeof(values[0]);
    }

    // finds next value geq from current position
    template <bool BINARY_SEARCH>
    std::optional<WaveValue> UncompressedWaveDatabase<BINARY_SEARCH>::skip_to(WaveValue to_find) {
        uint32_t fixed = to_find.timestamp << WaveValue::ValueTypeBits;

        auto current = get(internal_idx);
        auto diff    = to_find.timestamp - current.timestamp + 1;
        // we can assume strictly increasing values (all glitches should be filtered out in input processing
        // otherwise) so this is the maximum distance the value we are searching for can be from our current
        // position
        auto alternate_end = std::min(values.begin() + internal_idx + diff, values.end());

        typename decltype(values)::iterator iter;
        if constexpr(BINARY_SEARCH) {
            iter = std::lower_bound(values.begin() + internal_idx, alternate_end, fixed);
        } else {
            iter = std::find_if(std::execution::unseq, values.begin() + internal_idx, alternate_end,
                                [=](uint32_t v) { return v >= fixed; });
        }

        if(iter == values.end()) {
            return std::nullopt;
        } else {
            internal_idx = std::distance(values.begin(), iter);
            return {WaveValue::unpack(values[internal_idx])};
        }

        return std::nullopt;
    }

    template <bool BINARY_SEARCH>
    std::optional<WaveValue> UncompressedWaveDatabase<BINARY_SEARCH>::jump_to(WaveValue to_find) {
        internal_idx = 0;
        return skip_to(to_find);
    }

    template <bool BINARY_SEARCH>
    std::optional<WaveValue> UncompressedWaveDatabase<BINARY_SEARCH>::previous_value() {
        if(internal_idx > 0) { return {WaveValue::unpack(values[internal_idx - 1])}; }
        return std::nullopt;
    }

    template <bool BINARY_SEARCH>
    void UncompressedWaveDatabase<BINARY_SEARCH>::rewind() {
        internal_idx = 0;
    }

    template <bool BINARY_SEARCH>
    WaveValue UncompressedWaveDatabase<BINARY_SEARCH>::last() {
        return WaveValue::unpack(values.back());
    }

  // template struct UncompressedWaveDatabase<false>;
  // template struct UncompressedWaveDatabase<true>;

    auto EliasFanoWaveDatabase::init_data(std::span<const WaveValue> values) -> EncoderT::CompressedList {
        EncoderT encoder(values.size(), values.back().pack());
        for(const auto & v : values) { encoder.add(v.pack()); }
        return encoder.finish();
    }

    void                     EliasFanoWaveDatabase::rewind() { reader.reset(); }

    WaveValue                EliasFanoWaveDatabase::last() { return WaveValue::unpack(max); }

    std::optional<WaveValue> EliasFanoWaveDatabase::previous_value() {
        if(reader.position() != 0) { return {WaveValue::unpack(reader.previousValue())}; }
        return std::nullopt;
    }

    std::optional<WaveValue> EliasFanoWaveDatabase::jump_to(WaveValue to_find) {
        uint32_t encoded = to_find.timestamp << WaveValue::ValueTypeBits;
        // if (to_find.timestamp == 1) {
        //     std::println("max {}, jump_to {}, position {}", max, encoded, reader.position());
        // }
        if(encoded > max) {
            reader.jumpTo(max);
            return std::nullopt;
        }
        // TODO(robin): is this not always true?
        if(reader.jumpTo(encoded, true /* assumeDistinct */)) { return {WaveValue::unpack(reader.value())}; }
        return std::nullopt;
    }

    std::optional<WaveValue> EliasFanoWaveDatabase::skip_to(WaveValue to_find) {
        uint32_t encoded = to_find.timestamp << WaveValue::ValueTypeBits;
        // std::println("max {}, skip_to {}, position {}", max, encoded, reader.position());
        // skip to seems unsafe for too big values
        if(encoded > max) {
            reader.skipTo(max);
            return std::nullopt;
        }
        // TODO(robin): is this not always true?
        if(reader.skipTo(encoded)) { return {WaveValue::unpack(reader.value())}; }
        return std::nullopt;
    }

    uint32_t  EliasFanoWaveDatabase::memory_usage() { return size; }

    WaveValue EliasFanoWaveDatabase::get(size_t idx) {
        reader.jump(idx);
        return WaveValue::unpack(reader.value());
    }

    EliasFanoWaveDatabase::EliasFanoWaveDatabase(std::span<const WaveValue> values)
        : data(init_data(values)), reader(data), max(values.back().pack()),
          size(EncoderT::Layout::fromUpperBoundAndSize(values.size(), max).bytes()) {

        // for (auto & v : values) {
        //     std::println("{}", v);
        // }
    }

  // template struct EliasFanoWaveDatabase<impl::EncoderT, impl::ReaderT>;

    template <class... DBS>
    BenchmarkingDatabase<DBS...>::BenchmarkingDatabase(std::span<const WaveValue> values)
        : the_db(find_best_db(values)) {}

    template <class... DBS>
    WaveValue BenchmarkingDatabase<DBS...>::get(size_t idx) {
        return std::visit([&](auto & db) { return db.get(idx); }, the_db);
    }

    template <class... DBS>
    uint32_t BenchmarkingDatabase<DBS...>::memory_usage() {
        return std::visit([&](auto & db) { return db.memory_usage(); }, the_db);
    }

    template <class... DBS>
    std::optional<WaveValue> BenchmarkingDatabase<DBS...>::skip_to(WaveValue to_find) {
        return std::visit([&](auto & db) { return db.skip_to(to_find); }, the_db);
    }

    template <class... DBS>
    std::optional<WaveValue> BenchmarkingDatabase<DBS...>::jump_to(WaveValue to_find) {
        return std::visit([&](auto & db) { return db.jump_to(to_find); }, the_db);
    }

    template <class... DBS>
    std::optional<WaveValue> BenchmarkingDatabase<DBS...>::previous_value() {
        return std::visit([&](auto & db) { return db.previous_value(); }, the_db);
    }

    template <class... DBS>
    void BenchmarkingDatabase<DBS...>::rewind() {
        std::visit([&](auto & db) { return db.rewind(); }, the_db);
    }

    template <class... DBS>
    WaveValue BenchmarkingDatabase<DBS...>::last() {
        return std::visit([&](auto & db) { return db.last(); }, the_db);
    }

    template <class... DBS>
    std::variant<DBS...> BenchmarkingDatabase<DBS...>::find_best_db(std::span<const WaveValue> values) {
        std::tuple<DBS...> dbs{DBS{values}...};

        double                              best_time;
        int                                 best = -1;
        std::optional<std::variant<DBS...>> ret;
        ([&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (
                [&] {
                    auto       db    = std::get<Is>(dbs);
                    auto       start = std::chrono::high_resolution_clock::now();
                    const auto N     = 100;

                    uint32_t checksum = 0;
                    for(int i = 0; i <= N; i++) {
                        const auto & [check, ops] = work(db);
                        checksum += check;
                    }
                    std::chrono::duration<double, std::nano> duration =
                        std::chrono::high_resolution_clock::now() - start;

                    double memory_usage_baseline = sizeof(WaveValue{}.pack()) * values.size();
                    double memory_factor         = ((double)db.memory_usage() / memory_usage_baseline);
                    if(memory_usage_baseline < 1e6) {
                        // ignore memory usage if the usage is small
                        memory_factor = 1.0f;
                    }

                    double score = memory_factor * duration.count();
                    if(best == -1) {
                        best      = Is;
                        best_time = score;
                    } else if(score < best_time) {
                        best      = Is;
                        best_time = score;
                    }
                }(),
                ...);
            std::println("found best db: {}", best);
            ((best == Is && (void(ret.emplace(std::move(std::get<Is>(dbs)))), 1)) || ...);
        }(std::make_index_sequence<sizeof...(DBS)>{}));

        return *ret;
    }

template<class DB>
std::pair<uint32_t, uint32_t> work(DB & db) {
    db.rewind();
    // 10M points, 2000 pixels -> about 2000 queries per pixel
    // TODO(robin): sweep this
    uint32_t step_per_pixel = 1000;
    uint32_t time           = 0;
    uint32_t sum            = 0;
    uint32_t ops            = 0;
    while(true) {
        auto val = db.skip_to(WaveValue{time, (WaveValueType)0});
        ops++;
        auto previous = db.previous_value();
        if(previous) { sum += (uint32_t)previous->type; }
        if(val) {
            // std::println("time: {}, val: {}, prev: {}", time, *val, *previous);
            time = (val->timestamp + step_per_pixel) / step_per_pixel * step_per_pixel;
            // time += step_per_pixel;
            sum += (uint32_t)val->type;
        } else {
            break;
        }
    }
    return {sum, ops};
}

template struct BenchmarkingDatabase<UncompressedWaveDatabase<true>, UncompressedWaveDatabase<false>, EliasFanoWaveDatabase>;

template std::pair<uint32_t, uint32_t> work<>(WaveDatabase & db);
template std::pair<uint32_t, uint32_t> work<>(UncompressedWaveDatabase<false> & db);
template std::pair<uint32_t, uint32_t> work<>(UncompressedWaveDatabase<true> & db);
template std::pair<uint32_t, uint32_t> work<>(EliasFanoWaveDatabase & db);

template struct impl::UncompressedWaveDatabase<true>;
template struct impl::UncompressedWaveDatabase<false>;
}
