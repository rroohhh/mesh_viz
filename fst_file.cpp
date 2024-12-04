#include "fst_file.h"
#include "mesh_utils.h"
#include "node.h"
#include <print>

char* FstFile::get_value_at(const NodeVar & var, uint64_t time) const
{
	// TODO(robin): very crude check for unitited buffer
	if (value_buffer.size() == 0) {
		return nullptr;
	} else {
		return fstReaderGetValueFromHandleAtTime(reader, time, var.handle, value_buffer.data());
	}
}

uint64_t FstFile::max_time() const
{
	return fstReaderGetEndTime(reader);
}

uint64_t FstFile::min_time() const
{
	return fstReaderGetStartTime(reader);
}

void FstFile::read_changes(
    uint64_t min_time,
    uint64_t max_time,
    const std::vector<NodeVar>& vars,
    value_change_cb_t cb) const
{
	fstReaderClrFacProcessMaskAll(reader);
	for (const auto& var : vars) {
		fstReaderSetFacProcessMask(reader, var.handle);
	}
	fstReaderIterBlocksSetNativeDoublesOnCallback(reader, 1);
	fstReaderSetLimitTimeRange(reader, min_time, max_time);
	fstReaderIterBlocks2(
	    reader, FstFile::value_change_callback, FstFile::value_change_callback2, &cb, nullptr);
}

void FstFile::value_change_callback2(
    void* user_callback_data_pointer,
    uint64_t time,
    handle_t facidx,
    const unsigned char* value,
    uint32_t /*len*/) // TODO(robin): use this len information
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
	std::vector<std::shared_ptr<Node>> nodes;

	fstReaderIterateHierRewind(reader);

	fstHier* hier;

	bool next_is_node = false;
	bool in_node_scope = false;
	int depth = 0;

	std::vector<std::string> hierarchy_stack;
	decltype(Node::role) node_role;
	decltype(Node::system_config) system_config;
	auto node = std::make_shared<Node>(0, 0, NodeData{}, this, node_role, system_config);
	NodeData* current_node_data;
	uint32_t max_bits = 0;
	std::shared_ptr<Formatter> formatter{new HexFormatter{}};
	decltype(NodeVar::attrs) var_attrs;

	while ((hier = fstReaderIterateHier(reader))) {
		// std::println("hier {}", (void *) hier);
		switch (hier->htyp) {
			case FST_HT_SCOPE: {
				// std::println("hier: {}", hier->u.scope.name);
				if (in_node_scope) {
					depth++;

					auto scope = hier->u.scope;
					hierarchy_stack.push_back({scope.name});
					current_node_data =
					    &current_node_data->subscopes.try_emplace({scope.name}).first->second;
				}
				if (next_is_node) {
					next_is_node = false;
					node->role = node_role;
					node->system_config = system_config;
					in_node_scope = true;
					depth = 1;
				}

				break;
			}
			case FST_HT_UPSCOPE: {
				// std::println("hier pop");
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
					     {var.name, var.length, var.handle, node, std::move(formatter),
					      var_attrs}});
					formatter.reset(new HexFormatter{});
					max_bits = max_bits > var.length ? max_bits : var.length;
				}
				// std::println("[var]: name: {}", hier->u.var.name);
				break;
			}
			case FST_HT_ATTRBEGIN: {
				auto attr = hier->u.attr;
				if (attr.typ == FST_AT_MISC and attr.subtype == FST_MT_COMMENT) {
					// std::println("found comment: {}", attr.name);
					auto parsed = parse_attr<
					    ParamsWrap<TraceFPGABandwidthParams>,
					    ParamsWrap<PoissonEventTrafficParams>>(attr.name);
					auto nodeattr = std::get_if<NodeAttr>(&parsed);
					if (nodeattr) {
						assert(not in_node_scope);
						next_is_node = true;
						// TODO(robin): ownership of this?
						node = std::make_shared<Node>(nodeattr->x, nodeattr->y, NodeData{}, this, node_role, system_config);
						current_node_data = &node->data;
					}
					auto signalattr = std::get_if<SignalAttr>(&parsed);
					if (signalattr) {
						if (signalattr->name == "formatter") {
							formatter.reset(new FixedFormatter{
							    parse_formatter(std::get<std::string>(signalattr->value))});
						}
						SignalAttr attr = *signalattr;
						var_attrs.insert({attr.name, attr.value});
					}
					auto system_attr = std::get_if<decltype(Node::system_config)>(&parsed);
					if (system_attr) {
						system_config = *system_attr;
					}
					auto node_role_attr = std::get_if<decltype(Node::role)>(&parsed);
					if (node_role_attr) {
						// std::println("parsed node role");
						node_role = *node_role_attr;
					}
					// std::visit([](auto v) {
					//     std::println("[attr]: {}", v);
					// }, );
				}
				break;
			}
			case FST_HT_ATTREND: {
				// std::println("[attr]: end");
				break;
			}
		}
	}


	// values .insert(values.end(), time - last_time - 1, values.last());
	value_buffer.assign(max_bits + 1, 0);
	return nodes;
}

