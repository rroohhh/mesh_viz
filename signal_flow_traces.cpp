#include "signal_flow_traces.h"
#include "canvas_util.h"
#include "imgui.h"
#include "node.h"
#include "utils.cpp"

#include "format_util.h"
#include <format>
#include <ranges>
#include <algorithm>


SignalFlowTrace::SignalFlowTrace(
    std::optional<SignalFlowData>* signal_flow_data, std::shared_ptr<Cursor> cursor, FlowData::id_t id) :
    signal_flow_data(signal_flow_data),
	id(id),
	cursor(cursor)
{
}

template<class T>
inline uint32_t color_hash(T t, float s = 1.0, float v = 1.0, float alpha = 1.0)
{
	size_t hash = std::hash<T>{}(t);
	float hash_f = (float) hash + std::numeric_limits<size_t>::min();
	hash_f /= (float) std::numeric_limits<size_t>::max() - (float) std::numeric_limits<size_t>::min();

	// std::println("h: {}, s: {}, v: {}")
	float r, g, b;
	ImGui::ColorConvertHSVtoRGB(hash_f, s, v, r, g, b);
	return ImGui::ColorConvertFloat4ToU32({r, g, b, alpha});
}

constexpr auto DELTA_ZONE_SIZE = 0.2;
constexpr auto ZONE_HEIGHT = 2.0;

