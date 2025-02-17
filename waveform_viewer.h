#pragma once

#include "core.h"
#include "node_var.h"
#include "wave_data_base.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <span>
#include <string>
#include <vector>

struct FstFile;
struct Highlights;

struct Timeline
{
	std::shared_ptr<FstFile> file;
	uint32_t first_time, last_time;

	Timeline(std::shared_ptr<FstFile> file);

	auto render(double zoom, double offset, uint64_t cursor_value, ImRect bb);
};

const auto MIN_TEXT_SIZE = 20;
const auto PADDING = 3;

// returns the end of the text
auto clip_text_to_width(std::span<char> text, float pixels);

const auto FEATHER_SIZE = 4.0f;
// const auto FEATHER_SIZE = 0.0f;
const auto MOUSE_WHEEL_DRAG_FACTOR = 10.0f;

struct WaveformViewer
{
private:
	std::shared_ptr<FstFile> file;
	Highlights * highlights;
	Timeline timeline;
	double zoom = 1.0;
	double offset_f = 0.0;
	uint64_t cursor_value = 0;
	float window_zoom_start = 0, window_zoom_end = 0;
	bool did_window_zoom = false;

	float timeline_height = 100, waveforms_height = 100;
	float label_width = 100, waveform_width = 100;
	bool playing = false;

	mutable std::mutex mutex;

	std::unordered_map<NodeID, WaveDatabase> fac_dbs;

public:
	WaveformViewer(std::shared_ptr<FstFile> file, Highlights * highlights);

	uint64_t render();

	void add(const NodeVar& var, std::span<std::string> group_hier = {});

private:
	std::vector<NodeVar> vars;

	// just dummy vectors, maybe quicker than allocing for every waveform
	std::vector<ImVec2> lines_a;
	std::vector<ImVec2> lines_b;
	std::vector<ImVec2> highlights_to_draw;
	std::vector<uint32_t> highlight_colors;
	// time and pos and space
	std::vector<std::tuple<simtime_t, float, float>> text_to_draw;

	void draw_waveform(int64_t first_time, int64_t last_time, const NodeVar& var);
};
