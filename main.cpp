#include "IconsFontAwesome4.h"
#include <pybind11/embed.h>
// #include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
namespace py = pybind11;
#include "libfst/fstapi.h"
#include "mesh_utils.cpp"

#include <cmath>
#include <csignal>
#include <functional>
#include <iostream>
#include <set>
#include <vector>
// #define IMGUI_ENABLE_FREETYPE
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>

#include "utils.cpp"

static void glfw_error_callback(int error, const char * description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

struct WaveformViewer;

using simtime_t      = uint64_t;
using simtimedelta_t = int64_t;
using handle_t       = fstHandle;
struct FstFile;

const int CURSOR_COL        = 0xff00ff00;
const int NODE_COL          = 0xff00ff00;
const int TIMELINE_TICK_COL = 0xaa0000ff;

struct Node;

// node vars should know which Node they belong to, because we want to support multiple fstfiles per trace (ie up to one
// per node to avoid inter thread sync)
struct NodeVar {
    std::string name;
    // TODO(robin): shared_ptr?
    std::shared_ptr<Node> owner_node;

    NodeVar(std::string name, handle_t handle, std::shared_ptr<Node> owner_node)
        : name(name), handle(handle), owner_node(owner_node) {}

    auto value_at_time(clock_t time) const;

private:
    handle_t handle;
    friend Node;
    friend FstFile;
    friend WaveformViewer;
};

struct NodeData {
    std::string name;
    std::string compname;

    std::map<std::string, NodeData> subscopes;
    std::map<std::string, NodeVar>  variables;
};

const int NODE_SIZE = 100;
struct Node : public std::enable_shared_from_this<Node> {
    int      x, y;
    NodeData data;
    uint64_t current_time;

    Node(int x, int y, NodeData data, FstFile * ctx) : x(x), y(y), data(data), ctx(ctx) {}

    void render(uint64_t c_time, const ImVec2 & offset, const float & zoom, std::function<void(std::shared_ptr<Node>)> process_func,
                WaveformViewer * v) {
        viewer       = v;
        current_time = c_time;
        auto label   = std::format("[{}, {}]", x, y);
        ImGui::PushID(label.c_str());
        // std::println("{}", data);
        auto size = ImVec2(NODE_SIZE, NODE_SIZE) * zoom;
        auto base = ImVec2(x * (1.5 * NODE_SIZE), y * (1.5 * NODE_SIZE)) * zoom;
        auto min  = offset + base;
        auto max  = offset + base + size;
        if(ImGui::IsRectVisible(min, max)) {
            auto draw = ImGui::GetWindowDrawList();
            draw->AddRect(min, max, NODE_COL);
            // auto label_size = ImGui::CalcTextSize(label.c_str());
            // auto node_size = max - min;

            // if (node_size > label_size) {
            //     draw->AddText(min, 0xffffffff, label.c_str());
            // }
            // + ImVec2{0, ImGui::GetTextLineHeight()}
            ImGui::SetCursorScreenPos(min);
            ImGui::BeginGroup();
            ImGui::PushItemWidth(NODE_SIZE * zoom);

            try {
                process_func(this->shared_from_this());
            } catch(const std::exception & e) {
                std::cout << e.what() << std::endl;
                // std::println("exception while executing python code: {}", std::current_exception());
            }
            // render_data(data, NODE_SIZE * zoom);
            ImGui::PopItemWidth();
            ImGui::EndGroup();
        }
        ImGui::PopID();
    }

    char * get_current_var_value(const NodeVar & var);

    char * value_at_time(const NodeVar & var, simtime_t time);

    void add_var_to_viewer(const NodeVar & var);

private:
    FstFile *        ctx;
    WaveformViewer * viewer;

    void render_data(const NodeData & data, float width) const {
        // for (auto & [name, var] : data.variables) {
        //     ImGui::Text("V: %s", name.c_str());
        // }
        for(auto & [name, d] : data.subscopes) {
            // std::println("name: {}", name);
            auto label = std::format("S: {}", name);
            if(ImGui::CollapsingHeaderWithWidth(label.c_str(), 0, width)) {
                ImGui::TreePush(label.c_str());
                render_data(d, width);
                ImGui::TreePop();
            }
        }
    }
};

auto NodeVar::value_at_time(clock_t time) const { return owner_node->value_at_time(*this, time); }

using value_change_cb_t = std::function<void(uint64_t time, handle_t facidx, const unsigned char * value)>;

struct FstFile {
    using fstReader = void *;
    fstReader reader;
    char *    buffer;

    ~FstFile() {
        fstReaderClose(reader);
        if(buffer) { delete[] buffer; }
    }

    FstFile(const char * path) : reader(fstReaderOpen(path)), buffer(nullptr) {}
    std::vector<std::shared_ptr<Node>> read_nodes() {
        if(buffer) delete[] buffer;
        std::vector<std::shared_ptr<Node>> nodes;

        fstReaderIterateHierRewind(reader);

        fstHier * hier;

        bool next_is_node  = false;
        bool in_node_scope = false;
        int  depth         = 0;

        std::vector<std::string> hierarchy_stack;
        auto                     node = std::make_shared<Node>(0, 0, NodeData{}, this);
        NodeData *               current_node_data;

        int max_bits = 0;

        while((hier = fstReaderIterateHier(reader))) {
            switch(hier->htyp) {
            case FST_HT_SCOPE: {
                if(in_node_scope) {
                    depth++;

                    auto scope = hier->u.scope;
                    hierarchy_stack.push_back({scope.name});
                    current_node_data =
                        &current_node_data->subscopes.try_emplace({scope.name}, NodeData{}).first->second;
                }
                if(next_is_node) {
                    next_is_node  = false;
                    in_node_scope = true;
                    depth         = 1;
                }

                break;
            }
            case FST_HT_UPSCOPE: {
                if(in_node_scope) {
                    depth--;
                    if(depth == 0) {
                        nodes.push_back(node);
                        in_node_scope = false;
                    } else {
                        hierarchy_stack.pop_back();
                        current_node_data = &node->data;
                        for(const auto & name : hierarchy_stack) {
                            current_node_data = &current_node_data->subscopes.at({name});
                        }
                    }
                }
                break;
            }
            case FST_HT_VAR: {
                if(in_node_scope) {
                    auto var = hier->u.var;
                    // std::println("adding var {}", var.name);
                    current_node_data->variables.insert({{var.name}, {var.name, var.handle, node}});
                    max_bits = max_bits > var.length ? max_bits : var.length;
                }
                // std::println("[var]: name: {}", var.name);
                break;
            }
            case FST_HT_ATTRBEGIN: {
                auto attr = hier->u.attr;
                if(attr.typ == FST_AT_MISC and attr.subtype == FST_MT_COMMENT) {
                    auto parsed   = parse_attr(attr.name);
                    auto nodeattr = std::get_if<NodeAttr>(&parsed);
                    if(nodeattr) {
                        assert(not in_node_scope);
                        next_is_node = true;
                        // TODO(robin): ownership of this?
                        node              = std::make_shared<Node>(nodeattr->x, nodeattr->y, NodeData{}, this);
                        current_node_data = &node->data;
                    }
                    // std::visit([](auto v) {
                    //     std::println("[attr]: {}", v);
                    // }, );
                }
                break;
            }
            case FST_HT_ATTREND: {
                std::println("[attr]: end");
                break;
            }
            }
        }

        buffer = new char[max_bits];
        return nodes;
    }

    static void value_change_callback(void * user_callback_data_pointer, uint64_t time, handle_t facidx,
                                      const unsigned char * value) {
        auto cb = (value_change_cb_t *)user_callback_data_pointer;
        if(cb) { (*cb)(time, facidx, value); }
    }

    static void value_change_callback2(void * user_callback_data_pointer, uint64_t time, handle_t facidx,
                                       const unsigned char * value, uint32_t len) {
        auto cb = (value_change_cb_t *)user_callback_data_pointer;
        if(cb) { (*cb)(time, facidx, value); }
    }

    void read_changes(uint64_t min_time, uint64_t max_time, std::vector<NodeVar> vars, value_change_cb_t cb) {
        fstReaderClrFacProcessMaskAll(reader);
        for(const auto & var : vars) { fstReaderSetFacProcessMask(reader, var.handle); }
        fstReaderIterBlocksSetNativeDoublesOnCallback(reader, 1);
        fstReaderSetLimitTimeRange(reader, min_time, max_time);
        fstReaderIterBlocks2(reader, FstFile::value_change_callback, FstFile::value_change_callback2, &cb, nullptr);
    }

    uint64_t min_time() { return fstReaderGetStartTime(reader); }

    uint64_t max_time() { return fstReaderGetEndTime(reader); }

    char * get_value_at(handle_t handle, uint64_t time) {
        return fstReaderGetValueFromHandleAtTime(reader, time, handle, buffer);
    }
};

char * Node::get_current_var_value(const NodeVar & var) { return value_at_time(var, current_time); }

char * Node::value_at_time(const NodeVar & var, simtime_t time) { return ctx->get_value_at(var.handle, time); }

template <>
struct std::formatter<NodeVar> {
    constexpr auto parse(std::format_parse_context & ctx) { return ctx.begin(); }
    auto format(const auto & v, auto & ctx) const { return std::format_to(ctx.out(), "NodeVar{{name={}}}", v.name); }
};

template <>
struct std::formatter<NodeData> {
    constexpr auto parse(std::format_parse_context & ctx) { return ctx.begin(); }
    auto           format(const auto & v, auto & ctx) const {
        auto ret = std::format_to(ctx.out(), "NodeData{{name={}}}", v.name);
        for(auto & [scope, d] : v.subscopes) { std::format_to(ctx.out(), "scope: {}: data: {}", scope, d); }
        return ret;
    }
};

void DrawVLine(auto & draw, const ImVec2 & min, const ImVec2 & sz, float x, int col, float thickness = 1.0f) {
    draw->AddLine(min + ImVec2(x, 0), min + ImVec2(x, sz.y), col, thickness);
}

struct Timeline {
    FstFile * file;
    bool      playing = false;
    uint64_t  first_time, last_time;

    Timeline(FstFile * file) : file(file) {}

    void DrawCenterText(auto & draw, const char * text, const ImVec2 & pos) {
        auto sz = ImGui::CalcTextSize(text);
        draw->AddText(pos - ImVec2(sz.x / 2, 0), 0xffffffff, text);
    }

    auto render(float zoom, int64_t offset, uint64_t cursor_value, ImRect bb) {
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
};

const auto MIN_TEXT_SIZE = 20;
const auto PADDING       = 3;
float      DPI_SCALE     = 1;

// returns the end of the text
char * clip_text_to_width(char * text, float pixels) {
    const char * indicator       = "..";
    static float indicator_width = 0;
    if(indicator_width == 0) { indicator_width = ImGui::CalcTextSize(indicator).x; }

    auto length = strlen(text);
    if(ImGui::CalcTextSize(text).x < pixels) {
        return text + length;
    } else {
        // std::println("string = `{}`, size = {}, actual = {}", text, pixels, ImGui::CalcTextSize(text).x);
        pixels -= indicator_width;
        auto end = std::partition_point(text, text + length,
                                        [=](char & end) { return ImGui::CalcTextSize(text, &end).x < pixels; });
        // std::println("clipped_size = {}", end - text);
        // this should always be valid, as:
        // 1. we always clip atleast one char (otherwise the full thing fits and the early check succeeds)
        // 2. we can overwrite the 'NUL' char
        end[0] = indicator[0];
        end[1] = indicator[1];
        return end + 2;
    }
}

// const auto FEATHER_SIZE = 4.0f;
const auto FEATHER_SIZE = 0.0f;
const auto MOUSE_WHEEL_DRAG_FACTOR = 10.0f;

struct WaveformViewer {
    FstFile * file;
    Timeline  timeline;
    float     zoom         = 1.0;
    float     offset_f     = 0.0;
    uint64_t  cursor_value = 0;

    float timeline_height = 100, waveforms_height = 100;
    float label_width = 100, waveform_width = 100;

    handle_t                                            maxHandle;
    std::set<fstHandle>                                 facs;
    std::vector<std::vector<std::pair<ImVec2, size_t>>> bases;
    std::vector<std::tuple<uint64_t, int64_t, int64_t>> old_values;

    std::vector<std::vector<ImVec2>> lines;

    // fst handle, time, size
    std::vector<std::tuple<fstHandle, int64_t, float>> text_to_draw;

    WaveformViewer(FstFile * file) : file(file), timeline(file) {}

    uint64_t render() {
        int64_t min_time = file->min_time();
        int64_t max_time = file->max_time();

        ImGui::Begin("WaveformViewer");
        auto sz    = ImGui::GetContentRegionAvail();
        sz.x       = max(sz.x, 1);
        sz.y       = max(sz.y, 1);
        auto width = sz.x;

        bool is_hovered = false;
        bool is_active  = false;

        auto min = ImGui::GetCursorScreenPos();
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("canvas", sz,
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                                   ImGuiButtonFlags_MouseButtonMiddle);
        is_hovered |= ImGui::IsItemHovered();
        is_active |= ImGui::IsItemActive();
        ImGui::SetCursorScreenPos(min);

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

        ImGuiIO & io = ImGui::GetIO();
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
        if(is_hovered) {
            offset_f += MOUSE_WHEEL_DRAG_FACTOR * io.MouseWheelH / zoom;
        }
        if(is_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right)) {
            offset_f = 0;
            zoom     = 0.98 * width / (max_time - min_time);
        }
        if(is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
            cursor_value = clip((io.MousePos.x - min.x - label_width) / zoom - offset_f, min_time, max_time);
            // playing = false;
        }

        ImRect timeline_bb(min + ImVec2(label_width, 0), min + ImVec2(sz.x, timeline_height));
        const auto & [first_time, last_time] = timeline.render(zoom, offset_f, cursor_value, timeline_bb);

        // Timeline labels
        {
            min = ImGui::GetCursorScreenPos() + ImVec2(0, timeline_height);
            ImGui::SetCursorScreenPos(min);

            // clear without resize
            bases.assign(bases.size(), {});
            int i = 0;

            ImGui::Separator();
            for(const auto & var : vars) {
                auto base = ImGui::GetCursorScreenPos();
                bases[var.handle].emplace_back(ImVec2(min.x + label_width, base.y + 1), i);
                char * val = var.value_at_time(cursor_value);
                // float size = label_width - ImGui::CalcTextSize(var.name.c_str());
                ImGui::Text("%s: ", var.name.c_str());
                ImGui::SameLine();
                ImGui::TextUnformatted(val, clip_text_to_width(val, label_width - ImGui::GetCursorPos().x));
                ImGui::Separator();
                i++;

                old_values[var.handle] = {0, -1, -1};
            }
        }

        // Waveform lines
        for(auto & line : lines) { line.clear(); }

        int64_t offset = offset_f;
        auto    draw   = ImGui::GetWindowDrawList();
        auto    y_size = ImGui::GetTextLineHeight() - 2;

        text_to_draw.clear();

        // gtkwave does the following two opts:
        // 1. only render value text if enough space is there (thats probably why surfer is so slow)
        // 2. only render one transition per screen pixel
        // 2. we can also compress the changes and preload them maybe? (one uint32_t or so per change?)
        file->read_changes(first_time, last_time, vars, [&](uint64_t time, handle_t fac, const unsigned char * value) {
            if(time == 0) {
                auto v          = value[0] == '1';
                old_values[fac] = {v, time, 0};

                return;
            } else {
                const auto & [old_value, old_time, last_draw_time] = old_values[fac];
                auto draw_time                                     = last_draw_time;
                auto new_value                                     = value[0] == '1';
                if((not((old_time < first_time and time < first_time) or
                        (old_time > last_time and time > last_time))) and
                   old_time >= 0) {
                    // skip changes for same pixel
                    auto pixel_delta     = (time - last_draw_time) * zoom;
                    auto old_screen_time = (old_time + offset) * zoom;
                    auto new_screen_time = (time + offset) * zoom;
                    if(pixel_delta >= 1.0f / DPI_SCALE) {
                        draw_time = time;
                        for(const auto & [base, var_idx] : bases.at(fac)) {
                            // we decided to draw the first transition, so add the initial point
                            if(lines[var_idx].size() == 0) {
                                lines[var_idx].push_back(
                                    base + ImVec2((first_time + offset) * zoom, y_size * (1.0 - old_value)));
                            } else {
                                // last_draw_time is only valid once we have draw atleast one point...
                                if(old_time != last_draw_time) {
                                    lines[var_idx].push_back(
                                        base + ImVec2((last_draw_time + offset) * zoom, y_size * (1.0 - old_value)));
                                }
                            }

                            lines[var_idx].push_back(
                                base + ImVec2(new_screen_time - FEATHER_SIZE / 2, y_size * (1.0 - old_value)));
                            lines[var_idx].push_back(
                                base + ImVec2(new_screen_time + FEATHER_SIZE / 2, y_size * (1.0 - new_value)));
                        }
                    }

                    // clip to first_time
                    if(old_time < first_time) { pixel_delta = new_screen_time - (first_time + offset) * zoom; }
                    if(pixel_delta > MIN_TEXT_SIZE) {
                        text_to_draw.emplace_back(fac, old_time < first_time ? first_time : old_time, pixel_delta);
                    }
                }

                old_values[fac] = {new_value, time, draw_time};
            }
        });

        // not every variable has necessarily a change at the last viewed change, so draw lines till the end of the
        // waveform viewer,
        float start = (first_time + offset) * zoom;
        float end   = (last_time + 1 + offset) * zoom;

        for(auto & fac : facs) {
            float    space;
            uint64_t time = first_time;
            for(const auto & [base, var_idx] : bases.at(fac)) {
                auto & line = lines.at(var_idx);
                if(line.size() == 0) {
                    char * value = file->get_value_at(fac, first_time);
                    auto   v     = value[0] == '1';
                    line.push_back(base + ImVec2(start, y_size * (1.0 - v)));
                    line.push_back(base + ImVec2(end, y_size * (1.0 - v)));
                } else {
                    ImVec2 last_point = line.back();
                    // TODO(robin): rounding problems?
                    time = (last_point.x - base.x) / zoom - offset;
                    // Due to the line compression (only one per pixel) we need to do the fixup step here aswell
                    line.push_back(ImVec2(base.x + (std::get<2>(old_values[fac]) + offset) * zoom,
                                          base.y + (1.0 - std::get<0>(old_values[fac])) * y_size));
                    line.push_back(ImVec2(base.x + end, base.y + (1.0 - std::get<0>(old_values[fac])) * y_size));
                }
                space = line.at(line.size() - 1).x - line.at(line.size() - 2).x;
            }

            if(space > MIN_TEXT_SIZE) { text_to_draw.emplace_back(fac, time, space); }
        }

        for(const auto & [fac, time, space] : text_to_draw) {
            char * value = file->get_value_at(fac, time);
            auto   end   = clip_text_to_width(value, space - 3 * PADDING);

            for(const auto & [base, var_idx] : bases.at(fac)) {
                draw->AddText(base + ImVec2((time + offset) * zoom + 2 * PADDING, -PADDING), 0xffffffff, value, end);
            }
        }

        // TODO(robin): use https://github.com/ocornut/imgui/pull/7972
        // TODO(robin): line at the beginning vanish if start is offscreen
        // TODO(robin): translation and text
        draw->Flags &= ~ImDrawListFlags_AntiAliasedLines;
        for(const auto & line : lines) { draw->AddPolyline(&line[0], line.size(), 0xffff00ff, 0, 1.0f / DPI_SCALE); }
        draw->Flags |= ImDrawListFlags_AntiAliasedLines;

        // Cursor
        auto c_pos = label_width + (cursor_value + offset) * zoom;
        DrawVLine(draw, min, sz, c_pos, CURSOR_COL, 2.0f);

        ImGui::End();
        return cursor_value;
    }

    void add(const NodeVar & var) {
        std::println("adding {}", var);
        vars.push_back(var);
        facs.insert(var.handle);
        lines.resize(vars.size());
        maxHandle = max(maxHandle, var.handle);
        // index vs length -> need to add one
        bases.resize(maxHandle + 1);
        old_values.resize(maxHandle + 1);
    }

private:
    std::vector<NodeVar> vars;
};

void Node::add_var_to_viewer(const NodeVar & var) { viewer->add(var); }

struct NodesPanel {
    float                              zoom = 1.0;
    ImVec2                             pos;
    int                                grid_size = 10;
    std::vector<std::shared_ptr<Node>> nodes;
    WaveformViewer *                   viewer;

    NodesPanel(std::vector<std::shared_ptr<Node>> nodes, WaveformViewer * viewer) : nodes(nodes), viewer(viewer) {}

    void render(uint64_t current_time, const ImVec2 & offset, const ImVec2 & size, const auto & process_func) {
        ImGui::Text("tick %lu", current_time);
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("canvas", size,
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                                   ImGuiButtonFlags_MouseButtonMiddle);

        const bool is_hovered = ImGui::IsItemHovered(); // Hovered
        const bool is_active  = ImGui::IsItemActive();  // Held
        ImGuiIO &  io         = ImGui::GetIO();

        if(is_hovered) {
            float old_zoom = zoom;
            // TODO(robin): do this log style
            if (ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
                if(io.MouseWheel > 0) {
                    zoom /= std::powf(1.1, io.MouseWheel);
                } else if(io.MouseWheel < 0) {
                    zoom /= std::pow(0.9, std::fabs(io.MouseWheel));
                }
                pos += (io.MousePos - offset - pos) * (1.0 - zoom / old_zoom);
            } else {
                pos.x += MOUSE_WHEEL_DRAG_FACTOR * io.MouseWheelH;
                pos.y += MOUSE_WHEEL_DRAG_FACTOR * io.MouseWheel;
            }
        }

        if(is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0)) { pos += io.MouseDelta; }

        for(auto & node : nodes) { node->render(current_time, offset + pos, zoom, process_func, viewer); }
    }
};

