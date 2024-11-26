#pragma once

#include "../nlohmann/json.hpp"
#include <boost/multiprecision/cpp_int.hpp>
using json = nlohmann::json;

using bigint = boost::multiprecision::cpp_int;

namespace nlohmann {
template <typename T>
struct adl_serializer<std::shared_ptr<T>>
{
	static void to_json(json& j, const std::shared_ptr<T>& opt)
	{
		if (opt) {
			j = *opt;
		} else {
			j = nullptr;
		}
	}

	static void from_json(const json& j, std::shared_ptr<T>& opt)
	{
		if (j.is_null()) {
			opt = nullptr;
		} else {
			opt.reset(new T(j.get<T>()));
		}
	}
};
}

namespace impl {
struct Const
{
	bigint value;
};

struct Signal
{};
struct Slice;
struct SwitchValue;
struct Operator;

class FormatStatement;
using FormatStatementP = std::shared_ptr<FormatStatement>;

struct Slice
{
	FormatStatementP value;
	uint64_t start;
	uint64_t stop;
};

struct SwitchValue
{
	FormatStatementP test;
	struct SwitchCase
	{
		// value and mask
		std::vector<std::pair<bigint, bigint>> cases;
		FormatStatementP value;
	};
	std::vector<SwitchCase> cases;
};

struct Operator
{
	std::string op;

	std::vector<FormatStatement> operands;
};

using Literal = std::string;
struct Formatted
{
	std::string specifier;
	FormatStatementP arg;
};

using FormatChunk = std::variant<Literal, Formatted>;

class FormatStatement : public std::variant<Const, Signal, Slice, SwitchValue, Operator>
{};

void from_json(const json& j, Slice& n);

void from_json(const json& j, Const& n);

void from_json(const json& j, Signal& n);

void from_json(const json& j, FormatStatement& n);

void from_json(const json& j, Operator& n);

void from_json(const json& j, SwitchValue::SwitchCase& n);

void from_json(const json& j, SwitchValue& n);

void from_json(const json& j, FormatStatement& n);

void from_json(const json& j, Formatted& n);

void from_json(const json& j, FormatChunk& n);
};

struct Formatter
{
	virtual std::span<char> format(char* signal) const = 0;
	virtual ~Formatter() {};
};

struct BinaryFormatter : public Formatter
{
	std::span<char> format(char* signal) const override;
};

struct HexFormatter : public Formatter
{
	mutable std::string res;
	std::span<char> format(char* signal) const override;
};

struct FixedFormatter : public Formatter
{
	std::vector<impl::FormatChunk> chunks;
	mutable std::string cache;
	mutable std::string cache_cleaned;

	std::span<char> format(char* signal) const override;

private:
	bigint visit_stmt(const impl::Slice& slice, const bigint& s) const;

	bigint visit_stmt(const impl::Const& c, const bigint&) const;

	bigint visit_stmt(const impl::Signal&, const bigint& s) const;

	bigint visit_stmt(const impl::Operator& op, const bigint& s) const;

	bigint visit_stmt(const impl::SwitchValue& sv, const bigint& s) const;

	bigint visit_stmt(const impl::FormatStatementP& stmt, const bigint& s) const;

	bigint visit_stmt(const impl::FormatStatement& stmt, const bigint& s) const;

	void visit_chunk(const impl::Literal& lit, const bigint& s) const;

	void visit_chunk(const impl::Formatted& fmt, const bigint& s) const;

	bigint bin_to_bigint(const char* signal) const;
};

void from_json(const json& j, FixedFormatter& n);

FixedFormatter parse_formatter(std::string fmt);
