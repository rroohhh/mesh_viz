#include "signal_flow_data.h"

#include "fst_file.h"
#include "node.h"
#include "format_util.h"

#include <ranges>
#include <execution>
#include <algorithm>
#include <unordered_map>

constexpr const char* SAMPLE_MARKER = "signal_flow_sample";
constexpr const char* SAMPLE_CLK = "clk";
constexpr const char* SAMPLE_INTERFACE = "interface";
constexpr const char* SAMPLE_MODULE = "module";
constexpr const char* SAMPLE_DIRECTION = "direction";

FlowDirection parse_flow_direction(std::string s)
{
	if (s == "in") {
		return FlowDirection::In;
	} else if (s == "out") {
		return FlowDirection::Out;
	} else {
		assert(false);
	}
}

FlowMetadata::FlowMetadata(std::shared_ptr<Node> node, NodeVar& var) :
    module_path{json::parse(std::get<std::string>(var.attrs[SAMPLE_MODULE]))
                    .template get<std::vector<std::string>>()},
    interface_path{json::parse(std::get<std::string>(var.attrs[SAMPLE_INTERFACE]))
                       .template get<std::vector<std::string>>()},
    direction{parse_flow_direction(std::get<std::string>(var.attrs[SAMPLE_DIRECTION]))},
    node{node}
{
}

FlowData::FlowData(decltype(id_to_time) id_to_time, FlowMetadata metadata) :
    id_to_time{id_to_time}, metadata{metadata}
{
}

// \direction: out
// \interface: ["output", 1, "payload"]
// \module: ["west", "input_mq_reader"]

SignalFlowData::SignalFlowData(
    std::shared_ptr<FstFile> fst_file, std::vector<std::shared_ptr<Node>> nodes) :
    data_future{std::async(std::launch::async, [=] {
	    FstFile local_fstfile(*fst_file);
	    std::vector<FlowData> flow_data;

	    for (auto node : nodes) {
		    auto clk = node->data.variables.at(SAMPLE_CLK);
		    for (auto [name, var] : node->data.variables) {
			    if (var.attrs.contains(SAMPLE_MARKER)) {
				    // std::println("reading flow data: {}", var.pretty_name());
				    auto strobe = node->data.variables.at(name + "_strobe");
				    // sample negedge, because that is where design inputs from the simulation are
				    // changed
				    auto [times, data] = local_fstfile.read_values<uint32_t>(
				        var, clk, {strobe}, {}, true /* negedge */);

				    auto zipped = std::views::zip(data, times);
				    auto map =
				        std::unordered_multimap<id_t, simtime_t>{zipped.begin(), zipped.end()};

				    flow_data.push_back(FlowData{map, {node, var}});
			    }
		    }
	    }
	    return flow_data;
    })}
{
}

struct FlowSample
{
	simtime_t time;
	FlowDirection direction;
	std::vector<std::string> interface_path;

    // first sort after time, for equal time sort after direction
    // the direction enum is set up to sort in before out
    auto operator <=>(FlowSample const & other) const {
      return std::tie(time, direction) <=> std::tie(other.time, other.direction);
    }
};

template <>
struct std::formatter<FlowSample, char>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	auto format(const auto& s, auto& ctx) const
	{
		return std::format_to(
		    ctx.out(), "FlowSample{{time={}, direction={}, interface_path={}}}", s.time, s.direction, s.interface_path);
	}
};

std::optional<FlowZones> SignalFlowData::query(id_t t)
{
	using namespace std::literals::chrono_literals;
	if (data_future.valid() and data_future.wait_for(0ms) == std::future_status::ready) {
		data = data_future.get();
	}
	if (data) {
		std::println("handling query for {}", t);

		// node -> module_path -> FlowSample[]
		std::map<std::shared_ptr<Node>, std::map<std::vector<std::string>, std::vector<FlowSample>>>
		    samples;

		// first: collect all flow samples
		for (auto& flow : *data) {
			auto [times_begin, times_end] = flow.id_to_time.equal_range(t);
			auto times = std::ranges::subrange(times_begin, times_end) | std::views::values |
			             std::ranges::to<std::vector>();
			for (auto& time : times) {
				samples[flow.metadata.node][flow.metadata.module_path].push_back(FlowSample{
				    .time = time,
				    .direction = flow.metadata.direction,
				    .interface_path = flow.metadata.interface_path,
				});
			}
		}

		std::map<std::shared_ptr<Node>, std::vector<FlowZone>> zones;

		// then pair up in and out samples per module per node to produce zones
		for (auto& [node, samples] : samples) {
            for (auto & [module_path, samples] : samples) {
              std::sort(std::execution::unseq, samples.begin(), samples.end());
              std::optional<FlowSample> last_in;
              for (auto & sample: samples) {
                // TODO(robin): could lint case where there is no out for a In
                if (sample.direction == FlowDirection::In) {
                  last_in = sample;
                }
                if (sample.direction == FlowDirection::Out) {
                  if (last_in) {
                    zones[node].emplace_back(last_in->time, sample.time, module_path, last_in->interface_path, sample.interface_path);
                  } else {
                    std::println("WARNING: no `In` sample for `Out` sample {}, module {}, ignoring", sample, module_path);
                  }
                }
                // std::println("module {}: {}", module_path, sample);
              }
            }
            std::sort(std::execution::unseq, zones[node].begin(), zones[node].end());
		}

		return FlowZones{.zones = zones};
	}

	return std::nullopt;
}

bool FlowZone::is_delta() {
  return start == end;
}
