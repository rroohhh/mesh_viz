#include "formatter.h"

#include <print>

void impl::from_json(const json& j, Slice& n)
{
	j.at("start").get_to(n.start);
	j.at("stop").get_to(n.stop);
	j.at("value").get_to(n.value);
}
void impl::from_json(const json& j, Const& n)
{
	bigint i(j.at("value").template get<std::string>());
	n.value = i;
}

// sentinel
void impl::from_json(const json&, Signal&) {}

void impl::from_json(const json& j, Operator& n)
{
	j.at("operator").get_to(n.op);
	j.at("operands").get_to(n.operands);
}
void impl::from_json(const json& j, SwitchValue::SwitchCase& n)
{
	std::vector<std::pair<bigint, bigint>> parsed_cases;
	if (not j.at(0).is_null()) {
		auto cases = j.at(0).template get<std::vector<json>>();
		for (auto& c : cases) {
			// this will always be binary
			auto value = c.template get<std::string>();
			bigint mask(0);
			bigint expected(0);

			for (auto& c : value) {
				mask = mask << 1;
				expected = expected << 1;
				if (c == '1') {
					mask |= 1;
					expected |= 1;
				} else if (c == '0') {
					mask |= 1;
				}
			}

			parsed_cases.emplace_back(expected, mask);
		}
	}
	n.cases = parsed_cases;
	j.at(1).get_to(n.value);
}
void impl::from_json(const json& j, SwitchValue& n)
{
	j.at("test").get_to(n.test);
	j.at("cases").get_to(n.cases);
}
void impl::from_json(const json& j, FormatStatement& n)
{
	std::string type = j.at("type");
	if (type == "Slice") {
		n = {j.template get<Slice>()};
	} else if (type == "Const") {
		n = {j.template get<Const>()};
	} else if (type == "Signal") {
		n = {j.template get<Signal>()};
	} else if (type == "Operator") {
		n = {j.template get<Operator>()};
	} else if (type == "SwitchValue") {
		n = {j.template get<SwitchValue>()};
	} else {
		assert(false);
	}
}
void impl::from_json(const json& j, Formatted& n)
{
	j.at(1).get_to(n.specifier);
	j.at(0).get_to(n.arg);
}
void impl::from_json(const json& j, FormatChunk& n)
{
	if (j.type() == json::value_t::string) {
		n = j.template get<std::string>();
	} else {
		n = j.template get<Formatted>();
	}
}
std::span<char> BinaryFormatter::format(char* signal) const
{
	// TODO(robin): this is annoying, make this a std::span from the beginning
	return {signal, strlen(signal)};
}
std::span<char> HexFormatter::format(char* signal) const
{
	res.clear();

	auto len = strlen(signal);
	auto step = len % 4;
	if (step == 0) step = 4;

	char table[16]{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
	while (len > 0) {
		uint8_t fmt_idx = 0;
		if (step == 1) {
			fmt_idx |= (signal[0] == '1') << 0;
		} else if (step == 2) {
			fmt_idx |= (signal[1] == '1') << 0;
			fmt_idx |= (signal[0] == '1') << 1;
		} else if (step == 3) {
			fmt_idx |= (signal[2] == '1') << 0;
			fmt_idx |= (signal[1] == '1') << 1;
			fmt_idx |= (signal[0] == '1') << 2;
		} else {
			fmt_idx |= (signal[3] == '1') << 0;
			fmt_idx |= (signal[2] == '1') << 1;
			fmt_idx |= (signal[1] == '1') << 2;
			fmt_idx |= (signal[0] == '1') << 3;
		}

		res += table[fmt_idx];
		len -= step;
		signal += step;
		step = 4;
	}
	return {res};
}
std::span<char> FixedFormatter::format(char* signal) const
{
	cache.clear();
	auto s = bin_to_bigint(signal);
	for (auto& chunk : chunks) {
		std::visit([&](auto chunk) { visit_chunk(chunk, s); }, chunk);
	}
	size_t pos = 0;
	cache_cleaned.assign(cache.size(), ' ');
	for (auto& c : cache) {
		if (c != '\x08') {
			cache_cleaned[pos++] = c;
		} else if (pos > 0) {
			pos -= 1;
		}
	}
	cache_cleaned.resize(pos);
	while (cache_cleaned.back() == ' ') {
		cache_cleaned.pop_back();
	}
	return {cache_cleaned};
}
bigint FixedFormatter::visit_stmt(const impl::Slice& slice, const bigint& s) const
{
	bigint mask(0);
	boost::multiprecision::bit_set(mask, slice.stop - slice.start);
	return (visit_stmt(slice.value, s) >> slice.start) & (mask - 1);
}
bigint FixedFormatter::visit_stmt(const impl::Const& c, const bigint&) const
{
	return c.value;
}
bigint FixedFormatter::visit_stmt(const impl::Signal&, const bigint& s) const
{
	return s;
}
bigint FixedFormatter::visit_stmt(const impl::Operator& op, const bigint& s) const
{
	if (op.op == "+") {
		return visit_stmt(op.operands[0], s) + visit_stmt(op.operands[1], s);
	} else if (op.op == "==") {
		return visit_stmt(op.operands[0], s) == visit_stmt(op.operands[1], s);
	} else {
		std::println("op: {}", op.op);
		std::unreachable();
	}
}
bigint FixedFormatter::visit_stmt(const impl::SwitchValue& sv, const bigint& s) const
{
	auto test = visit_stmt(sv.test, s);
	for (const auto& c : sv.cases) {
		if (c.cases.size() == 0) {
			return visit_stmt(c.value, s);
		} else {
			for (const auto& [expected, mask] : c.cases) {
				if ((test & mask) == expected) {
					return visit_stmt(c.value, s);
				}
			}
		}
	}
	std::unreachable();
}
bigint FixedFormatter::visit_stmt(const impl::FormatStatementP& stmt, const bigint& s) const
{
	return visit_stmt(*stmt, s);
}
bigint FixedFormatter::visit_stmt(const impl::FormatStatement& stmt, const bigint& s) const
{
	return std::visit([&](auto stmt) { return visit_stmt(stmt, s); }, stmt);
}
void FixedFormatter::visit_chunk(const impl::Literal& lit, const bigint&) const
{
	cache += lit;
}
void FixedFormatter::visit_chunk(const impl::Formatted& fmt, const bigint& s) const
{
	auto value = visit_stmt(fmt.arg, s);
	if (fmt.specifier == "") {
		cache += value.str();
	} else if (fmt.specifier == "s") {
		while (value > 0) {
			cache += (char) (value & 0xff);
			value >>= 8;
		}
	} else {
		std::println("spec: {}", fmt.specifier);
		assert(false);
	}
}
bigint FixedFormatter::bin_to_bigint(const char* signal) const
{
	bigint i;

	// TODO(robin): pass in length
	int len = strlen(signal);
	size_t pos = 0;
	while (signal[pos] != '\0') {
		if (signal[pos] == '1') {
			boost::multiprecision::bit_set(i, len - pos - 1);
		}
		pos++;
	}
	return i;
}

void from_json(const json& j, FixedFormatter& n)
{
	j.get_to(n.chunks);
}
FixedFormatter parse_formatter(std::string fmt)
{
	return json::parse(fmt).template get<FixedFormatter>();
}
