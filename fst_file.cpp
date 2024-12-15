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
	auto node = std::make_shared<Node>(0, 0, NodeData{}, this, node_role, system_config, waveform_viewer, histograms, async_runner);
	NodeData* current_node_data;
	uint32_t max_bits = 0;
	std::shared_ptr<Formatter> formatter{new HexFormatter{}};
	decltype(NodeVar::attrs) var_attrs;

	while ((hier = fstReaderIterateHier(reader))) {
		// std::println("hier {}", (void *) hier);
		switch (hier->htyp) {
			case FST_HT_SCOPE: {
				std::println("hier: {}", hier->u.scope.name);
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
					std::println("adding var {} handle {}", var.name, var.handle);
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
					auto parsed = parse_attr<
					    ParamsWrap<TraceFPGABandwidthParams>,
					    ParamsWrap<PoissonEventTrafficParams>>(attr.name);
					auto nodeattr = std::get_if<NodeAttr>(&parsed);
					if (nodeattr) {
						assert(not in_node_scope);
						next_is_node = true;
						node = std::make_shared<Node>(nodeattr->x, nodeattr->y, NodeData{}, this, node_role, system_config, waveform_viewer, histograms, async_runner);
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

// template<>
// void from_chars(const char * start, const char * end, bool & result) {
// 	result = (start[0] == '1');
// }

// TODO(robin): do caching? or multithreading?
template <class T, int nbits>
std::vector<T> FstFile::read_values(const NodeVar & var) const
{
	if constexpr (std::is_same<T, bool>()) {
		auto cached = cache.get(var.handle);
		if (cached) {
			// std::println("cache hit");
			return *cached;
		}
	}

	std::vector<T> values;
	values.reserve(max_time() - min_time() + 1);
	uint64_t last_time = 0;
	fast_reader.read_val(
	    min_time(), max_time(), {var}, [&](uint64_t time, handle_t, const unsigned char* value) {
		    // auto v = std::strtoull((const char*) value, nullptr, 2);
		    T v;
			if constexpr(nbits == 0) {
				from_chars((const char*) value, (const char *) value + var.nbits, v);
			} else {
				from_chars((const char*) value, (const char *) value + nbits, v);
			}
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

	if constexpr (std::is_same<T, bool>()) {
		cache.add(var.handle, values);
	}

	return values;
}


// TODO(robin): this can be optimized a lot, but we probably dont care
template <class T>
std::pair<std::vector<simtime_t>, std::vector<T>> FstFile::read_values(const NodeVar& var, const NodeVar& sampling_var, std::vector<NodeVar> conditions, std::vector<NodeVar> masks, bool negedge) const
{
	using bit_type = bool;
	auto var_data = read_values<T>(var);
	auto sampling_var_data = read_values<bit_type, 1>(sampling_var);
	std::vector<std::vector<bit_type>> condition_data(conditions.size());
	std::vector<std::vector<bit_type>> mask_data(masks.size());
	for (size_t i = 0; i < conditions.size(); i++) {
		condition_data[i] = read_values<bit_type, 1>(conditions[i]);
	}
	for (size_t i = 0; i < masks.size(); i++) {
		mask_data[i] = read_values<bit_type, 1>(masks[i]);
	}

	std::valarray<bit_type> tmp(false, var_data.size());
	std::valarray<bit_type> mask(false, var_data.size());
	for (const auto & m : mask_data) {
		std::copy(std::execution::unseq, m.begin(), m.end(), std::begin(tmp));
		mask |= tmp;
	}

	std::valarray<bit_type> cond(true, var_data.size());
	for (const auto & c : condition_data) {
		std::copy(std::execution::unseq, c.begin(), c.end(), std::begin(tmp));
		cond &= tmp;
	}


	std::vector<simtime_t> times;
	std::vector<uint64_t> values;

	for (simtime_t time = 1; time < var_data.size(); time++) {
		// trigger on rising edge
		if ((not negedge and (sampling_var_data[time - 1] == 0) and (sampling_var_data[time] != 0))
			or (negedge and (sampling_var_data[time - 1] != 0) and (sampling_var_data[time] == 0)) ){

			// auto masked	= false;
			// for (auto & mask : mask_data) {
			// 	masked |= mask[time] != 0;
			// }
			// auto cond	= true;
			// for (auto & conds : condition_data) {
			// 	cond &= conds[time] != 0;
			// }

			// if (not masked and cond) {
			if (not mask[time] and cond[time]) {
				times.push_back(time);
				values.push_back(var_data[time]);
			}
		}
	}

	return {times, values};
}

template std::vector<uint64_t> FstFile::read_values(const NodeVar & var) const;
template std::vector<uint8_t> FstFile::read_values(const NodeVar & var) const;
template std::vector<uint8_t> FstFile::read_values<uint8_t, 1>(const NodeVar & var) const;
template std::vector<bool> FstFile::read_values(const NodeVar & var) const;
template std::vector<bool> FstFile::read_values<bool, 1>(const NodeVar & var) const;
template std::pair<std::vector<simtime_t>, std::vector<uint64_t>> FstFile::read_values(const NodeVar& var, const NodeVar& sampling_var, std::vector<NodeVar> conditions, std::vector<NodeVar> masks, bool negedge) const;
