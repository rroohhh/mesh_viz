#include "waveform_viewer.h"
#include "IconsFontAwesome4.h"
#include "imgui.h"
#include "utils.cpp"
#include <future>
#include <print>
#include "highlights.h"

void DrawCenterText(auto& draw, const char* text, const ImVec2& pos)
{
	auto sz = ImGui::CalcTextSize(text);
	draw->AddText(pos - ImVec2(sz.x / 2, 0), 0xffffffff, text);
}

void DrawVLine(
    auto& draw, const ImVec2& min, const ImVec2& sz, float x, int col, float thickness = 1.0f)
{
	draw->AddLine(min + ImVec2(x, 0), min + ImVec2(x, sz.y), col, thickness);
}

auto clip_text_to_width(std::span<char> text, float pixels)
{
	const char* indicator = "+";
	static float indicator_width = 0;
	if (indicator_width == 0) {
		indicator_width = ImGui::CalcTextSize(indicator).x;
	}

	if (ImGui::CalcTextSize(text.data()).x < pixels) {
		return text.end();
	} else {
		pixels -= indicator_width;
		auto end = std::partition_point(text.begin(), text.end(), [=](char& end) {
			return ImGui::CalcTextSize(text.data(), &end).x < pixels;
		});
		// this should always be valid, as:
		// 1. we always clip atleast one char (otherwise the full thing fits and the early check
		// succeeds)
		end[0] = indicator[0];
		end[1] = '\0';
		return end + 1;
	}
}

auto Timeline::render(float zoom, float offset, uint64_t cursor_value, ImRect bb)
{
	int64_t min_time = file->min_time();
	int64_t max_time = file->max_time();

	auto sz = bb.GetSize();
	auto line_height = ImGui::GetTextLineHeight();
	sz.y -= line_height;

	auto width = sz.x;
	auto text_min = bb.Min;
	auto min = text_min + ImVec2{0, line_height};

	first_time = clip(min_time - (int64_t) floor(offset), min_time, max_time);
	int64_t last_time_unclipped = first_time + width / zoom;
	last_time = clip(last_time_unclipped, min_time, max_time);
	bool draw_last = last_time_unclipped > max_time;

	auto draw = ImGui::GetWindowDrawList();

	// Draw time grid
	// want a marker every 10 pixels, so calculate how much every 10 pixels is
	float b = 10;
	float fine_width = b / zoom;
	float human_base = 10;
	int64_t log_step = ceil(log(fine_width) / log(human_base));
	int64_t fine_step = powf(human_base, log_step);
	int64_t coarse_step = powf(human_base, log_step + 1);

	if (fine_step > 0) {
		int64_t time_value = ((first_time + fine_step - 1) / fine_step) * fine_step;
		while (time_value <= last_time) {
			DrawVLine(draw, min, ImVec2(sz.x, 10), (time_value + offset) * zoom, TIMELINE_TICK_COL);
			time_value += fine_step;
		}
	}

	if (coarse_step > 0) {
		int64_t time_value = ((first_time + coarse_step - 1) / coarse_step) * coarse_step;
		while (time_value <= last_time or draw_last) {
			if (time_value > last_time) {
				time_value = last_time;
				draw_last = false;
			}
			DrawCenterText(
			    draw, std::format("{}", time_value).c_str(),
			    text_min + ImVec2{(time_value + offset) * zoom, 0});
			DrawVLine(draw, min, sz, (time_value + offset) * zoom, TIMELINE_TICK_COL, 3.0f);
			time_value += coarse_step;
		}
	}

	// Draw cursor
	float c_pos = ((float) cursor_value + offset) * zoom;
	if (c_pos > 0) {
		DrawVLine(draw, min, sz, c_pos, CURSOR_COL, 2.0f);
	}

	return std::tuple{first_time, last_time};
}

Timeline::Timeline(FstFile* file) : file(file) {}

WaveformViewer::WaveformViewer(FstFile* file, Highlights * highlights) : file(file), highlights(highlights), timeline(file) {}