void SignalFlowTrace::render(int win_id)
{
	using namespace std::literals::chrono_literals;
	bool submit_query = false;
	if(data_future) {
		if (data_future->valid() and data_future->wait_for(0ms) == std::future_status::ready) {
			auto res = data_future->get();
			if(res) {
				data = *res;
			} else {
				submit_query = true;
			}
		}
	} else {
		if(*signal_flow_data) {
			submit_query = true;
		}
	}
	if (submit_query) {
		data_future = std::async(std::launch::async, [&]() {
			return (*signal_flow_data)->query(id);
		});
	}

	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_Once);
	if (ImGui::Begin(
	        std::format("{:08x} signal_trace##{}", id, win_id).c_str(), &open)) {
		if (data){
			auto all_zones = data->zones;

			auto min_time = std::ranges::min(all_zones | std::views::values | std::views::join, {}, &FlowZone::start).start;
			auto max_time = std::ranges::max(all_zones | std::views::values | std::views::join, {}, &FlowZone::end).end;
			auto sz = ImGui::GetContentRegionAvail();
			if (!timeline) {
				timeline = Timeline(min_time, max_time, cursor);
				offset_f = -((float) min_time);
				zoom = sz.x / (max_time - min_time);
				std::println("setting {}, {}", offset_f, zoom);
			}
			auto timeline_start = ImGui::GetCursorScreenPos();
			auto timeline_height = ImGui::GetFontSize() + 20 * scale;
			auto timeline_end = timeline_start + ImVec2(sz.x, timeline_height);

			// std::println("timeline start {}, timeline end {}", timeline_start, timeline_end);

			auto canvas_hovered = Canvas2D(offset_f, zoom, min_time, max_time);
			ImGui::SetCursorScreenPos(timeline_start); // + ImVec2(0, timeline_height));
			ImGui::BeginChild("timeline", timeline_end - timeline_start);
			timeline->render(zoom, offset_f);
			ImGui::EndChild();

			// sort nodes after first time a zone is ever encountered
			auto nodes = all_zones | std::views::keys | std::ranges::to<std::vector>();
			// zones are sorted by start time (and then end time)
			std::ranges::sort(nodes, {}, [&](auto node){ return all_zones[node][0].start; });

			if (ImGui::BeginChild("zones", ImVec2(0, 0), 0, ImGuiWindowFlags_NoScrollWithMouse)) {
				auto draw = ImGui::GetWindowDrawList();
				auto pad = ImVec2(2.0, 2.0);

				for (auto & node : nodes) {
					auto zones = all_zones[node];

					// figure out how many delta zones (zones with zero time duration we need to place)
					// we cannot order these (as they all occur at the same time and we have no information on the actual signal flow)
					// so they all get displayed at the same time, vertically distributed
					//
					// first, reorder the zones after the level:
					// Note: these zones will still be sorted after start time
					// as `zones` is already sorted after start time
					std::map<size_t, std::vector<FlowZone>> level_to_zones;
					for (auto & zone : zones) {
						level_to_zones[zone.module_path.size()].push_back(zone);
					}

					struct DeltaInfo {
						bool delta_before, delta_after;
						int num_delta, delta_pos;
					};

					for (auto& [level, zones] : level_to_zones) {
						// zone a, zone δa, zone δb, zone b
						// -> want info:
						// a:
						// - delta before : false
						// - delta after : true
						// b:
						// - delta before : true
						// - delta after : false
						// δa:
						// - num delta: 2
						// - delta pos : 0
						// - delta before : false
						// - delta after : false
						// δb:
						// - num delta: 2
						// - delta pos : 1
						// - delta before : false
						// - delta after : false
						auto delta_infos = std::vector<DeltaInfo>();
						auto max_delta = 0;

						for (size_t i = 0; i < zones.size(); i++) {
							auto zone = zones[i];

							int num_delta = 0, delta_pos = 0;
							bool delta_before = false;
							bool delta_after = false;
							if (zone.is_delta()) {
								int j = i;
								while ((j >= 0) and zones[j].is_delta() and (zones[j].start == zone.start)) j--;
								j += 1;
								delta_pos = i - j;

								// eh
								while (((j + num_delta) < (int) zones.size()) and zones[j + num_delta].is_delta() and (zones[j + num_delta].start == zone.start))  num_delta++;

								max_delta = max(num_delta, max_delta);
							} else {
								int j = i + 1;
								// this is sorted after (start, reverse zone size)
								// so if there are any delta zones "before" me, they
								// will be after this
								while (j < (int) zones.size() and (zones[j].start == zone.start)) {
									delta_before = zones[j++].is_delta();
								}

								while (j < (int) zones.size() and (zones[j].start <= zone.end)) {
									if (zones[j].end == zone.end) {
										delta_after = zones[j].is_delta();
									}

									j++;
								}
							}

							delta_infos.emplace_back(delta_before, delta_after, num_delta, delta_pos);
						}


						auto base = ImGui::GetCursorScreenPos();

						const float zone_height = max(max_delta, ZONE_HEIGHT);

						for (const auto & [delta_info, zone]: std::views::zip(delta_infos, zones)) {

							// start + ImVec2(0, zone.module_path.size() * 2.0 *ImGui::GetTextLineHeight());

							auto delta_start_offset = DELTA_ZONE_SIZE * delta_info.delta_before;
							auto delta_end_offset = DELTA_ZONE_SIZE * delta_info.delta_after;

							auto zone_start_t = zone.start + delta_start_offset;
							auto zone_end_t = zone.end - delta_end_offset;
							float zone_start_y = 0;
							float zone_end_y = zone_height;

							if (zone.is_delta()) {
								zone_start_t = zone.start - DELTA_ZONE_SIZE;
								zone_end_t = zone.end + DELTA_ZONE_SIZE;

								zone_start_y = (float) (delta_info.delta_pos) * zone_height / (float) delta_info.num_delta;
								zone_end_y = (float) (delta_info.delta_pos + 1) * zone_height / (float) delta_info.num_delta;
							}

							auto zone_start = base + ImVec2((zone_start_t + offset_f) * zoom, zone_start_y * ImGui::GetTextLineHeight());
							auto zone_end = base + ImVec2((zone_end_t + offset_f) * zoom, zone_end_y * ImGui::GetTextLineHeight());

							auto path_name = std::format("{}", zone.module_path);
							auto color_background = color_hash(path_name, 0.8, 1.0, 1.0);
							auto color_border = color_hash(path_name, 1.0, 1.0, 1.0);

							// std::println("base: {}, offset: {}, zoom: {}", base, offset_f, zoom);
							// std::println("zone: {}, delta before {} after {} pos {} num {}", zone, delta_info.delta_before, delta_info.delta_after, delta_info.delta_pos, delta_info.num_delta);

							draw->AddRectFilled(zone_start + pad, zone_end - pad, color_background, 5.0f);
							draw->AddRect(zone_start + pad, zone_end - pad, color_border, 5.0f, 0, 4.0f);

							auto name = std::format("[{},{}]", node->x, node->y);
							if (zone.module_path.size() > 0) {
								name = zone.module_path.back();
							}
							auto sz = ImGui::CalcTextSize(name.c_str());

							if (sz.x < (zone_end.x - zone_start.x)) {
								draw->AddText(nullptr, 24.0f * scale, (zone_end + zone_start) / 2.0 - sz / 2.0, text_color(), name.c_str());
							}

							ImGui::SetCursorScreenPos(zone_start);
							ImGui::Dummy(zone_end - zone_start);
							if (ImGui::BeginItemTooltip()) {
								ImGui::Text("%s", name.c_str());
								ImGui::Text("start: %d, end: %d, dur: %d", zone.start, zone.end, zone.end - zone.start);
								ImGui::Text("%s", std::format("module: {}", zone.module_path).c_str());
								ImGui::Text("%s", std::format("in: {}", zone.interface_in).c_str());
								ImGui::Text("%s", std::format("out: {}", zone.interface_out).c_str());
								ImGui::EndTooltip();
							}
							if (ImGui::IsItemHovered()) {
								draw->AddRect(zone_start + pad, zone_end - pad, 0xffffffff, 5.0f, 0, 4.0f);
							}

							// ImGui::NewLine();
							// ImGui::Text(std::format("zone {}", zone).c_str());
						}

						ImGui::SetCursorScreenPos(ImVec2(base.x, ImGui::GetCursorScreenPos().y));
					}
				}
			}

			ImGui::SetCursorScreenPos(timeline_start + ImVec2(0, timeline_height));
			cursor->render(offset_f, zoom, canvas_hovered and ImGui::IsMouseDown(ImGuiMouseButton_Left));

			ImGui::EndChild();
		}
	}
	ImGui::End();
}

SignalFlowTraces::SignalFlowTraces(
    std::shared_ptr<FstFile> fst_file, std::shared_ptr<Cursor> cursor) :
	fst_file(fst_file),
	cursor(cursor),
    signal_flow_data(std::nullopt)
{
}

void SignalFlowTraces::render()
{
	for (auto& [signal_flow_trace, id] : signal_flow_traces) {
		signal_flow_trace.render(id);
	}
	std::erase_if(signal_flow_traces,
	        [](auto& trace_id) { return not std::get<0>(trace_id).open; });
}

void SignalFlowTraces::load(std::vector<std::shared_ptr<Node>> nodes) {
	signal_flow_data.emplace(fst_file, nodes);
}
