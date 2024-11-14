#pragma once

#include <vector>
#include "libfst/fstapi.h"

using handle_t       = fstHandle;
using simtime_t      = uint64_t;
using simtimedelta_t = int64_t;

#include "node.h"


using value_change_cb_t = std::function<void(uint64_t time, handle_t facidx, const unsigned char * value)>;

struct FstFile;
struct FstFile {
    using fstReader = void *;
    fstReader reader;
    char *    buffer;

    ~FstFile();

    FstFile(const char * path);

    std::vector<std::shared_ptr<Node>> read_nodes();

    static void value_change_callback(void * user_callback_data_pointer, uint64_t time, handle_t facidx,
                                      const unsigned char * value);

    static void value_change_callback2(void * user_callback_data_pointer, uint64_t time, handle_t facidx,
                                       const unsigned char * value, uint32_t len);

    void read_changes(uint64_t min_time, uint64_t max_time, std::vector<NodeVar> & vars, value_change_cb_t cb);

    uint64_t min_time();

    uint64_t max_time();

    char * get_value_at(handle_t handle, uint64_t time);
};
