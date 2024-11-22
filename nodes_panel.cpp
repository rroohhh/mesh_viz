#include "nodes_panel.h"
#include "utils.cpp"

NodesPanel::NodesPanel(std::vector<std::shared_ptr<Node>> nodes, WaveformViewer* viewer) :
    nodes(nodes), viewer(viewer)
{
}

void NodesPanel::render(
    uint64_t current_time,
    const ImVec2& offset,
    const ImVec2& size,
    const std::function<void(std::shared_ptr<Node>)>& process_func)
{
	ImGui::Text("tick %lu", current_time);
	ImGui::SetNextItemAllowOverlap();
	ImGui::InvisibleButton(
	    "canvas", size,
	    ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
	        ImGuiButtonFlags_MouseButtonMiddle);

	const bool is_hovered = ImGui::IsItemHovered(); // Hovered
	const bool is_active = ImGui::IsItemActive();   // Held
	ImGuiIO& io = ImGui::GetIO();

	if (is_hovered) {
		float old_zoom = zoom;
		// TODO(robin): do this log style
		if (ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
			if (io.MouseWheel > 0) {
				zoom /= std::powf(1.1, io.MouseWheel);
			} else if (io.MouseWheel < 0) {
				zoom /= std::pow(0.9, std::fabs(io.MouseWheel));
			}
			pos += (io.MousePos - offset - pos) * (1.0 - zoom / old_zoom);
		} else {
			pos.x += MOUSE_WHEEL_DRAG_FACTOR * io.MouseWheelH;
			pos.y += MOUSE_WHEEL_DRAG_FACTOR * io.MouseWheel;
		}
	}

	if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0)) {
		pos += io.MouseDelta;
	}

	for (auto& node : nodes) {
		node->render(current_time, offset + pos, zoom, process_func, viewer);
	}
}
