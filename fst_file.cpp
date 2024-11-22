#include "fst_file.h"
#include "mesh_utils.cpp"
#include <print>

template <class T>
struct ParamsWrap
{
	using Params = T;
};

char* FstFile::get_value_at(handle_t handle, uint64_t time)
{
	return fstReaderGetValueFromHandleAtTime(reader, time, handle, buffer);
}
uint64_t FstFile::max_time()
{
	return fstReaderGetEndTime(reader);
}
uint64_t FstFile::min_time()
{
	return fstReaderGetStartTime(reader);
}
void FstFile::read_changes(
    uint64_t min_time, uint64_t max_time, const std::vector<NodeVar>& vars, value_change_cb_t cb)
{
	fstReaderClrFacProcessMaskAll(reader);
	for (const auto& var : vars) {
		fstReaderSetFacProcessMask(reader, var.handle);
	}
	fstReaderIterBlocksSetNativeDoublesOnCallback(reader, 1);
	fstReaderSetLimitTimeRange(reader, min_time, max_time);
	// fstReaderSetUnlimitedTimeRange(reader);
	fstReaderIterBlocks2(
	    reader, FstFile::value_change_callback, FstFile::value_change_callback2, &cb, nullptr);
}
void FstFile::value_change_callback2(
    void* user_callback_data_pointer,
    uint64_t time,
    handle_t facidx,
    const unsigned char* value,
    uint32_t len)
{
	auto cb = (value_change_cb_t*) user_callback_data_pointer;
	if (cb) {
		(*cb)(time, facidx, value);
	}
}
void FstFile::value_change_callback(
    void* user_callback_data_pointer, uint64_t time, handle_t facidx, const unsigned char* value)
{
	auto cb = (value_change_cb_t*) user_callback_data_pointer;
	if (cb) {
		(*cb)(time, facidx, value);
	}
}
std::vector<std::shared_ptr<Node>> FstFile::read_nodes()
{
	if (buffer)
		delete[] buffer;
	std::vector<std::shared_ptr<Node>> nodes;

	fstReaderIterateHierRewind(reader);

	fstHier* hier;

	bool next_is_node = false;
	bool in_node_scope = false;
	int depth = 0;

	std::vector<std::string> hierarchy_stack;
	auto node = std::make_shared<Node>(0, 0, NodeData{}, this);
	NodeData* current_node_data;
	int max_bits = 0;
	std::shared_ptr<Formatter> formatter{new HexFormatter{}};

	while ((hier = fstReaderIterateHier(reader))) {
		switch (hier->htyp) {
			case FST_HT_SCOPE: {
				if (in_node_scope) {
					depth++;

					auto scope = hier->u.scope;
					hierarchy_stack.push_back({scope.name});
					current_node_data =
					    &current_node_data->subscopes.try_emplace({scope.name}, NodeData{})
					         .first->second;
				}
				if (next_is_node) {
					next_is_node = false;
					in_node_scope = true;
					depth = 1;
				}

				break;
			}
			case FST_HT_UPSCOPE: {
				if (in_node_scope) {
					depth--;
					if (depth == 0) {
						nodes.push_back(node);
						in_node_scope = false;
					} else {
						hierarchy_stack.pop_back();
						current_node_data = &node->data;
						for (const auto& name : hierarchy_stack) {
							current_node_data = &current_node_data->subscopes.at({name});
						}
					}
				}
				break;
			}
			case FST_HT_VAR: {
				if (in_node_scope) {
					auto var = hier->u.var;
					// std::println("adding var {} handle {}", var.name, var.handle);
					if (var.length == 1) {
						formatter.reset(new BinaryFormatter{});
					}
					current_node_data->variables.insert(
					    {{var.name},
					     {var.name, var.length, var.handle, node, std::move(formatter)}});
					formatter.reset(new HexFormatter{});
					max_bits = max_bits > var.length ? max_bits : var.length;
				}
				// std::println("[var]: name: {}", var.name);
				break;
			}
			case FST_HT_ATTRBEGIN: {
				auto attr = hier->u.attr;
				if (attr.typ == FST_AT_MISC and attr.subtype == FST_MT_COMMENT) {
					auto parsed = parse_attr<
					    ParamsWrap<TraceFPGABandwidthParams>,
					    ParamsWrap<PoissonEventTrafficParams>>(attr.name);
					auto nodeattr = std::get_if<NodeAttr>(&parsed);
					if (nodeattr) {
						assert(not in_node_scope);
						next_is_node = true;
						// TODO(robin): ownership of this?
						node = std::make_shared<Node>(nodeattr->x, nodeattr->y, NodeData{}, this);
						current_node_data = &node->data;
					}
					auto signalattr = std::get_if<SignalAttr>(&parsed);
					if (signalattr) {
						if (signalattr->name == "formatter") {
							formatter.reset(new FixedFormatter{parse_formatter(signalattr->value)});
						}
					}
					// std::visit([](auto v) {
					//     std::println("[attr]: {}", v);
					// }, );
				}
				break;
			}
			case FST_HT_ATTREND: {
				std::println("[attr]: end");
				break;
			}
		}
	}

	buffer = new char[max_bits];
	return nodes;
}
FstFile::FstFile(const char* path) : reader(fstReaderOpen(path)), buffer(nullptr) {}
FstFile::~FstFile()
{
	fstReaderClose(reader);
	if (buffer) {
		delete[] buffer;
	}
}

bool all_zero(const char* string)
{
	while (*string) {
		if ((string++)[0] != '0')
			return false;
	}
	return true;
}

WaveDatabase FstFile::read_wave_db(NodeVar var)
{
	std::vector<WaveValue> values;
	read_changes(
	    min_time(), max_time(), {var}, [&](uint64_t time, handle_t, const unsigned char* value) {
		    values.push_back(WaveValue{
		        time,
		        all_zero((const char*) value) ? WaveValueType::Zero : WaveValueType::NonZero});
	    });
	return WaveDatabase(values);
}
