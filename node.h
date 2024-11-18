#pragma once

#include <memory>
#include <string>
#include "formatter.h"
#include "imgui.h"

struct Node;
struct NodeVar;
struct WaveformViewer;
struct FstFile;

#include "fst_file.h"

// node vars should know which Node they belong to, because we want to support multiple fstfiles per trace (ie up to one
// per node to avoid inter thread sync)
struct NodeVar {
    std::string name;
    uint64_t nbits;
    // TODO(robin): shared_ptr?
    std::shared_ptr<Node> owner_node;
    std::shared_ptr<Formatter> formatter;

    NodeVar(std::string name, uint64_t nbits, handle_t handle, std::shared_ptr<Node> owner_node, std::shared_ptr<Formatter> formatter)
        : name(name), nbits(nbits), owner_node(owner_node), formatter(std::move(formatter)), handle(handle) {}

    char * value_at_time(clock_t time) const;

    bool is_vector();

    std::span<char> format(char * value);

private:
    handle_t handle;
    friend Node;
    friend FstFile;
    friend WaveformViewer;
};

template <>
struct std::formatter<NodeVar, char> {
    constexpr auto parse(std::format_parse_context & ctx) { return ctx.begin(); }

    auto format(const auto & s, auto & ctx) const {
        return std::format_to(ctx.out(), "NodeVar{{name={}, nbits={}}}", s.name, s.nbits);
    }
};

struct NodeData {
    std::string name;
    std::string compname;

    std::map<std::string, NodeData> subscopes;
    std::map<std::string, NodeVar>  variables;
};

const int NODE_SIZE = 100;
struct Node : public std::enable_shared_from_this<Node> {
    int      x, y;
    NodeData data;
    uint64_t current_time;

    Node(int x, int y, NodeData data, FstFile * ctx) : x(x), y(y), data(data), ctx(ctx) {}

    void render(uint64_t c_time, const ImVec2 & offset, const float & zoom,
                std::function<void(std::shared_ptr<Node>)> process_func, WaveformViewer * v);

    char * get_current_var_value(const NodeVar & var);

    char * value_at_time(const NodeVar & var, simtime_t time);

    void add_var_to_viewer(const NodeVar & var);

private:
    FstFile *        ctx;
    WaveformViewer * viewer;

    void render_data(const NodeData & data, float width) const;
};
