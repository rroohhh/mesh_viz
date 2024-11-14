#include "node.h"

#include "utils.cpp"
#include "waveform_viewer.h"

void Node::render_data(const NodeData & data, float width) const {
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
void Node::render(uint64_t c_time, const ImVec2 & offset, const float & zoom,
                  std::function<void(std::shared_ptr<Node>)> process_func, WaveformViewer * v) {
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

char * NodeVar::value_at_time(clock_t time) const { return owner_node->value_at_time(*this, time); }
bool NodeVar::is_vector() { return nbits > 1; }
std::span<char> NodeVar::format(char * value) { return formatter->format(value); }


char * Node::get_current_var_value(const NodeVar & var) { return value_at_time(var, current_time); }

char * Node::value_at_time(const NodeVar & var, simtime_t time) { return ctx->get_value_at(var.handle, time); }

void Node::add_var_to_viewer(const NodeVar & var) { viewer->add(var); }
