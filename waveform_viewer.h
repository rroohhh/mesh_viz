#pragma once

#include "core.h"
#include "cursor.h"
#include "node_var.h"
#include "wave_data_base.h"
#include "timeline.h"
#include "imgui.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <span>
#include <string>
#include <vector>

struct FstFile;
struct Highlights;

const auto MIN_TEXT_SIZE = 20;
const auto PADDING = 3;

const auto FEATHER_SIZE = 4.0f;
// const auto FEATHER_SIZE = 0.0f;

struct WaveformViewer
{
private:
	std::shared_ptr<FstFile> file;
	Highlights * highlights;
	Timeline timeline;
	double zoom = 1.0;
	double offset_f = 0.0;
	std::shared_ptr<Cursor> cursor;

	// float window_zoom_start = 0, window_zoom_end = 0;
	// bool did_window_zoom = false;

	float timeline_height = 40, waveforms_height = 100;
	float label_width = 100, waveform_width = 100;
	bool playing = false;

	mutable std::mutex mutex;

	std::unordered_map<NodeID, WaveDatabase> fac_dbs;

public:
	WaveformViewer(std::shared_ptr<FstFile> file, Highlights * highlights, std::shared_ptr<Cursor>);

	void render();

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
