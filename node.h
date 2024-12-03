#pragma once

#include "formatter.h"
#include "imgui.h"
#include <memory>
#include <string>

#include "mesh_utils.h"

struct Node;
struct NodeVar;
struct WaveformViewer;
struct Histograms;
struct FstFile;
struct Highlights;

#include "fst_file.h"

// node vars should know which Node they belong to, because we want to support multiple fstfiles per
// trace (ie up to one per node to avoid inter thread sync)
struct NodeVar
{
	std::string name;
	uint64_t nbits;
	std::shared_ptr<Node> owner_node;
	std::shared_ptr<Formatter> formatter;
	std::map<std::string, std::variant<int64_t, uint64_t, double, std::string>> attrs;

	NodeVar(
	    std::string name,
	    uint64_t nbits,
	    handle_t handle,
	    std::shared_ptr<Node> owner_node,
	    std::shared_ptr<Formatter> formatter,
		decltype(NodeVar::attrs) attrs);

	char* value_at_time(clock_t time) const;

	bool is_vector() const;

	std::span<char> format(char* value) const;

	std::string pretty_name() const;

private:
	using handle_t = ::handle_t;
	handle_t handle;
	friend Node;
	friend FstFile;
	friend WaveformViewer;
	friend Highlights;
};

template <>
struct std::formatter<NodeVar, char>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	auto format(const auto& s, auto& ctx) const
	{
		return std::format_to(ctx.out(), "NodeVar{{name={}, nbits={}}}", s.name, s.nbits);
	}
};

struct NodeData
{
	std::string name;
	std::string compname;

	std::map<std::string, NodeData> subscopes;
	std::map<std::string, NodeVar> variables;
};

const int NODE_SIZE = 100;
struct Node : public std::enable_shared_from_this<Node>
{
	int x, y;
	NodeData data;
	uint64_t current_time;

	bool highlight = false;
	NodeRoleAttr role;
	SystemAttr<ParamsWrap<TraceFPGABandwidthParams>, ParamsWrap<PoissonEventTrafficParams>> system_config;

	Node(int x, int y, NodeData data, FstFile* ctx, NodeRoleAttr role, decltype(system_config) system_config);

	void render(
	    uint64_t c_time,
	    const ImVec2& offset,
	    const float& zoom,
	    std::function<void(std::shared_ptr<Node>)> process_func,
	    WaveformViewer* v, Histograms* hist);

	char* get_current_var_value(const NodeVar& var);

	char* value_at_time(const NodeVar& var, simtime_t time);

	void add_var_to_viewer(const NodeVar& var);

	// TODO(robin): too lazy to make this span, pybind11 doesnt have it by default
	void add_hist(const NodeVar& var, const NodeVar& sampling_var, const std::vector<NodeVar> & conditions = {}, const std::vector<NodeVar> & masks = {}, bool negedge = false);

	// TODO(robin): encapsulate this
	FstFile* ctx;
private:
	WaveformViewer* viewer;
	Histograms* histograms;

	void render_data(const NodeData& data, float width) const;
};