// TODO(robin): add group hierarchies
void WaveformViewer::add(const NodeVar& var, std::span<std::string> group_hier)
{
	auto guard = std::lock_guard(mutex);
	vars.push_back(var);
	if (fac_dbs.find(var.stable_id()) == fac_dbs.end()) {
		fac_dbs.emplace(var.stable_id(), file->read_wave_db(var));
	}
}

uint64_t WaveformViewer::render()
{
	auto guard = std::lock_guard(mutex);

	int64_t min_time = file->min_time();
	int64_t max_time = file->max_time();

	ImGui::Begin("WaveformViewer");
	auto sz = ImGui::GetContentRegionAvail();
	sz.x = max(sz.x, 1);
	sz.y = max(sz.y, 1);
	auto width = sz.x;

	auto min = ImGui::GetCursorScreenPos();
	auto draw = ImGui::GetWindowDrawList();

	// input handling
	{
		ImGui::SetNextItemAllowOverlap();
		ImGui::InvisibleButton(
		    "canvas", sz,
		    ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
		        ImGuiButtonFlags_MouseButtonMiddle);


		ImGuiIO& io = ImGui::GetIO();

		ImRect waveform_bb = ImRect(min + ImVec2(label_width, timeline_height), min + sz);

		const auto is_hovered = ImGui::IsItemHovered() or (waveform_bb.Contains(io.MousePos));
		const auto is_active =
		    ImGui::IsItemActive() or
		    (is_hovered and (ImGui::IsMouseDown(ImGuiButtonFlags_MouseButtonLeft) or
		                     ImGui::IsMouseDown(ImGuiButtonFlags_MouseButtonRight) or
		                     ImGui::IsMouseDown(ImGuiButtonFlags_MouseButtonMiddle)));

		if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0)) {
			offset_f += io.MouseDelta.x / zoom;
		}
		if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 5.0)) {
			auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle, 0);
			auto orig = io.MousePos - delta;
			did_window_zoom = true;
			auto start = ::min(orig.x, io.MousePos.x);
			auto end = ::max(orig.x, io.MousePos.x);
			window_zoom_start = (start - label_width) / zoom - offset_f;
			window_zoom_end = (end - label_width) / zoom - offset_f;
		}
		if (did_window_zoom) {
			DrawVLine(draw, min, sz, label_width + (window_zoom_start + offset_f) * zoom, 0xff0000ff, 2.0f);
			DrawVLine(draw, min, sz, label_width + (window_zoom_end + offset_f) * zoom, 0xff0000ff, 2.0f);
		}
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle) and did_window_zoom) {
			window_zoom_end = (io.MousePos.x - label_width) / zoom - offset_f;
			offset_f = -window_zoom_start;
			zoom = 0.98 * width / (window_zoom_end - window_zoom_start);
			did_window_zoom = false;
		}
		if (is_hovered and ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
			float old_zoom = zoom;
			// TODO(robin): do this log style
			if (io.MouseWheel > 0) {
				zoom /= std::powf(1.1, io.MouseWheel);
			} else if (io.MouseWheel < 0) {
				zoom /= std::pow(0.9, std::fabs(io.MouseWheel));
			}
			offset_f -= (io.MousePos.x - min.x - label_width) * (1.0 / old_zoom - 1.0 / zoom);
		}
		if (is_hovered) {
			offset_f += MOUSE_WHEEL_DRAG_FACTOR * io.MouseWheelH / zoom;
		}
		if (is_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right)) {
			offset_f = 0;
			zoom = 0.98 * width / (max_time - min_time);
		}
		if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
			cursor_value =
			    clip((io.MousePos.x - min.x - label_width) / zoom - offset_f, min_time, max_time);
			playing = false;
		}

		ImGui::SetCursorScreenPos(min);
	}

	// layout splitters
	{
		// TODO(robin): fix timeline_height being greater than sz.y
		waveforms_height = sz.y - timeline_height;
		ImRect timeline_splitter_bb(
		    min + ImVec2(0, -1.0f + timeline_height), min + ImVec2(sz.x, 1.0f + timeline_height));
		ImGui::SplitterBehavior(
		    timeline_splitter_bb, ImGui::GetID("timeline##Splitter"), ImGuiAxis_Y, &timeline_height,
		    &waveforms_height, 5, 5);

		waveform_width = sz.x - label_width;
		ImRect waveform_splitter_bb(
		    min + ImVec2(-1.0f + label_width, timeline_height),
		    min + ImVec2(1.0f + label_width, sz.y));
		ImGui::SplitterBehavior(
		    waveform_splitter_bb, ImGui::GetID("label##Splitter"), ImGuiAxis_X, &label_width,
		    &waveform_width, 5, 5);
	}


	if (ImGui::SmallButton(playing ? ICON_FA_PAUSE "###playing" : ICON_FA_PLAY "###playing")) {
		playing = not playing;
	}
	if (cursor_value >= max_time) {
		playing = false;
	}
	if (playing)
		cursor_value++;
	ImGui::SetCursorScreenPos(min);


	ImRect timeline_bb(min + ImVec2(label_width, 0), min + ImVec2(sz.x, timeline_height));
	auto [first_time, last_time] = timeline.render(zoom, offset_f, cursor_value, timeline_bb);
	// draw one more to get the piece that is partially cut off
	last_time += 1;

	// TODO(robin): make this float
	float offset = offset_f;

	// waveform labels
	{
		min = ImGui::GetCursorScreenPos() + ImVec2(0, timeline_height);
		ImGui::SetCursorScreenPos(min);

		ImGui::PushClipRect(min, min + sz, false);
		ImGui::SetNextWindowScroll(ImVec2(0.0, -1.0));

		// TODO(robin): mouse io for this
		ImGui::BeginChild("waveforms");
		// padding between timeline and us
		ImGui::Dummy(ImVec2(0.0, 10.0));

		bool first = true;
		ImGuiListClipper clipper;
		clipper.Begin(vars.size(), ImGui::GetTextLineHeightWithSpacing());
		while (clipper.Step()) {
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
				auto& var = vars[i];
				// std::println("var: {}", var.name);

				char* val = var.value_at_time(cursor_value);
				auto formatted = var.format(val);
				auto text = std::format("{}: {}", var.pretty_name(), formatted.data());
				std::span<char> text_span = text;
				clip_text_to_width(text_span, label_width - ImGui::GetCursorPos().x);

				ImGui::PushID(i);

				if (ImGui::Selectable(
				        text_span.data(), false, 0,
				        ImVec2(ImGui::GetContentRegionAvail().x - waveform_width - 5.0f, 0.0)))
					std::println("selected {}", i);
				if  (ImGui::IsItemHovered()) {
					var.owner_node->highlight = true;
				}
				ImGui::SameLine();

				if (not first) {
					ImGui::Separator();
					ImGui::SameLine();
				}
				first = false;

				ImGui::SetCursorPosX(label_width);
				auto size_x = ImGui::GetContentRegionAvail().x;
				ImGui::Dummy(ImVec2(size_x, ImGui::GetTextLineHeightWithSpacing()));
				if  (ImGui::IsItemHovered()) {
					var.owner_node->highlight = true;
				}

				ImGui::SameLine();

				ImGui::SetCursorPosX(label_width);

				// waveform lines
				draw_waveform(first_time, last_time, var);
				ImGui::NewLine();

				// ImGui::NewLine();
				// if (not first) {
				//     ImGui::Separator();
				// }
				// first = false;

				ImGui::PopID();
			}
		}
		ImGui::EndChild();
		ImGui::PopClipRect();
	}

	// Cursor
	auto c_pos = label_width + (cursor_value + offset) * zoom;

	if (c_pos >= label_width) {
		DrawVLine(draw, min, sz, c_pos, CURSOR_COL, 2.0f);
	}

	ImGui::End();
	return cursor_value;
}

