#pragma once

#include "core.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdint>
#include <mutex>
#include <set>
#include <span>
#include "fst_file.h"


struct Timeline {
    FstFile * file;
    bool      playing = false;
    uint64_t  first_time, last_time;

    Timeline(FstFile * file);

    auto render(float zoom, int64_t offset, uint64_t cursor_value, ImRect bb);
};

const auto MIN_TEXT_SIZE = 20;
const auto PADDING       = 3;

// returns the end of the text
auto clip_text_to_width(std::span<char> text, float pixels);

const auto FEATHER_SIZE = 4.0f;
// const auto FEATHER_SIZE = 0.0f;
const auto MOUSE_WHEEL_DRAG_FACTOR = 10.0f;

bool all_zero(const char * string);

struct WaveformViewer {
private:
    FstFile * file;
    Timeline  timeline;
    float     zoom         = 1.0;
    float     offset_f     = 0.0;
    uint64_t  cursor_value = 0;

    float timeline_height = 100, waveforms_height = 100;
    float label_width = 100, waveform_width = 100;

    mutable std::mutex mutex;
    handle_t                                            maxHandle = 0;
    std::set<fstHandle>                                 facs;
    std::vector<std::vector<std::pair<ImVec2, size_t>>> bases;
    std::vector<std::tuple<uint64_t, int64_t, int64_t>> old_values;

    std::vector<std::vector<ImVec2>> lines_a;
    std::vector<std::vector<ImVec2>> lines_b;

    // fst handle, time, size
    std::vector<std::tuple<fstHandle, int64_t, float>> text_to_draw;

public:
    WaveformViewer(FstFile * file);

    uint64_t render();

    void add(const NodeVar & var);

private:
    std::vector<NodeVar> vars;
};
