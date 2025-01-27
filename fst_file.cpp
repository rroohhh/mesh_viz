#include "fst_file.h"
#include "mesh_utils.h"
#include "node.h"
#include "node_var.h"

#include <print>
#include <execution>
#include <algorithm>

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

std::vector<std::shared_ptr<Node>> FstFile::read_nodes(WaveformViewer * waveform_viewer, Histograms * histograms, AsyncRunner * async_runner)
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
	auto node = std::make_shared<Node>(0, 0, NodeData{}, shared_from_this(), node_role, system_config, waveform_viewer, histograms, async_runner);
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
					// TODO(robin): add formatter for single bit enums
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
					using SystemConfigT = decltype(Node::system_config);
					auto parsed = parse_attr<
					    ParamsWrap<decltype(SystemConfigT::node_params)>,
					    ParamsWrap<decltype(SystemConfigT::event_params)>,
					    ParamsWrap<decltype(SystemConfigT::error_params)>
						>(attr.name);
					auto nodeattr = std::get_if<NodeAttr>(&parsed);
					if (nodeattr) {
						assert(not in_node_scope);
						next_is_node = true;
						node = std::make_shared<Node>(nodeattr->x, nodeattr->y, NodeData{}, shared_from_this(), node_role, system_config, waveform_viewer, histograms, async_runner);
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

FstFile::FstFile(const char* path) : reader(fstReaderOpen(path)), filename(path), cache(25), fast_reader(path) {}

FstFile::FstFile(const FstFile& other) :
    reader(fstReaderOpen(other.filename.c_str())),
    filename(other.filename),
    value_buffer(other.value_buffer),
	cache(5),
	fast_reader(other.fast_reader)
{
	// std::println("copied");
}

FstFile::~FstFile()
{
	fstReaderClose(reader);
}

bool all_zero(const byte_t* vals, uint16_t bytes)
{
	for (uint16_t i = 0; i < bytes; i++) {
		if (vals[i] != 0) {
			return false;
		}
	}
	return true;
}

WaveDatabase FstFile::read_wave_db(NodeVar var) const
{
	std::vector<WaveValue> values;
	fast_reader.read_values(var.handle - 1,
	    [&](uint32_t time, const unsigned char* value, uint16_t bytes) {
		    values.push_back(WaveValue{
		        static_cast<uint32_t>(time),
		        all_zero(value, bytes) ? WaveValueType::Zero : WaveValueType::NonZero});
	    });
	return WaveDatabase(values);
}

// template<typename T>
// void from_chars(const char * start, const char * end, T & result) {
// 	std::from_chars(start, end, result, 2);
// }

// NOTE(robin): untested for idx > 1
template<class T, uint16_t idx>
T from_bytes(const byte_t* data) {
	if constexpr(idx == 1) {
		return *data;
	} else {
		return ((data[0]) << (8 * (idx - 1))) | from_bytes<T, idx - 1>(data + 1);
	}
}

// TODO(robin): do caching? or multithreading?
template <class T, class O>
O FstFile::read_values(const NodeVar & var) const
{
	auto bytes = (var.nbits + 7) / 8;
	switch (bytes) {
		case 1:
			return read_values_inner<T, O, 1>(var);
		case 2:
			return read_values_inner<T, O, 2>(var);
		case 3:
			return read_values_inner<T, O, 3>(var);
		case 4:
			return read_values_inner<T, O, 4>(var);
		case 5:
			return read_values_inner<T, O, 5>(var);
		case 6:
			return read_values_inner<T, O, 6>(var);
		case 7:
			return read_values_inner<T, O, 7>(var);
		case 8:
			return read_values_inner<T, O, 8>(var);
		default:
			return read_values_inner<T, O>(var);
	}
}

// TODO(robin): do caching? or multithreading?
template <class T, class O, int nbytes>
O FstFile::read_values_inner(const NodeVar & var) const
{
	if constexpr (std::is_same<T, bit_type_t>()) {
		auto cached = cache.get(var.handle);
		if (cached) {
			// std::println("cache hit");
			return *cached;
		}
	}

	O values(max_time() - min_time() + 1);
	// std::println("values.size(): {}", values.size());
	// values.reserve();
	int64_t last_time = -1;
	auto shift = (8 - (var.nbits % 8)) % 8;
	fast_reader.read_values(
	    var.handle - 1, [&](uint32_t time, const byte_t* data, uint16_t bytes) {
		    T v{0};
			if constexpr(nbytes == 0) {
				for (int i = 0; i < bytes; i++) {
					v <<= 8;
					v |= (*data++);
				}
				v = v >> shift;
			} else {
				v = from_bytes<T, nbytes>(data) >> shift;
			}
		    if (last_time == -1) {
			    values[time] = v;
		    } else {
			    // NOTE(robin): assumes no glitches


				std::fill(std::execution::unseq, std::begin(values) + last_time + 1, std::begin(values) + time, values[last_time]);

			    // values.insert(values.end(), time - last_time - 1, values.back());

				values[time] = v;
			    // values.push_back(v);
		    }
		    last_time = time;
	    });

	if (last_time < max_time()) {
		std::fill(std::execution::unseq, std::begin(values) + last_time + 1, std::begin(values) + max_time(), values[last_time]);
		// values.insert(values.end(), max_time() - last_time - 1, values.back());
	}

	if constexpr (std::is_same<T, bit_type_t>()) {
		cache.add(var.handle, values);
	}

	return std::move(values);
}

template <class T>
std::pair<std::vector<simtime_t>, std::vector<T>> FstFile::read_values(const NodeVar& var, const NodeVar& sampling_var, std::vector<NodeVar> conditions, std::vector<NodeVar> masks, bool negedge) const
{
	auto var_data = read_values<T, std::valarray<T>>(var);
	auto clk = read_values<bit_type_t, std::valarray<bit_type_t>>(sampling_var);

	// std::vector<std::vector<bit_type_t>> condition_data(conditions.size());
	// std::vector<std::vector<bit_type_t>> mask_data(masks.size());

	std::valarray<bit_type_t> cond(true, var_data.size());
	std::valarray<bit_type_t> mask(false, var_data.size());

	for (size_t i = 0; i < conditions.size(); i++) {
		cond *= read_values<bit_type_t, std::valarray<bit_type_t>>(conditions[i]);
	}
	for (size_t i = 0; i < masks.size(); i++) {
		mask |= read_values<bit_type_t, std::valarray<bit_type_t>>(masks[i]);
	}
	cond *= (1 - mask);

	auto num_entries = var_data.size();
	// std::valarray<bit_type_t> tmp2(false, var_data.size());

	// std::copy(std::execution::unseq, sampling_var_data.begin(), sampling_var_data.end(), std::begin(tmp));
	// std::copy(std::execution::unseq, sampling_var_data.begin(), sampling_var_data.end(), std::begin(tmp2));

	clk[std::slice(1, num_entries - 1, 1)] &= (1 - clk)[std::slice(0, num_entries - 1, 1)];

	if (negedge) {
		cond &= 1 - clk;
	} else {
		cond &= clk;
	}
	cond[0] = 0;

	uint64_t count = std::reduce(std::execution::unseq, std::begin(cond), std::end(cond), 0UL);

	std::vector<simtime_t> times(count);
	std::vector<T> values(count);
	auto idx = 0;
	for (simtime_t time = 1; time < var_data.size(); time++) {
		if (cond[time]) {
			times[idx] = time;
			values[idx] = var_data[time];
			idx += 1;
		}
	}

	return std::make_pair(times, values);
}

template std::vector<uint32_t> FstFile::read_values<uint32_t>(const NodeVar & var) const;
template std::valarray<uint32_t> FstFile::read_values<uint32_t>(const NodeVar & var) const;
template std::valarray<FstFile::bit_type_t> FstFile::read_values<FstFile::bit_type_t>(const NodeVar & var) const;

// template std::vector<bool> FstFile::read_values(const NodeVar & var) const;
// template std::vector<bool> FstFile::read_values<bool, 1>(const NodeVar & var) const;
template std::pair<std::vector<simtime_t>, std::vector<uint32_t>> FstFile::read_values(const NodeVar& var, const NodeVar& sampling_var, std::vector<NodeVar> conditions, std::vector<NodeVar> masks, bool negedge) const;
//
