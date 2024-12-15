#include "async_runner.h"
#include "mesh_utils.h"
#include "node.h"
#include "node_var.h"
#include "histogram.h"
#include "fst_file.h"

#include <print>
#include <pybind11/embed.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;
using namespace py::literals;

// https://github.com/pybind/pybind11/issues/1042#issuecomment-642215028
template <typename Sequence>
inline py::array_t<typename Sequence::value_type> as_pyarray(Sequence&& seq)
{
	auto size = seq.size();
	auto data = seq.data();
	std::unique_ptr<Sequence> seq_ptr = std::make_unique<Sequence>(std::move(seq));
	auto capsule = py::capsule(
	    seq_ptr.get(), [](void* p) { std::unique_ptr<Sequence>(reinterpret_cast<Sequence*>(p)); });
	seq_ptr.release();
	return py::array(size, data, capsule);
}


struct AsyncNode: public std::enable_shared_from_this<AsyncNode>
{
// TODO(robin): this is not safe with enable_shared_from_this
public:
	FstFile ctx;

	AsyncNode(const FstFile & ctx) : ctx(ctx) {};
	AsyncNode(const AsyncNode & other) = delete;
	AsyncNode(const AsyncNode && other) = delete;
};


class StopIteration : public py::stop_iteration {
    public:
	    // NOTE(robin): paaaaain: https://github.com/PyO3/pyo3/pull/4407
        StopIteration(py::object result) : stop_iteration("--"), result(py::make_tuple(std::move(result))) {};

        void set_error() const override {
			std::println("raising {}", result.attr("__repr__")().cast<std::string>());
            PyErr_SetObject(PyExc_StopIteration, this->result.ptr());
        }
    private:
        py::object result;
};

PYBIND11_MAKE_OPAQUE(std::vector<std::shared_ptr<Node>>)
PYBIND11_MAKE_OPAQUE(std::map<std::string, NodeVar>)
PYBIND11_MAKE_OPAQUE(std::map<std::string, NodeData>)
PYBIND11_MAKE_OPAQUE(decltype(NodeVar::attrs))

