#include "histogram.h"
#include "fst_file.h"
#include "highlights.h"

#include "imgui.h"
#include "implot.h"
#include "node.h"
#include "node_var.h"
#include "wave_data_base.h"
#include "utils.cpp"

#include <memory>
#include <print>

void Histograms::render()
{
	for (auto& [hist, id] : histograms) {
		hist.render(id);
	}
	std::erase_if(histograms,
	        [](auto& hist_id) { return not std::get<0>(hist_id).open; });
}

Histogram::Histogram(Highlights* highlights, FstFile* fstfile, const NodeVar& var, const NodeVar& sampling_var, std::vector<NodeVar> conditions, std::vector<NodeVar> masks, bool negedge) :
    highlights(highlights), var(var), sampling_var(sampling_var), conditions(conditions), masks(masks), data_future{std::async(std::launch::async, [=] {
	    FstFile my_fstfile(*fstfile);
	    auto [times, data] = my_fstfile.read_values<uint64_t>(var, sampling_var, conditions, masks, negedge);
	    return DataT{data, times};
    })}
{
}

Histogram::Histogram(Highlights* highlights, FstFile*, std::string name, std::vector<NodeVar> used, std::span<const DataT::simtime_t> times, std::span<const DataT::value_t> values) :
    highlights(highlights), extra(used), extra_name(name), data_future{resolved_future(DataT{values, times})}
{
}

bool Histogram::HighlightCheckbox(const NodeVar & var, const char * category, bool default_open) {
	auto label = var.pretty_name();
	auto stable_id = var.stable_id();
	auto [it, _] = to_highlight.try_emplace(stable_id, default_open);
	auto open = &it->second;
	ImGui::PushID(category);
	ImGui::Indent();
	auto old = *open;
	ImGui::MenuItem(label.c_str(), NULL, open);
	bool changed = (old != *open) and highlighted;
	ImGui::Unindent();
	ImGui::PopID();
	return changed;
}

