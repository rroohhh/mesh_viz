#pragma once

#include <cinttypes>

#include "libfst/fstapi.h"

extern float DPI_SCALE;
extern int CURSOR_COL;
extern int TIMELINE_TICK_COL;
extern int NODE_COL;
extern int FPGA_COL;
extern int NODE_HIGHLIGHT_COL;

using simtime_t = uint32_t;
using simtimedelta_t = int64_t;


using handle_t = fstHandle;
