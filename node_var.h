#pragma once

#include "core.h"
#include "formatter.h"

#include <variant>
#include <span>

struct Node;

using NodeID = handle_t;

// node vars should know which Node they belong to, because we want to support multiple fstfiles per
// trace (ie up to one per node to avoid inter thread sync)
struct NodeVar
{
	std::string name;
	uint64_t nbits;
	std::shared_ptr<Node> owner_node;
	std::shared_ptr<Formatter> formatter;
	std::map<std::string, std::variant<int64_t, uint64_t, double, std::string>> attrs;

	NodeVar(
	    std::string name,
	    uint64_t nbits,
	    handle_t handle,
	    std::shared_ptr<Node> owner_node,
	    std::shared_ptr<Formatter> formatter,
		decltype(NodeVar::attrs) attrs);

	char* value_at_time(clock_t time) const;

	bool is_vector() const;

	std::span<char> format(char* value) const;

	std::string pretty_name() const;

	// this returns a stable ID which always refers to the same variable,
	// but not necessarily to the same state (different formatter, etc)
	NodeID stable_id() const;
private:
	using handle_t = ::handle_t;
	handle_t handle;
	friend struct FstFile;
};


template <>
struct std::formatter<NodeVar, char>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	auto format(const auto& s, auto& ctx) const
	{
		return std::format_to(ctx.out(), "NodeVar{{name={}, nbits={}}}", s.name, s.nbits);
	}
};
