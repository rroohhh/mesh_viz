#include "node.h"

#include "utils.cpp"
#include "fst_file.h"
#include "waveform_viewer.h"

void Node::render(
    uint64_t c_time,
    const ImVec2& offset,
    const float& zoom,
    std::function<void(std::shared_ptr<Node>)> process_func)
{
	current_time = c_time;
	auto label = std::format("[{}, {}]", x, y);
	ImGui::PushID(label.c_str());
	// std::println("{}", data);
	auto size = ImVec2(NODE_SIZE, NODE_SIZE) * zoom;
	auto base = ImVec2(x * (1.5 * NODE_SIZE), -y * (1.5 * NODE_SIZE)) * zoom;
	auto min = offset + base;
	auto max = offset + base + size;
	if (ImGui::IsRectVisible(min, max)) {
		auto draw = ImGui::GetWindowDrawList();
		if (role.is_fpga) {
			draw->AddRect(min, max, FPGA_COL);
		} else {
			draw->AddRect(min, max, NODE_COL);
		}
		if (highlight) {
			draw->AddRect(
			    min - ImVec2(2, 2), max + ImVec2(2, 2), NODE_HIGHLIGHT_COL, 0.0f, 0, 4.0f);
		}
		// auto label_size = ImGui::CalcTextSize(label.c_str());
		// auto node_size = max - min;

		// if (node_size > label_size) {
		//     draw->AddText(min, 0xffffffff, label.c_str());
		// }
		// + ImVec2{0, ImGui::GetTextLineHeight()}
		ImGui::SetCursorScreenPos(min);
		ImGui::BeginChild(label.c_str(), size);
		try {
			process_func(this->shared_from_this());
		} catch (const std::exception& e) {
			std::cout << e.what() << std::endl;
			// std::println("exception while executing python code: {}", std::current_exception());
		}
		// render_data(data, NODE_SIZE * zoom);
		ImGui::EndChild();
	}
	ImGui::PopID();

	highlight = false;
}

char* NodeVar::value_at_time(clock_t time) const
{
	return owner_node->value_at_time(*this, time);
}

bool NodeVar::is_vector() const
{
	return nbits > 1;
}

std::span<char> NodeVar::format(char* value) const
{
	// TODO(robin): figure out why this can be null sometimes
	if (value != nullptr) {
		return formatter->format(value);
	} else {
		return {};
	}
}


char* Node::get_current_var_value(const NodeVar& var)
{
	return value_at_time(var, current_time);
}

char* Node::value_at_time(const NodeVar& var, simtime_t time)
{
	return ctx->get_value_at(var, time);
}

void Node::add_var_to_viewer(const NodeVar& var)
{
	viewer->add(var);
}

Node::Node(
    int x,
    int y,
    NodeData data,
    FstFile* ctx,
    NodeRoleAttr role,
    decltype(system_config) system_config, WaveformViewer* viewer, Histograms* histograms, AsyncRunner * async_runner) :
    x(x), y(y), data(data), role(role), system_config(system_config), ctx(ctx), viewer(viewer), histograms(histograms), async_runner(async_runner)
{
}

