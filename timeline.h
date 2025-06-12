#pragma once

#include <cstdint>
#include <memory>

#include "cursor.h"

class Timeline {
	int64_t min_time, max_time;
	std::shared_ptr<Cursor> cursor;

public:
	Timeline(uint32_t min_time, uint32_t max_time, std::shared_ptr<Cursor> cursor);

	std::pair<uint32_t, uint32_t> render(double zoom, double offset);
};
