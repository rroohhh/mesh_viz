#include "waveform_viewer.h"
#include "utils.cpp"
#include <print>

void DrawCenterText(auto & draw, const char * text, const ImVec2 & pos) {
    auto sz = ImGui::CalcTextSize(text);
    draw->AddText(pos - ImVec2(sz.x / 2, 0), 0xffffffff, text);
}

void DrawVLine(auto & draw, const ImVec2 & min, const ImVec2 & sz, float x, int col, float thickness = 1.0f) {
    draw->AddLine(min + ImVec2(x, 0), min + ImVec2(x, sz.y), col, thickness);
}

auto clip_text_to_width(std::span<char> text, float pixels) {
    const char * indicator       = "+";
    static float indicator_width = 0;
    if(indicator_width == 0) { indicator_width = ImGui::CalcTextSize(indicator).x; }

    auto length = text.size();
    if(ImGui::CalcTextSize(text.data()).x < pixels) {
        return text.end();
    } else {
        // std::println("string = `{}`, size = {}, actual = {}", text, pixels, ImGui::CalcTextSize(text).x);
        pixels -= indicator_width;
        auto end = std::partition_point(text.begin(), text.end(),
                                        [=](char & end) { return ImGui::CalcTextSize(text.data(), &end).x < pixels; });
        // std::println("clipped_size = {}", end - text);
        // this should always be valid, as:
        // 1. we always clip atleast one char (otherwise the full thing fits and the early check succeeds)
        // 2. we can overwrite the 'NUL' char
        end[0] = indicator[0];
        // end[1] = indicator[1];
        return end + 1;
    }
}
auto Timeline::render(float zoom, int64_t offset, uint64_t cursor_value, ImRect bb) {
    int64_t min_time = file->min_time();
    int64_t max_time = file->max_time();

    // if(ImGui::SmallButton(playing ? ICON_FA_PAUSE "###playing" : ICON_FA_PLAY "###playing")) {
    //     playing = not playing;
    // }
    // ImGui::SameLine();

    auto sz          = bb.GetSize();
    auto line_height = ImGui::GetTextLineHeight();
    sz.y -= line_height;

    auto width    = sz.x;
    auto text_min = bb.Min;
    auto min      = text_min + ImVec2{0, line_height};

    if(playing) { cursor_value += 1; }

    first_time                   = clip(min_time - offset, min_time, max_time);
    uint64_t last_time_unclipped = first_time + width / zoom;
    last_time                    = clip(last_time_unclipped, min_time, max_time);
    bool draw_last               = last_time_unclipped > max_time;

    auto draw = ImGui::GetWindowDrawList();

    // Draw time grid
    // want a marker every 10 pixels, so calculate how much every 10 pixels is
    float   b           = 10;
    float   fine_width  = b / zoom;
    float   human_base  = 10;
    int64_t log_step    = ceil(log(fine_width) / log(human_base));
    int64_t fine_step   = powf(human_base, log_step);
    int64_t coarse_step = powf(human_base, log_step + 1);

    if(fine_step > 0) {
        int64_t time_value = ((first_time + fine_step - 1) / fine_step) * fine_step;
        while(time_value < last_time) {
            DrawVLine(draw, min, ImVec2(sz.x, 10), (time_value + offset) * zoom, TIMELINE_TICK_COL);
            time_value += fine_step;
        }
    }

    if(coarse_step > 0) {
        int64_t time_value = ((first_time + coarse_step - 1) / coarse_step) * coarse_step;
        while(time_value < last_time or draw_last) {
            if(time_value >= last_time) {
                time_value = last_time;
                draw_last  = false;
            }
            DrawCenterText(draw, std::format("{}", time_value).c_str(),
                           text_min + ImVec2{(time_value + offset) * zoom, 0});
            DrawVLine(draw, min, sz, (time_value + offset) * zoom, TIMELINE_TICK_COL, 3.0f);
            time_value += coarse_step;
        }
    }

    // Draw cursor
    auto c_pos = (cursor_value + offset) * zoom;
    DrawVLine(draw, min, sz, c_pos, CURSOR_COL, 2.0f);

    return std::tuple{first_time, last_time};
}
Timeline::Timeline(FstFile * file) : file(file) {}
WaveformViewer::WaveformViewer(FstFile * file) : file(file), timeline(file) {}
void WaveformViewer::add(const NodeVar & var) {
    auto guard = std::lock_guard(mutex);
    vars.push_back(var);
    facs.insert(var.handle);
    lines_a.resize(vars.size());
    lines_b.resize(vars.size());
    maxHandle = max(maxHandle, var.handle);
    // index vs length -> need to add one
    bases.resize(maxHandle + 1);
    old_values.resize(maxHandle + 1);
}
uint64_t WaveformViewer::render() {
    auto guard = std::lock_guard(mutex);

    int64_t min_time = file->min_time();
    int64_t max_time = file->max_time();

    ImGui::Begin("WaveformViewer");
    auto sz    = ImGui::GetContentRegionAvail();
    sz.x       = max(sz.x, 1);
    sz.y       = max(sz.y, 1);
    auto width = sz.x;

    auto min = ImGui::GetCursorScreenPos();
    // input handling
    {
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("canvas", sz,
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                                   ImGuiButtonFlags_MouseButtonMiddle);
        const auto is_hovered = ImGui::IsItemHovered();
        const auto is_active  = ImGui::IsItemActive();
        ImGuiIO &  io         = ImGui::GetIO();
        if(is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0)) { offset_f += io.MouseDelta.x / zoom; }
        if(is_hovered and ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
            float old_zoom = zoom;
            // TODO(robin): do this log style
            if(io.MouseWheel > 0) {
                zoom /= std::powf(1.1, io.MouseWheel);
            } else if(io.MouseWheel < 0) {
                zoom /= std::pow(0.9, std::fabs(io.MouseWheel));
            }
            offset_f -= (io.MousePos.x - min.x - label_width) * (1.0 / old_zoom - 1.0 / zoom);
        }
        if(is_hovered) { offset_f += MOUSE_WHEEL_DRAG_FACTOR * io.MouseWheelH / zoom; }
        if(is_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right)) {
            offset_f = 0;
            zoom     = 0.98 * width / (max_time - min_time);
        }
        if(is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
            cursor_value = clip((io.MousePos.x - min.x - label_width) / zoom - offset_f, min_time, max_time);
            // playing = false;
        }

        ImGui::SetCursorScreenPos(min);
    }

    // layout splitters
    {
        // TODO(robin): fix timeline_height being greater than sz.y
        waveforms_height = sz.y - timeline_height;
        ImRect timeline_splitter_bb(min + ImVec2(0, -1.0f + timeline_height),
                                    min + ImVec2(sz.x, 1.0f + timeline_height));
        ImGui::SplitterBehavior(timeline_splitter_bb, ImGui::GetID("timeline##Splitter"), ImGuiAxis_Y, &timeline_height,
                                &waveforms_height, 5, 5);

        waveform_width = sz.x - label_width;
        ImRect waveform_splitter_bb(min + ImVec2(-1.0f + label_width, timeline_height),
                                    min + ImVec2(1.0f + label_width, sz.y));
        ImGui::SplitterBehavior(waveform_splitter_bb, ImGui::GetID("label##Splitter"), ImGuiAxis_X, &label_width,
                                &waveform_width, 5, 5);
    }

    ImRect timeline_bb(min + ImVec2(label_width, 0), min + ImVec2(sz.x, timeline_height));
    const auto & [first_time, last_time] = timeline.render(zoom, offset_f, cursor_value, timeline_bb);

    // waveform labels
    {
        min = ImGui::GetCursorScreenPos() + ImVec2(0, timeline_height);
        ImGui::SetCursorScreenPos(min);

        // clear without resize
        bases.assign(bases.size(), {});
        int i = 0;

        ImGui::Separator();
        for(auto & var : vars) {
            auto base = ImGui::GetCursorScreenPos();
            bases[var.handle].emplace_back(ImVec2(min.x + label_width, base.y + 1), i);
            char * val       = var.value_at_time(cursor_value);
            auto   formatted = var.format(val);
            // float size = label_width - ImGui::CalcTextSize(var.name.c_str());
            ImGui::Text("%s: ", var.name.c_str());
            ImGui::SameLine();
            ImGui::TextUnformatted(&*formatted.begin(),
                                   &*clip_text_to_width(formatted, label_width - ImGui::GetCursorPos().x));
            ImGui::Separator();
            i++;

            old_values[var.handle] = {0, -1, -1};
        }
    }

    // Waveform lines
    for(auto & line : lines_a) { line.clear(); }
    for(auto & line : lines_b) { line.clear(); }

    // TODO(robin): make this float
    int64_t offset = offset_f;
    auto    draw   = ImGui::GetWindowDrawList();
    auto    y_size = ImGui::GetTextLineHeight() - 2;

    text_to_draw.clear();

    // gtkwave does the following two opts:
    // 1. only render value text if enough space is there (thats probably why surfer is so slow)
    // 2. only render one transition per screen pixel
    // 2. we can also compress the changes and preload them maybe? (one uint32_t or so per change?)
    file->read_changes(first_time, last_time, vars, [&](uint64_t time, handle_t fac, const unsigned char * value) {
        std::println("{}, {}, {}", time, fac, (int) all_zero((char *) value));

        const auto & [old_value, old_time, last_draw_time] = old_values[fac];
        if(old_time < 0) {
            auto new_value  = not all_zero((const char *)value);
            old_values[fac] = {new_value, time, 0};

            return;
        } else {
            // std::println("old_time {}, time {}", old_value, old_time);

            if(old_time > last_time) return;

            auto draw_time = last_draw_time;
            auto new_value = not all_zero((const char *)value);
            if((not((old_time < first_time and time < first_time) or (old_time > last_time and time > last_time))) and
               old_time >= 0) {
                // skip changes for same pixel
                auto pixel_delta     = (time - last_draw_time) * zoom;
                auto old_screen_time = (old_time + offset) * zoom;
                auto new_screen_time = (time + offset) * zoom;
                auto is_vector       = false;
                if(pixel_delta >= 1.0f / DPI_SCALE) {
                    draw_time = time;
                    for(const auto & [base, var_idx] : bases.at(fac)) {
                        auto & var = vars[var_idx];
                        is_vector  = var.is_vector();
                        if(not is_vector) {
                            // we decided to draw the first transition, so add the initial point
                            if(lines_a[var_idx].size() == 0) {
                                lines_a[var_idx].push_back(
                                    base + ImVec2((first_time + offset) * zoom, y_size * (1.0 - old_value)));
                            } else {
                                // last_draw_time is only valid once we have draw atleast one point...
                                if(old_time != last_draw_time) {
                                    lines_a[var_idx].push_back(
                                        base + ImVec2((last_draw_time + offset) * zoom, y_size * (1.0 - old_value)));
                                }
                            }

                            lines_a[var_idx].push_back(base + ImVec2(new_screen_time, y_size * (1.0 - old_value)));
                            lines_a[var_idx].push_back(base + ImVec2(new_screen_time, y_size * (1.0 - new_value)));
                        } else {
                            if(lines_a[var_idx].size() == 0) {
                                lines_a[var_idx].push_back(base + ImVec2((first_time + offset) * zoom, y_size));
                                lines_b[var_idx].push_back(
                                    base + ImVec2((first_time + offset) * zoom, y_size * (1 - old_value)));
                            } else {
                                if(old_time != last_draw_time) {
                                    // TODO(robin): this is a bit ugly when we have a zero transition, but im fine with
                                    // it for now too lazy to think how to fix this
                                    lines_a[var_idx].push_back(
                                        base + ImVec2((last_draw_time + offset) * zoom - FEATHER_SIZE / 2, y_size));
                                    lines_b[var_idx].push_back(
                                        base + ImVec2((last_draw_time + offset) * zoom - FEATHER_SIZE / 2,
                                                      y_size * (1 - old_value)));
                                }
                            }

                            lines_a[var_idx].push_back(base + ImVec2(new_screen_time - FEATHER_SIZE / 2, y_size));
                            lines_a[var_idx].push_back(base + ImVec2(new_screen_time, y_size / 2));
                            lines_a[var_idx].push_back(base + ImVec2(new_screen_time + FEATHER_SIZE / 2, y_size));

                            // std::println("value: {}, all_zero: {}", (const char *) value, all_zero((const char *)
                            // value));
                            if(new_value) {
                                lines_b[var_idx].push_back(
                                    base + ImVec2(new_screen_time - FEATHER_SIZE / 2, y_size * (1 - old_value)));
                                lines_b[var_idx].push_back(base + ImVec2(new_screen_time, y_size / 2));
                                lines_b[var_idx].push_back(base + ImVec2(new_screen_time + FEATHER_SIZE / 2, 0));
                            } else {
                                lines_b[var_idx].push_back(base + ImVec2(new_screen_time - FEATHER_SIZE / 2, 0));
                                lines_b[var_idx].push_back(base + ImVec2(new_screen_time + FEATHER_SIZE / 2, y_size));
                            }
                        }
                    }
                }

                if(is_vector) {
                    // clip to first_time
                    if(old_time < first_time) { pixel_delta = new_screen_time - (first_time + offset) * zoom; }
                    if(pixel_delta > MIN_TEXT_SIZE) {
                        text_to_draw.emplace_back(fac, old_time < first_time ? first_time : old_time, pixel_delta);
                    }
                }
            }

            // TODO(robin): want last visible value, not last drawn value...
            // this is causing the graphics glitches with the end fixup
            // std::println("new_value {}, time")
            old_values[fac] = {new_value, time, draw_time};
        }
    });

    // for db perf test dump
    if (vars.size() > 0) exit(0);

    // not every variable has necessarily a change at the last viewed change, so draw lines till the end of the
    // waveform viewer,
    float start = (first_time + offset) * zoom;
    float end   = (last_time + 1 + offset) * zoom;

    for(auto & fac : facs) {
        float    space;
        uint64_t time      = first_time;
        bool     is_vector = false;
        for(const auto & [base, var_idx] : bases.at(fac)) {
            auto & var = vars[var_idx];
            is_vector  = var.is_vector();

            auto & line_a = lines_a.at(var_idx);
            auto & line_b = lines_a.at(var_idx);
            if(line_a.size() == 0) {
                char * value = file->get_value_at(fac, first_time);
                auto   v     = not all_zero(value);

                if (not is_vector) {
                    line_a.push_back(base + ImVec2(start, y_size * (1.0 - v)));
                    line_a.push_back(base + ImVec2(end, y_size * (1.0 - v)));
                } else {
                    line_a.push_back(base + ImVec2(start, y_size));
                    line_a.push_back(base + ImVec2(end, y_size));

                    if (v) {
                        line_b.push_back(base + ImVec2(start, 0));
                        line_b.push_back(base + ImVec2(end, 0));
                    }
                }
            } else {
                ImVec2 last_point = line_a.back();
                // TODO(robin): rounding problems?, FEATHER problems?
                time                                               = (last_point.x - base.x) / zoom - offset;
                const auto & [old_value, old_time, last_draw_time] = old_values[fac];
                if(not is_vector) {
                    // Due to the line compression (only one per pixel) we need to do the fixup step here aswell

                    if (last_draw_time != old_time) {
                        line_a.push_back(ImVec2(base.x + (last_draw_time + offset) * zoom, base.y + (1.0 - old_value) *
                    y_size));
                    }
                    line_a.push_back(ImVec2(base.x + end, base.y + (1.0 - old_value) * y_size));
                } else {
                    // Due to the line compression (only one per pixel) we need to do the fixup step here aswell
                    // line_a.push_back(ImVec2(base.x + (std::get<2>(old_values[fac]) + offset) * zoom, base.y +
                    // y_size));
                    line_a.push_back(ImVec2(base.x + end, base.y + y_size));

                    // std::println("old_value {}, time {}", old_value, old_time);
                    line_b.push_back(ImVec2(base.x + end, base.y + (1 - old_value) * y_size));
                }
            }
            // TODO(robin): fixme for vectors
            if(line_a.size() >= 2) { space = line_a.at(line_a.size() - 1).x - line_a.at(line_a.size() - 2).x; }
        }

        if(space > MIN_TEXT_SIZE and is_vector) { text_to_draw.emplace_back(fac, time, space); }
    }

    for(const auto & [fac, time, space] : text_to_draw) {
        char * value = file->get_value_at(fac, time);

        for(const auto & [base, var_idx] : bases.at(fac)) {
            auto text = vars[var_idx].format(value);
            auto end  = clip_text_to_width(text, space - 3 * PADDING);

            draw->AddText(base + ImVec2((time + offset) * zoom + 2 * PADDING, -PADDING), 0xffffffff, &*text.begin(),
                          &*end);
        }
    }

    // TODO(robin): use https://github.com/ocornut/imgui/pull/7972
    // TODO(robin): line at the beginning vanish if start is offscreen
    // TODO(robin): translation and text
    draw->Flags &= ~ImDrawListFlags_AntiAliasedLines;
    for(const auto & line : lines_a) { draw->AddPolyline(&line[0], line.size(), 0xffff00ff, 0, 1.0f / DPI_SCALE); }
    for(const auto & line : lines_b) { draw->AddPolyline(&line[0], line.size(), 0xffff00ff, 0, 1.0f / DPI_SCALE); }
    draw->Flags |= ImDrawListFlags_AntiAliasedLines;

    // Cursor
    auto c_pos = label_width + (cursor_value + offset) * zoom;
    DrawVLine(draw, min, sz, c_pos, CURSOR_COL, 2.0f);

    ImGui::End();
    return cursor_value;
}
bool all_zero(const char * string) {
    while(*string) {
        if((string++)[0] != '0') return false;
    }
    return true;
}