PYBIND11_EMBEDDED_MODULE(mesh_viz, m)
{
	py::bind_vector<std::vector<std::shared_ptr<Node>>>(m, "NodeVector");
	py::bind_map<std::map<std::string, NodeVar>>(m, "MapStringNodeVar");
	py::bind_map<std::map<std::string, NodeData>>(m, "MapStringNodeData");
	py::bind_map<decltype(NodeVar::attrs)>(m, "MapNodeAttrs");

	using SystemConfigT = decltype(Node::system_config);
	py::class_<SystemConfigT>(m, "SystemAttr")
	    .def_readonly("rng_seed", &SystemConfigT::rng_seed)
	    .def_readonly("width", &SystemConfigT::width)
	    .def_readonly("height", &SystemConfigT::height)
	    .def_readonly("link_delay", &SystemConfigT::link_delay)
	    .def_readonly("node_params", &SystemConfigT::node_params)
	    .def_readonly("event_params", &SystemConfigT::event_params);

	using NodeParamsT = decltype(SystemConfigT::node_params);
	py::class_<NodeParamsT>(m, "NodeParams")
	    .def_readonly("packet_len", &NodeParamsT::packet_len)
	    .def_readonly("p", &NodeParamsT::p);

	using EventParamsT = decltype(SystemConfigT::event_params);
	py::class_<EventParamsT>(m, "EventParams").def_readonly("e", &EventParamsT::e);

	py::class_<NodeRoleAttr>(m, "NodeRoleAttr").def_readonly("is_fpga", &NodeRoleAttr::is_fpga);

	py::class_<Node, std::shared_ptr<Node>>(m, "Node")
	    .def_readonly("x", &Node::x)
	    .def_readonly("y", &Node::y)
	    .def_readonly("data", &Node::data)
	    .def("get_current_var_value", &Node::get_current_var_value)
	    .def("add_var_to_viewer", &Node::add_var_to_viewer)
	    .def(
	        "add_hist",
	        [](Node& self, const NodeVar& var, const NodeVar& sampling_var,
	           std::vector<NodeVar> conditions, std::vector<NodeVar> masks, bool negedge) {
		        return self.add_hist(var, sampling_var, conditions, masks, negedge);
	        },
	        py::arg(), py::arg(), "conditions"_a = std::vector<NodeVar>{},
	        "masks"_a = std::vector<NodeVar>{}, "negedge"_a = false)
	    .def(
	        "add_hist",
	        [](Node& self, std::string name, std::vector<NodeVar> used, py::array_t<Histograms::DataT::simtime_t> times, py::array_t<Histograms::DataT::value_t> values) {
				// TODO(robin): do I need to do anything more to ensure contiguous data?
				auto times_span = std::span(times.data(), times.size());
				auto values_span = std::span(values.data(), values.size());
		        return self.add_hist(name, used, times_span, values_span);
	        })
	    .def(
	        "read_values",
	        [](Node& self, const NodeVar& var, const NodeVar& sampling_var,
	           std::vector<NodeVar> conditions = {}, std::vector<NodeVar> masks = {},
	           bool negedge = false) {
		        auto [a, b] =
		            self.ctx->read_values<uint64_t>(var, sampling_var, conditions, masks, negedge);
		        return std::make_pair(as_pyarray(std::move(a)), as_pyarray(std::move(b)));
	        },
	        py::arg(), py::arg(), "conditions"_a = std::vector<NodeVar>{},
	        "masks"_a = std::vector<NodeVar>{}, "negedge"_a = false)
	    .def_readonly("system_config", &Node::system_config)
	    .def_readonly("role", &Node::role)
	    .def("enqueue_task", &Node::enqueue_task);

	// TODO(robin): how to the lifetimes workout here?
	py::class_<AsyncNode, std::shared_ptr<AsyncNode>>(m, "AsyncNode")
	    .def(
	        "read_values",
	        [](std::shared_ptr<AsyncNode> self, const NodeVar& var, const NodeVar& sampling_var,
	           std::vector<NodeVar> conditions = {}, std::vector<NodeVar> masks = {},
	           bool negedge = false) {
		        return Awaitable::create(
		            [=]() {
			            auto res = self->ctx.read_values<uint64_t>(var, sampling_var, conditions, masks, negedge);
						std::println("got res, its a pair {} {}", res.first[0], res.second[0]);
						return res;
		            },
		            [](auto result) {
			            auto && [a, b] = result;
						std::println("casting, its still a pair {} {}", a[0], b[0]);
			            return py::make_tuple(as_pyarray(std::move(a)), as_pyarray(std::move(b)));
		            });
	        },
	        py::arg(), py::arg(), "conditions"_a = std::vector<NodeVar>{},
	        "masks"_a = std::vector<NodeVar>{}, "negedge"_a = false);

	// TODO(robin): does this have to be a shared_ptr?
	py::class_<Awaitable, std::shared_ptr<Awaitable>>(m, "Awaitable")
	    .def(
	        "__next__",
	        [](Awaitable * self) {
		        std::println("awaitable next");
		        return self->shared_from_this();
	        })
	    .def(
	        "__await__",
	        [](Awaitable * self) {
		        std::println("awaitable await");
		        return self->shared_from_this();
	        })
	    .def("send", [](Awaitable*, py::object result) {
		    std::println("awaitable send, got {}", result.attr("__repr__")().cast<std::string>());
		    throw StopIteration(result);
	    });

	py::class_<NodeData>(m, "NodeData")
	    .def_readonly("name", &NodeData::name)
	    .def_readonly("compname", &NodeData::compname)
	    .def_readonly("variables", &NodeData::variables)
	    .def_readonly("subscopes", &NodeData::subscopes);

	py::class_<NodeVar>(m, "NodeVar")
	    .def_readonly("name", &NodeVar::name)
	    .def_readonly("attrs", &NodeVar::attrs)
	    .def("format", [](NodeVar& self, char* value) { return self.format(value).data(); });
}

void Node::enqueue_task(std::function<py::object(py::object)> task)
{
	async_runner->add(
	    task, Awaitable::create([ctx = this->ctx]() {
			std::println("creating async node");
			return std::make_shared<AsyncNode>(*ctx);
		}));
}

template<class ...Args>
void Node::add_hist(Args && ...args) {
	std::println("add hist");
	histograms->add(std::forward<Args>(args)...);
}


void py_init_module_imgui_main(py::module& m);

PYBIND11_EMBEDDED_MODULE(imgui, m)
{
	py_init_module_imgui_main(m);
}