FstFile::FstFile(const char* path) : reader(fstReaderOpen(path)), filename(path) {}

FstFile::FstFile(const FstFile& other) :
    reader(fstReaderOpen(other.filename.c_str())),
    filename(other.filename),
    value_buffer(other.value_buffer)
{
	// std::println("copied");
}

FstFile::~FstFile()
{
	fstReaderClose(reader);
}

bool all_zero(const char* string)
{
	while (*string) {
		if ((string++)[0] != '0')
			return false;
	}
	return true;
}

WaveDatabase FstFile::read_wave_db(NodeVar var) const
{
	std::vector<WaveValue> values;
	read_changes(
	    min_time(), max_time(), {var}, [&](uint64_t time, handle_t, const unsigned char* value) {
		    values.push_back(WaveValue{
		        static_cast<uint32_t>(time),
		        all_zero((const char*) value) ? WaveValueType::Zero : WaveValueType::NonZero});
	    });
	return WaveDatabase(values);
}


template <class T>
std::vector<T> FstFile::read_values(const NodeVar & var) const
{
	std::vector<T> values;
	values.reserve(max_time() - min_time() + 1);
	uint64_t last_time = 0;
	read_changes(
	    min_time(), max_time(), {var}, [&](uint64_t time, handle_t, const unsigned char* value) {
		    auto v = std::stoull((const char*) value, nullptr, 2);
		    if (values.size() == 0) {
			    values.push_back(v);
		    } else {
			    // NOTE(robin): assumes no glitches
			    // std::println("filling: {}, time")
			    values.insert(values.end(), time - last_time - 1, values.back());
			    values.push_back(v);
		    }
		    last_time = time;
	    });
	if (last_time < max_time()) {
		values.insert(values.end(), max_time() - last_time - 1, values.back());
	}
	return values;
}

// TODO(robin): this can be optimized a lot, but we probably dont care
template <class T>
std::pair<std::vector<simtime_t>, std::vector<T>> FstFile::read_values(const NodeVar& var, const NodeVar& sampling_var, std::vector<NodeVar> conditions, std::vector<NodeVar> masks, bool negedge) const
{
	auto var_data = read_values<T>(var);
	auto sampling_var_data = read_values<T>(sampling_var);
	std::vector<std::vector<T>> condition_data;
	std::vector<std::vector<T>> mask_data;
	for (auto & var : conditions) {
		condition_data.push_back(read_values<T>(var));
	}
	for (auto & var : masks) {
		mask_data.push_back(read_values<T>(var));
	}

	std::vector<simtime_t> times;
	std::vector<uint64_t> values;

	for (simtime_t time = 1; time < var_data.size(); time++) {
		// trigger on rising edge
		if ((not negedge and (sampling_var_data[time - 1] == 0) and (sampling_var_data[time] != 0))
			or (negedge and (sampling_var_data[time - 1] != 0) and (sampling_var_data[time] == 0)) ){
			auto masked	= false;
			for (auto & mask : mask_data) {
				masked |= mask[time] != 0;
			}
			auto cond	= true;
			for (auto & conds : condition_data) {
				cond &= conds[time] != 0;
			}

			if (not masked and cond) {
				times.push_back(time);
				values.push_back(var_data[time]);
			}
		}
	}

	return {times, values};
}

template std::vector<uint64_t> FstFile::read_values(const NodeVar & var) const;
template std::pair<std::vector<simtime_t>, std::vector<uint64_t>> FstFile::read_values(const NodeVar& var, const NodeVar& sampling_var, std::vector<NodeVar> conditions, std::vector<NodeVar> masks, bool negedge) const;
