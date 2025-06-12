#pragma once

#include <cstdint>

struct Cursor {
    int64_t pos = 0;

    int64_t min_time, max_time;

    Cursor(int64_t min_time, int64_t max_time);

	bool render(double offset, double zoom, bool update);
};
