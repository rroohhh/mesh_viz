#include "wave_data_base.h"
#include <chrono>
#include <fstream>
#include <map>
#include <print>
#include <ranges>

template <class T>
auto bench(const std::vector<WaveValue>& values, bool jumpy)
{
	T db(values);

	auto start = std::chrono::high_resolution_clock::now();
	int N = 10;

	uint32_t checksum = 0;
	uint32_t query_count = 0;
	// warmup
	for (int i = 0; i <= N; i++) {
		const auto& [check, ops] = impl::work(db, jumpy);
		checksum += check;
		query_count += ops;
	}
	std::chrono::duration<double, std::nano> duration;
	while (true) {
		for (int i = 0; i <= N; i++) {
			const auto& [check, ops] = impl::work(db, jumpy);
			checksum += check;
			query_count += ops;
		}
		duration = std::chrono::high_resolution_clock::now() - start;
		if (duration.count() > 1e9) {
			break;
		} else {
			N *= 2;
		}
	}

	std::println(
	    "check: {}, mem: {} MB, dur: {}ms, ops: {}, per query: {}ns", checksum,
	    db.memory_usage() / 1024.0f / 1024.0f, duration.count() / 1e6, query_count,
	    duration.count() / query_count);
}

int main()
{
	std::ifstream i("../wdb_perf.csv");
	std::map<uint32_t, std::vector<WaveValue>> values;
	for (std::string line; std::getline(i, line);) {
		auto parts_range =
		    line | std::views::split(std::string_view(", ")) |
		    std::views::transform([](auto sv) { return std::string{std::string_view{sv}}; });
		std::vector<std::string> parts{
		    std::ranges::begin(parts_range), std::ranges::end(parts_range)};
		uint32_t timestamp = std::stol(parts[0]);
		uint32_t fac = std::stol(parts[1]);
		uint32_t value = std::stol(parts[2]);
		auto [vals, _] = values.try_emplace(fac);
		vals->second.emplace_back(timestamp, (WaveValueType) value);
	}


	for (bool jumpy : {true, false}) {
		std::println("jumpy {}", jumpy);
		for (const auto& [fac, vals] : values) {
			std::println("fac: {}", fac);

			std::println("uncompressed binary search");
			bench<impl::UncompressedWaveDatabase<true>>(vals, jumpy);
			std::println("uncompressed linear scan");
			bench<impl::UncompressedWaveDatabase<false>>(vals, jumpy);
			std::println("elias fano");
			bench<impl::EliasFanoWaveDatabase>(vals, jumpy);
			std::println("auto tune");
			bench<WaveDatabase>(vals, jumpy);
		}
	}
}
