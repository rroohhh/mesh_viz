#include "canvas_util.h"

#include "core.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "utils.cpp"

#include <print>


const auto ZOOM_MARGIN = 0.01;

void zoom_to(double a, double b, double width, double & zoom, double & offset_f) {
    auto min = ::min(a, b);
	auto max = ::max(a, b);
    auto w = (max - min);
	offset_f = -min + ZOOM_MARGIN * w;
	zoom = (1.0 - 2.0 * ZOOM_MARGIN) * width / w;
}

bool Canvas2D(double & offset_f, double & zoom, int64_t min_time, int64_t max_time) {
	auto sz = ImGui::GetContentRegionAvail();

	sz.x = max(sz.x, 1);
	sz.y = max(sz.y, 1);
	auto width = sz.x;

	auto min = ImGui::GetCursorScreenPos();

	// input handling
	ImGui::SetNextItemAllowOverlap();
	ImGui::InvisibleButton(
		"canvas", sz,
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
			ImGuiButtonFlags_MouseButtonMiddle);


	ImGuiIO& io = ImGui::GetIO();

	const auto canvas_bb = ImRect(min, min + sz);

	const auto is_hovered = ImGui::IsItemHovered() or (canvas_bb.Contains(io.MousePos) and ImGui::IsWindowHovered(ImGuiFocusedFlags_RootAndChildWindows));

	// is any item hovered also is set for generically any child
	// and !ImGui::IsAnyItemHovered());
	// std::println("is hovered {}, sz {}", is_hovered, sz);


	const auto is_active =
		ImGui::IsItemActive() or
		(is_hovered and (ImGui::IsMouseDown(ImGuiButtonFlags_MouseButtonLeft) or
							ImGui::IsMouseDown(ImGuiButtonFlags_MouseButtonRight) or
							ImGui::IsMouseDown(ImGuiButtonFlags_MouseButtonMiddle)));

	if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0)) {
		const auto origin = io.MousePos - ImGui::GetMouseDragDelta(ImGuiMouseButton_Right, 0);
		if (canvas_bb.Contains(origin)) {
			offset_f += io.MouseDelta.x / zoom;
		}
	}

	auto draw = ImGui::GetWindowDrawList();

	auto state = ImGui::GetStateStorage();
	auto middle_origin_relevant_id = ImGui::GetID("canvas##middle_relevant");
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
		state->SetBool(middle_origin_relevant_id, is_active);
	}
	auto middle_origin_relevant = state->GetBool(middle_origin_relevant_id, false);

	if (middle_origin_relevant and (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 2.0) or ImGui::IsMouseReleased(ImGuiMouseButton_Middle))) {
		auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle, 0);
		auto orig = io.MousePos - delta;

		auto window_zoom_start = (orig.x - min.x) / zoom - offset_f;
		auto window_zoom_end = (io.MousePos.x - min.x) / zoom - offset_f;

		DrawVLine(draw, min, sz, (window_zoom_start + offset_f) * zoom, 0xff0000ff, 2.0f);
		DrawVLine(draw, min, sz, (window_zoom_end + offset_f) * zoom, 0xff0000ff, 2.0f);

		std::println("release: {}, {}, {}", ImGui::IsMouseReleased(ImGuiMouseButton_Middle), window_zoom_start, window_zoom_end);
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
	        zoom_to(window_zoom_start, window_zoom_end, width, zoom, offset_f);
			// zoom = 0.98 * width / (window_zoom_end - window_zoom_start);
			// offset_f = -window_zoom_start;
		}
	}
	if (is_hovered and ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
		double old_zoom = zoom;
		// TODO(robin): do this log style
		if (io.MouseWheel > 0) {
			zoom /= std::powf(1.1, io.MouseWheel);
		} else if (io.MouseWheel < 0) {
			zoom /= std::powf(0.9, -io.MouseWheel);
		}
		offset_f -= (io.MousePos.x - min.x) * (1.0 / old_zoom - 1.0 / zoom);
	}
	if (is_hovered) {
		offset_f += MOUSE_WHEEL_DRAG_FACTOR * io.MouseWheelH / zoom;
	}
	if (is_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right)) {
	    zoom_to(min_time, max_time, width, zoom, offset_f);
		// offset_f = min_time + ;
		// zoom = (1 - 2 * ZOOM_MARGIN) * width / (max_time - min_time);
	}

	ImGui::SetCursorScreenPos(min);

	return is_hovered;
}