bool Histogram::render(int id)
{
	using namespace std::literals::chrono_literals;
	if (data_future.valid() and data_future.wait_for(0ms) == std::future_status::ready) {
		data = data_future.get();
		// we highlight the var by default
		if (var) {
			to_highlight.emplace(var->stable_id(), true);
		} else {
			for (auto & e : extra) {
				to_highlight.emplace(e.stable_id(), true);
			}
		}
	}
	if (ImGui::Begin(std::format("{} histogram##{}", var ? var->pretty_name() : extra_name, id).c_str(), &open, ImGuiWindowFlags_MenuBar)) {
		if (var) {
			var->owner_node->highlight |= ImGui::IsWindowHovered();
		} else {
			for (auto & v : extra) {
				v.owner_node->highlight |= ImGui::IsWindowHovered();
			}
		}

		bool should_update_highlights = false;
		if(ImGui::BeginMenuBar()) {
			ImGui::ColorEdit4("highlight color picker", &color.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
			if(highlighted) {
				(*highlighted)->color = ImGui::ColorConvertFloat4ToU32(color);
			}

			if (ImGui::BeginMenu("highlight")) {
				if (var) {
					ImGui::Text("variable");
					should_update_highlights |= HighlightCheckbox(*var, "var");
				}

				if (sampling_var) {
					ImGui::Text("clock");
					should_update_highlights |= HighlightCheckbox(*sampling_var, "clock");
				}

				if (conditions.size() > 0) {
					ImGui::Text("conditions");
					for (auto & cond : conditions) {
						should_update_highlights |= HighlightCheckbox(cond, "cond");
					}
				}

				if (masks.size() > 0) {
					ImGui::Text("masks");
					for (auto & mask : masks) {
						should_update_highlights |= HighlightCheckbox(mask, "mask");
					}
				}

				if (extra.size() > 0) {
					ImGui::Text("extra");
					for (auto & e : extra) {
						should_update_highlights |= HighlightCheckbox(e, "extra");
					}
				}
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}


		if (ImPlot::BeginPlot("histogram", ImVec2(-1, -1))) {
			ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
			auto width = 0.9f;
			auto half_width = width / 2;
			if (data) {
				ImPlot::PlotBarsG(
				    "histogram",
				    [](int idx, void* inverted_idx_p) {
					    // NOTE(robin): this is O(N²)
					    auto inverted_idx = (DataT*) inverted_idx_p;
                        auto it = inverted_idx->posting_list.find(inverted_idx->keys[idx]);
					    // auto it = std::next(inverted_idx->posting_list.begin(), idx);
					    return ImPlotPoint{(double) it->first, (double) it->second.size()};
				    },
				    &*data, data->posting_list.size(), 0.9f);

				if (query) {
					if (ImPlot::DragRect(
					        0, &query->X.Min, &query->Y.Min, &query->X.Max, &query->Y.Max,
					        ImVec4(1, 0, 1, 1))) {
						update_query();
					}
				}

				if (ImPlot::IsPlotHovered()) {
					ImDrawList* draw_list = ImPlot::GetPlotDrawList();
					ImPlotPoint mouse = ImPlot::GetPlotMousePos();
					mouse.x = round(mouse.x);
					float tool_l = ImPlot::PlotToPixels(mouse.x - half_width, mouse.y).x;
					float tool_r = ImPlot::PlotToPixels(mouse.x + half_width, mouse.y).x;
					float tool_t = ImPlot::GetPlotPos().y;
					float tool_b = tool_t + ImPlot::GetPlotSize().y;
					ImPlot::PushPlotClipRect();
					draw_list->AddRectFilled(
					    ImVec2(tool_l, tool_t), ImVec2(tool_r, tool_b),
					    IM_COL32(128, 128, 128, 64));
					ImPlot::PopPlotClipRect();
					auto it = data->posting_list.find(mouse.x);
					if (it != data->posting_list.end()) {
						ImGui::BeginTooltip();
						ImGui::Text("Value: %lu", it->first);
						ImGui::Text("Count: %u", it->second.size());
						ImGui::EndTooltip();
					}

					if (ImPlot::IsPlotSelected()) {
						auto select = ImPlot::GetPlotSelection();
						if (ImGui::IsMouseClicked(ImPlot::GetInputMap().SelectCancel)) {
							ImPlot::CancelPlotSelection();
							set_query(select);
						}
					}

					if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
						set_query(std::nullopt);
					}
				}
			}
			ImPlot::EndPlot();
		}
		if (should_update_highlights) {
			update_highlights();
		}
	}
	ImGui::End();
	return open;
}

void Histogram::set_query(decltype(query) new_value)
{
	query = new_value;
	highlighted = std::make_shared<HighlightEntries>();
	if (query) {
		update_query();
		update_highlights();
	}
}

Histograms::Histograms(FstFile* fstfile, Highlights* highlights) :
    fstfile(fstfile), highlights(highlights)
{
}

void Histogram::update_query()
{
	std::vector<WaveValue> values;

	(*highlighted)->dbs.clear();
	for (int i = round(query->X.Min); i <= round(query->X.Max); i++) {
		auto it = data->posting_list.find(i);
		if (it != data->posting_list.end()) {
			auto & pl = it->second;
			if(pl.size() < 100) {
				for (size_t idx = 0; idx < pl.size(); idx++) {
					values.push_back(pl.get(idx));
				}
			} else {
				(*highlighted)->dbs.push_back(&pl);
			}
		}
	}
	if (values.size() > 0) {
		std::println("doing small wdb opt");
		std::sort(values.begin(), values.end());
		small_wdb_opt.emplace(values);
		(*highlighted)->dbs.push_back(&*small_wdb_opt);
	}
}

void Histogram::update_highlights()
{
	for (auto & [handle, should_highlight] : to_highlight) {
		highlights->remove(handle, *highlighted);
		if (should_highlight) {
			highlights->add(handle, *highlighted);
		}
	}
}
