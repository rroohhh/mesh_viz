#pragma once

#include "cursor.h"
#include "signal_flow_data.h"
#include "timeline.h"
#include <vector>
#include <optional>

struct Node;
struct FstFile;

class SignalFlowTrace {
	std::optional<FlowZones > data;
	std::optional<Timeline> timeline;
	std::optional<std::future<std::optional<FlowZones>>> data_future;

	std::optional<SignalFlowData> * signal_flow_data;
	FlowData::id_t id;

	bool open = true;

	double offset_f = 0.0, zoom = 10.0;

	std::shared_ptr<Cursor> cursor;

	friend class SignalFlowTraces; // to access open
public:
	SignalFlowTrace(std::optional<SignalFlowData> * signal_flow_data, std::shared_ptr<Cursor>, FlowData::id_t id);

	void render(int win_id);
};

class SignalFlowTraces
{
	std::vector<std::pair<SignalFlowTrace, int>> signal_flow_traces;
	int id_gen = 0;

	std::shared_ptr<FstFile> fst_file;

	std::shared_ptr<Cursor> cursor;
	std::optional<SignalFlowData> signal_flow_data;
public:
	SignalFlowTraces(std::shared_ptr<FstFile> fst_file, std::shared_ptr<Cursor>);

	void load(std::vector<std::shared_ptr<Node>> nodes);

	template<class... Args>
	void add(Args && ...args) {
		std::println("add trace");
		signal_flow_traces.emplace_back(std::piecewise_construct, std::forward_as_tuple(&signal_flow_data, cursor, std::forward<Args>(args)...), std::forward_as_tuple(id_gen++));
	}

    void render();
};
