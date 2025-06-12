#include <format>
#include <vector>


// TODO(robin): g++-14.2 does not implement this part of the c++23 standard :(
template <typename T>
struct std::formatter<std::vector<T>, char>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	auto format(const auto& s, auto& ctx) const
	{
		std::format_to(ctx.out(), "[");
		bool first = true;
		for (auto elem : s) {
			if (first) {
				std::format_to(ctx.out(), "{}", elem);

			} else {
				std::format_to(ctx.out(), ", {}", elem);
			}
			first = false;
		}
		return std::format_to(ctx.out(), "]");
	}
};
