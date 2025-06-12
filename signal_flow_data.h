#pragma once

#include "core.h"
#include <format>
#include <print>
#include <cstdint>
#include <future>
#include <unordered_map>
#include <utility>
#include <vector>
#include <map>

struct FstFile;
struct Node;
struct NodeVar;

// NOTE: the specific values of these are actually relevant,
// for the zone generation algorithm, we want In to sort before Out
enum class FlowDirection
{
	In = 0,
	Out = 1
};

template <>
struct std::formatter<FlowDirection, char>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	auto format(const auto& s, auto& ctx) const
	{
        switch (s) {
        case FlowDirection::In: {
		return std::format_to(
		    ctx.out(), "In");

        }

        case FlowDirection::Out: {
		return std::format_to(
		    ctx.out(), "Out");

        }
        default: {
        std::unreachable();
        }
        }
	}
};

struct FlowMetadata
{
	std::vector<std::string> module_path;
	std::vector<std::string> interface_path;
	FlowDirection direction;

	std::shared_ptr<Node> node;

	FlowMetadata(std::shared_ptr<Node> node, NodeVar& var);
};


template <>
struct std::formatter<FlowMetadata, char>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	auto format(const auto& s, auto& ctx) const
	{
		return std::format_to(
		    ctx.out(), "FlowMetadata{{module_path={}, interface_path={}, direction={}, node=[{},{}]}}", s.module_path, s.interface_path, s.direction, s.node->x, s.node->y);
	}
};

struct FlowData
{
	using id_t = uint32_t;

	std::unordered_multimap<id_t, simtime_t> id_to_time;

	FlowMetadata metadata;

	FlowData(decltype(id_to_time) id_to_time, FlowMetadata metadata);
};

struct FlowZone {
  simtime_t start;
  simtime_t end;
  std::vector<std::string> module_path;

  std::vector<std::string> interface_in;
  std::vector<std::string> interface_out;

  // sort after start and then longer zones before shorter zones
  // (this is the draw order and we want to draw longer ones before shorter ones)
  auto operator<=>(const FlowZone & other) const {
      return std::make_tuple(start, -end) <=> std::make_tuple(other.start, -other.end);
  }

  bool is_delta();
};

template <>
struct std::formatter<FlowZone, char>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	auto format(const auto& s, auto& ctx) const
	{
		return std::format_to(
		    ctx.out(), "FlowZone{{start={}, end={}, module={}, interface_in={}, interface_out={}}}", s.start, s.end, s.module_path, s.interface_in, s.interface_out);
	}
};

struct FlowZones {
  std::map<std::shared_ptr<Node>, std::vector<FlowZone>> zones;
};

class SignalFlowData
{
	std::future<std::vector<FlowData>> data_future;
	std::optional<std::vector<FlowData>> data;

public:
	SignalFlowData(std::shared_ptr<FstFile> fst_file, std::vector<std::shared_ptr<Node>> nodes);
    // returns nullopt iff the flow data is not loaded yet
	std::optional<FlowZones> query(id_t t);
};
