#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "cursor.h"
#include "imgui.h"

struct WaveformViewer;
struct Histograms;
struct AsyncRunner;
struct Node;

struct NodesPanel
{
	float zoom = 1.0;
	ImVec2 pos;
	int grid_size = 10;
	std::vector<std::shared_ptr<Node>> nodes;
	std::shared_ptr<Cursor> cursor;

	NodesPanel(std::vector<std::shared_ptr<Node>> nodes, std::shared_ptr<Cursor>);
	void render(
	    const ImVec2& offset,
	    const ImVec2& size,
	    const std::function<void(std::shared_ptr<Node>)>& process_func);
};
