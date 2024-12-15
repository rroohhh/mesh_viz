#include "node_var.h"

#include "node.h"

NodeVar::NodeVar(
    std::string name,
    uint64_t nbits,
    handle_t handle,
    std::shared_ptr<Node> owner_node,
    std::shared_ptr<Formatter> formatter,
    decltype(NodeVar::attrs) attrs) :
    name(name),
    nbits(nbits),
    owner_node(owner_node),
    formatter(std::move(formatter)),
    attrs(attrs),
    handle(handle)
{
}

std::string NodeVar::pretty_name() const
{
	return std::format("[{},{}] {}", owner_node->x, owner_node->y, name);
}

NodeID NodeVar::stable_id() const {
	return handle;
}
