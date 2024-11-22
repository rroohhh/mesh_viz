#pragma once

#include "core.h"
#include "fst_file.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdint>
#include <mutex>
#include <set>
#include <span>


struct Timeline
{
	FstFile* file;
	bool playing = false;
	uint64_t first_time, last_time;

	Timeline(FstFile* file);

	auto render(float zoom, float offset, uint64_t cursor_value, ImRect bb);
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
	FstFile* file;
	Timeline timeline;
	float zoom = 1.0;
	float offset_f = 0.0;
	uint64_t cursor_value = 0;

	float timeline_height = 100, waveforms_height = 100;
	float label_width = 100, waveform_width = 100;

	mutable std::mutex mutex;

	handle_t maxHandle = 0;
	std::map<handle_t, WaveDatabase> fac_dbs;

public:
	WaveformViewer(FstFile* file);

	uint64_t render();

	void add(const NodeVar& var);

private:
	std::vector<NodeVar> vars;
};