PYBIND11_MAKE_OPAQUE(std::map<std::string, NodeVar>)
PYBIND11_MAKE_OPAQUE(std::map<std::string, NodeData>)

PYBIND11_EMBEDDED_MODULE(mesh_viz, m) {
    py::bind_map<std::map<std::string, NodeVar>>(m, "MapStringNodeVar");
    py::bind_map<std::map<std::string, NodeData>>(m, "MapStringNodeData");

    py::class_<Node, std::shared_ptr<Node>>(m, "Node")
        .def_readonly("x", &Node::x)
        .def_readonly("y", &Node::y)
        .def_readonly("data", &Node::data)
        .def("get_current_var_value", &Node::get_current_var_value)
        .def("add_var_to_viewer", &Node::add_var_to_viewer);

    py::class_<NodeData>(m, "NodeData")
        .def_readonly("name", &NodeData::name)
        .def_readonly("compname", &NodeData::compname)
        .def_readonly("variables", &NodeData::variables)
        .def_readonly("subscopes", &NodeData::subscopes);

    py::class_<NodeVar>(m, "NodeVar").def_readonly("name", &NodeVar::name);
}

void py_init_module_imgui_main(py::module & m);

PYBIND11_EMBEDDED_MODULE(imgui, m) { py_init_module_imgui_main(m); }

