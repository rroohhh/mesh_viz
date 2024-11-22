inline ImVec2 operator-(const ImVec2& a, const ImVec2& b)
{
	return {a.x - b.x, a.y - b.y};
}

inline ImVec2 operator+(const ImVec2& a, const ImVec2& b)
{
	return {a.x + b.x, a.y + b.y};
}

inline ImVec2& operator+=(ImVec2& a, const ImVec2& b)
{
	a = a + b;
	return a;
}

inline ImVec2 operator/(const ImVec2& a, const float& b)
{
	return {a.x / b, a.y / b};
}

inline ImVec2 operator*(const ImVec2& a, const float& b)
{
	return {a.x * b, a.y * b};
}

inline bool operator>(const ImVec2& a, const ImVec2& b)
{
	return a.x > b.x and a.y > b.y;
}

inline auto min(auto a, auto b)
{
	return a < b ? a : b;
}

inline auto max(auto a, auto b)
{
	return a > b ? a : b;
}

inline auto clip(auto a, auto min, auto max)
{
	return ::min(::max(a, min), max);
}
