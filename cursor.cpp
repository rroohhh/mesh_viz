#include "cursor.h"
#include "core.h"
#include "imgui.h"

#include "utils.cpp"

Cursor::Cursor(int64_t min_time, int64_t max_time) : min_time(min_time), max_time(max_time) {}

bool Cursor::render(double offset, double zoom, bool update) {
	double c_pos = ((double) pos + offset) * zoom;
	auto draw = ImGui::GetWindowDrawList();
	auto min = ImGui::GetCursorScreenPos();
	auto sz = ImGui::GetContentRegionAvail();
	auto io = ImGui::GetIO();

	if (update) {
		pos =
			clip((io.MousePos.x - min.x) / zoom - offset, min_time, max_time);
	}

	if (c_pos > 0) {
		DrawVLine(draw, min, sz, c_pos, CURSOR_COL, 2.0f);
	}

	return update;
}
