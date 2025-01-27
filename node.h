#pragma once

#include "imgui.h"
#include "core.h"

#include <string>

#include "mesh_utils.h"

namespace pybind11 {
	class object;
}

struct Node;
struct NodeVar;
struct WaveformViewer;
struct AsyncRunner;
struct FstFile;
struct Highlights;
struct Histograms;

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
	SystemAttr<ParamsWrap<TraceFPGABandwidthParams>, ParamsWrap<PoissonEventTrafficParams>, ParamsWrap<FixedErrorModelParams>> system_config;

	Node(int x, int y, NodeData data, std::shared_ptr<FstFile> ctx, NodeRoleAttr role, decltype(system_config) system_config, WaveformViewer* v, Histograms* hist, AsyncRunner * async_runner);

	void render(
	    uint64_t c_time,
	    const ImVec2& offset,
	    const float& zoom,
	    std::function<void(std::shared_ptr<Node>)> process_func);

	char* get_current_var_value(const NodeVar& var);

	char* value_at_time(const NodeVar& var, simtime_t time);

	void add_var_to_viewer(const NodeVar& var);

	template<class ...Args>
	void add_hist(Args && ...args);

	// TODO(robin): can I make this external to the class somehow?
	void enqueue_task(std::function<pybind11::object(pybind11::object)>);

	// TODO(robin): encapsulate this
	std::shared_ptr<FstFile> ctx;
private:
	WaveformViewer* viewer;
	Histograms* histograms;
	AsyncRunner* async_runner;

	void render_data(const NodeData& data, float width) const;
};
