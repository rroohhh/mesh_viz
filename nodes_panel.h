#pragma once

#include "imgui.h"
#include "node.h"
#include <vector>

struct WaveformViewer;
struct Histograms;
struct AsyncRunner;

struct NodesPanel
{
	float zoom = 1.0;
	ImVec2 pos;
	int grid_size = 10;
	std::vector<std::shared_ptr<Node>> nodes;

	NodesPanel(std::vector<std::shared_ptr<Node>> nodes);
	void render(
	    uint64_t current_time,
	    const ImVec2& offset,
	    const ImVec2& size,
	    const std::function<void(std::shared_ptr<Node>)>& process_func);
};
