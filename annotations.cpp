#include "annotations.h"

constexpr const char* ANNOTATION_TEXT = "annotation_text";
constexpr const char* ANNOTATION_CLK = "clk";
constexpr const char* ANNOTATION_EVENT = "event_annotation";
constexpr const char* ANNOTATION_SPAN = "span_annotation";

AnnotationsData::AnnotationsData(
    std::shared_ptr<FstFile> fst_file, std::vector<std::shared_ptr<Node>> nodes) :
    data_future{std::async(std::launch::async, [=] {
	    FstFile local_fstfile(*fst_file);

	    std::map<ModulePath, AnnotationData> annotation_data;

	    for (auto node : nodes) {
		    auto clk = node->data.variables.at(SAMPLE_CLK);
		    for (auto [name, var] : node->data.variables) {
			    if (var.attrs.contains(ANNOTATION_TEXT)) {
				    std::println("reading annotation: {}", var.pretty_name());
				    // sample negedge, because that is where design inputs from the simulation are
				    // changed

					std::vector<WaveValue> values;
					fast_reader.read_values(var.handle - 1,
											[&](uint32_t time, const unsigned char* value, uint16_t bytes) {
											values.push_back(WaveValue{
		        static_cast<uint32_t>(time),
		        all_zero(value, bytes) ? WaveValueType::Zero : WaveValueType::NonZero});
	    });
	return WaveDatabase(values);

	//
				    auto [times, data] = local_fstfile.read_values<uint32_t>(
				        var, clk, {}, {}, true /* negedge */);

				    auto zipped = std::views::zip(data, times);
					auto values =
				    auto map =
				        std::unordered_multimap<id_t, simtime_t>{zipped.begin(), zipped.end()};

				    flow_data.push_back(FlowData{map, {node, var}});
			    }
		    }
	    }
	    return flow_data;
    })}
{
}
