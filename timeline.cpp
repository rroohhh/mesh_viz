#include "timeline.h"

#include "core.h"
#include "imgui.h"
#include "utils.cpp"

#include <cmath>
#include <print>

std::pair<uint32_t, uint32_t> Timeline::render(double zoom, double offset)
{
	// std::println("min_time {}, max_time {}, offset {}, zoom {}", min_time, max_time, offset, zoom);

	auto sz = ImGui::GetContentRegionAvail();

	auto line_height = ImGui::GetTextLineHeight();
	sz.y -= line_height;

	auto width = sz.x;

	auto text_min = ImGui::GetCursorScreenPos();

	auto min = text_min + ImVec2{0, line_height};

	uint32_t first_time = clip(-(int64_t) std::floor(offset), min_time, max_time);
	int64_t last_time_unclipped = first_time + width / zoom;
	uint32_t last_time = clip(last_time_unclipped, min_time, max_time);

	// std::println("width  {}, first_time {}, last_time {}", width, first_time, last_time);

	bool draw_last = last_time_unclipped > max_time;

	auto draw = ImGui::GetWindowDrawList();

	// Draw time grid
	// want a marker every 10 pixels, so calculate how much every 10 pixels is
	double b = 10;
	double fine_width = b / zoom;
	double human_base = 10;
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
			    text_min + ImVec2{(float) ((time_value + offset) * zoom), 0});
			DrawVLine(draw, min, sz, (time_value + offset) * zoom, TIMELINE_TICK_COL, 3.0f);
			time_value += coarse_step;
		}
	}

	// Draw cursor
	cursor->render(offset, zoom, false);

	// double c_pos = ((double) cursor_value + offset) * zoom; //
	// if (c_pos > 0) {
	// 	DrawVLine(draw, min, sz, c_pos, CURSOR_COL, 2.0f);
	// }

	return {first_time, last_time};
}

Timeline::Timeline(uint32_t min_time, uint32_t max_time, std::shared_ptr<Cursor> cursor) : min_time(min_time), max_time(max_time), cursor(cursor) {}