void signalHandler(int signum) { exit(signum); }

// Main code
int main(int, char **) {
    signal(SIGINT, signalHandler);
    py::scoped_interpreter guard{}; // start the interpreter and keep it alive

    auto module          = py::module::import("read_stuff");
    auto mesh_viz_module = py::module::import("mesh_viz");
    auto imgui_module    = py::module::import("imgui");

    auto process_func = module.attr("process");

    glfwSetErrorCallback(glfw_error_callback);
    if(!glfwInit()) return 1;

        // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char * glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char * glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char * glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    float xscale, yscale;

    // Create window with graphics context
    GLFWwindow * window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
    if(window == nullptr) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // glfwGetWindowContentScale(window, &xscale, &yscale);
    // DPI_SCALE = xscale;
    // std::println("xscale = {}, yscale = {}", xscale, yscale);
    glfwSetWindowContentScaleCallback(window, [](GLFWwindow * window, float xscale, float yscale) {
        std::println("xscale = {}, yscale = {}", xscale, yscale);
        DPI_SCALE = xscale;
    });

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO & io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.Fonts->AddFontFromFileTTF("../NotoSans[wdth,wght].ttf", 22);
    static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
    ImFontConfig         icons_config;
    icons_config.MergeMode  = true;
    icons_config.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF("../fontawesome-webfont.ttf", 22.0f, &icons_config, icons_ranges);

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    FstFile        f("../../toplevel/out.fst");
    WaveformViewer waveform_viewer(&f);
    NodesPanel     panel(f.read_nodes(), &waveform_viewer);

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if(glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport * viewport = ImGui::GetMainViewport();

        {
            auto dockid = ImGui::DockSpaceOverViewport();

            auto current_time = waveform_viewer.render();

            ImGui::Begin(
                "Mesh", nullptr,
                ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoScrollWithMouse); // Create a window called "Hello, world!" and append into it.
            if(ImGui::IsKeyPressed(ImGuiKey_R)) {
                try {
                    module.reload();
                    process_func = module.attr("process");
                } catch(...) {}
            }

            ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
            ImVec2 canvas_sz = ImGui::GetContentRegionAvail();

            ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

            panel.render(current_time, canvas_p0, canvas_p1, process_func);

            ImGui::End();

            // auto [current_time, min_time, max_time] = timeline.render();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w,
                     clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