void WaveformViewer::draw_waveform(int64_t first_time, int64_t last_time, const NodeVar& var)
{
	auto draw = ImGui::GetWindowDrawList();
	auto base = ImGui::GetCursorScreenPos();
	auto size_x = ImGui::GetContentRegionAvail().x;
	auto y_size = ImGui::GetTextLineHeight() - 2;

	lines_a.clear();
	lines_b.clear();
	highlights_to_draw.clear();
	highlight_colors.clear();
	text_to_draw.clear();

	auto highlight = highlights->highlight_for(var);
	auto& db = fac_dbs.at(var.stable_id());

	uint32_t time = static_cast<uint32_t>(first_time);

	auto current = db.jump_to(WaveValue{time, WaveValueType::Zero});
	auto previous = db.previous_value();
	// if we only have one value, jump_to transports us to the last, first and only
	// value, so previous gives us nothing, so we can use the zeroth
	auto old_value = previous ? *previous : current ? *current : db.get(0);
	// this is now the proper start point, if we leave this at the WaveValue{time} spot
	// we can miss the first value we search
	db.jump_to(old_value);
	time =
	    max(ceil(round((old_value.timestamp + offset_f) * zoom + 0.5f) / zoom - offset_f),
	        old_value.timestamp + 1);

	// std::println("time {}, v {} p {} o {}, this {}", time, current, previous, old_value, db.value());

	while (true) {
		auto maybe_value = db.skip_to(WaveValue{time, WaveValueType::Zero});

		auto maybe_previous = db.previous_value();
		// previous is only valid if we got a new value, if it is not, we are at the
		// last point, so use last
		auto previous = (maybe_previous and maybe_value) ? *maybe_previous : db.last();

		// if we dont have a new value, copy the last valid one
		auto value = maybe_value ? *maybe_value : WaveValue{last_time, db.last().type};

		// these get floored, because otherwise the not anti aliased rendering fucks up
		// clip left edge
		auto old_screen_time = floor((old_value.timestamp + offset_f) * zoom);
		auto new_screen_time = floor((value.timestamp + offset_f) * zoom);
		bool new_screen_time_is_clipped = new_screen_time > size_x;
		auto previous_screen_time = floor((previous.timestamp + offset_f) * zoom);

		// std::println("v {} p {} o {}", value, previous, old_value);

		if (new_screen_time > 0) {
			// add initial point
			if (lines_a.size() == 0) {
				if (var.is_vector()) {
					lines_a.push_back(base + ImVec2(max(0, old_screen_time), y_size));
					lines_b.push_back(
					    base +
					    ImVec2(max(0, old_screen_time), y_size * (1.0 - (int) old_value.type)));
				} else {
					lines_a.push_back(
					    base +
					    ImVec2(max(0, old_screen_time), y_size * (1.0 - (int) old_value.type)));
				}
			}

			// we always skip over at least one timestamp
			if (previous.timestamp > old_value.timestamp) {
				if (var.is_vector()) {
					lines_a.push_back(base + ImVec2(max(0, previous_screen_time), y_size));
					lines_b.push_back(
					    base +
					    ImVec2(max(0, previous_screen_time), y_size * (1.0 - (int) previous.type)));
				} else {
					lines_a.push_back(
					    base +
					    ImVec2(max(0, previous_screen_time), y_size * (1.0 - (int) previous.type)));
				}
			}

			if (var.is_vector()) {
				lines_a.push_back(base + ImVec2(new_screen_time - FEATHER_SIZE / 2, y_size));
				if (not new_screen_time_is_clipped) {
					lines_a.push_back(base + ImVec2(new_screen_time, y_size / 2.0));
					lines_a.push_back(base + ImVec2(new_screen_time + FEATHER_SIZE / 2, y_size));
				}

				lines_b.push_back(
				    base +
				    ImVec2(
				        new_screen_time - FEATHER_SIZE / 2, y_size * (1.0 - (int) previous.type)));
				if (not new_screen_time_is_clipped) {
					lines_b.push_back(base + ImVec2(new_screen_time, y_size / 2.0));
					lines_b.push_back(
					    base +
					    ImVec2(
					        new_screen_time + FEATHER_SIZE / 2, y_size * (1.0 - (int) value.type)));
				}
			} else {
				if (new_screen_time > 0.0f)
					lines_a.push_back(
					    base + ImVec2(new_screen_time, y_size * (1.0 - (int) previous.type)));
				if (not new_screen_time_is_clipped) {
					lines_a.push_back(
					    base + ImVec2(new_screen_time, y_size * (1.0 - (int) value.type)));
				}
			}

			auto hl_start_ts = old_value.timestamp;
			while (hl_start_ts < value.timestamp) {
				auto hl_start_screen_time = floor((hl_start_ts + offset_f) * zoom);
				// ceil to next pixel
				double next_pixel = round(hl_start_screen_time + 0.5f);
				// ceil to next valid timestamp
				auto next =
					max(ceil(next_pixel / zoom - offset_f),
						hl_start_ts + 1); // value.timestamp + ceil(per_pixel);

				auto [should_hl, hl_color] = highlight.should_highlight(hl_start_ts, next);

				if (should_hl) {
					// auto start = base + ImVec2(max(old_screen_time, 0), -1);
					// auto end = base + ImVec2(new_screen_time, y_size + 1);

					auto hl_end_screen_time = floor((hl_start_ts + 1 + offset_f) * zoom);

					auto start = base + ImVec2(max(hl_start_screen_time, 0), -1);
					auto end = base + ImVec2(hl_end_screen_time, y_size + 1);
					if (highlights_to_draw.size() > 0 and highlights_to_draw.back().x >= start.x) {
						highlights_to_draw.back() = end;
					} else {
						highlights_to_draw.push_back(start);
						highlights_to_draw.push_back(end);
						highlight_colors.push_back(hl_color);
					}
				}
				hl_start_ts = next;
			}
		}

		auto text_space = (new_screen_time - max(0, previous_screen_time));
		if (var.is_vector() and text_space > MIN_TEXT_SIZE and
		    previous.type != WaveValueType::Zero) {
			text_to_draw.emplace_back(time, previous_screen_time, text_space);
		}

		// because of the scrollbar and custom clipping above, we might never reach
		// `last_time`, so stop once we clipped once
		if (value.timestamp >= last_time or new_screen_time_is_clipped) {
			break;
		}

		// ceil to next pixel
		double next_pixel = round(new_screen_time + 0.5f);
		// ceil to next valid timestamp
		time =
		    max(ceil(next_pixel / zoom - offset_f),
		        value.timestamp + 1); // value.timestamp + ceil(per_pixel);

		// std::println("")
		old_value = value;
	}

	draw->Flags &= ~ImDrawListFlags_AntiAliasedLines;
	// std::println("drawing lines with {} and {} points", lines_a.size(),
	// lines_b.size());
	draw->AddPolyline(&lines_a[0], lines_a.size(), 0xffffffff, 0, 1.0f / DPI_SCALE);
	draw->AddPolyline(&lines_b[0], lines_b.size(), 0xffffffff, 0, 1.0f / DPI_SCALE);
	draw->Flags |= ImDrawListFlags_AntiAliasedLines;
	float last = 0;
	for (size_t i = 0; i < highlights_to_draw.size(); i+= 2) {
		auto start = highlights_to_draw[i];
		auto end = highlights_to_draw[i + 1];
		auto col = highlight_colors[i / 2];
		auto w = end.x - start.x;
		const auto MIN_W = 5.0;
		if (w < MIN_W) {
			start.x -= (MIN_W - w) / 2.0;
			if (start.x < last) {
				// the min size thing can make the rects overlap
				// fix this here
				start.x = last;
			}
			end.x += (MIN_W - w) / 2.0;
		}
		last = end.x;
		// IM_COL32(0x59, 0x28, 0xED, 0x7f)
		draw->AddRectFilled(start, end, col);
	}

	for (auto & [time, screen_time, text_space] : text_to_draw) {
		char* value = file->get_value_at(var, time);
		auto text = var.format(value);
		auto end = clip_text_to_width(text, text_space - 3 * PADDING - 2 * FEATHER_SIZE);
		draw->AddText(
			base + ImVec2(max(0, screen_time) + 1 * PADDING + FEATHER_SIZE, -PADDING),
			0xffffffff, &*text.begin(), &*end);
	}
}
