#pragma once

#include "imgui.h"
#include "node.h"
#include "waveform_viewer.h"
#include <vector>

struct NodesPanel {
    float                              zoom = 1.0;
    ImVec2                             pos;
    int                                grid_size = 10;
    std::vector<std::shared_ptr<Node>> nodes;
    WaveformViewer *                   viewer;

    NodesPanel(std::vector<std::shared_ptr<Node>> nodes, WaveformViewer * viewer);
    void render(uint64_t current_time, const ImVec2 & offset, const ImVec2 & size, const std::function<void(std::shared_ptr<Node>)> & process_func);
};
