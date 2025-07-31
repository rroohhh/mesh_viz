#pragma once

#include <cinttypes>

#include "libfst/fstapi.h"


extern float scale;
extern float DPI_SCALE;
extern int CURSOR_COL;
extern int TIMELINE_TICK_COL;
extern int NODE_COL;
extern int FPGA_COL;
extern int NODE_HIGHLIGHT_COL;

extern float MOUSE_WHEEL_DRAG_FACTOR;

using simtime_t = uint32_t;
using simtimedelta_t = int64_t;


using handle_t = fstHandle;
