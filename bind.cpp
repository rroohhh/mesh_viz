#include "node.h"
#include <pybind11/embed.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

PYBIND11_MAKE_OPAQUE(std::map<std::string, NodeVar>)
PYBIND11_MAKE_OPAQUE(std::map<std::string, NodeData>)

PYBIND11_EMBEDDED_MODULE(mesh_viz, m)
{
	py::bind_map<std::map<std::string, NodeVar>>(m, "MapStringNodeVar");
	py::bind_map<std::map<std::string, NodeData>>(m, "MapStringNodeData");

	py::class_<Node, std::shared_ptr<Node>>(m, "Node")
	    .def_readonly("x", &Node::x)
	    .def_readonly("y", &Node::y)
	    .def_readonly("data", &Node::data)
	    .def("get_current_var_value", &Node::get_current_var_value)
	    .def("add_var_to_viewer", &Node::add_var_to_viewer);

	py::class_<NodeData>(m, "NodeData")
	    .def_readonly("name", &NodeData::name)
	    .def_readonly("compname", &NodeData::compname)
	    .def_readonly("variables", &NodeData::variables)
	    .def_readonly("subscopes", &NodeData::subscopes);

	py::class_<NodeVar>(m, "NodeVar")
	    .def_readonly("name", &NodeVar::name)
	    .def("format", [](NodeVar& self, char* value) { return self.format(value).data(); });
}

void py_init_module_imgui_main(py::module& m);

PYBIND11_EMBEDDED_MODULE(imgui, m)
{
	py_init_module_imgui_main(m);
}
