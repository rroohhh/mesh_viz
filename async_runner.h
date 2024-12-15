#include <pybind11/embed.h>

#include <any>
#include <future>
#include <memory>
#include <optional>
#include <print>

namespace py = pybind11;
using namespace py::literals;

using coro_gen = std::function<py::object(py::object)>;

class Awaitable: public std::enable_shared_from_this<Awaitable> {
private:
	std::future<std::any> fut;
	std::function<py::object(std::any)> caster;

	template<class T, class C>
	Awaitable(T && todo, C && caster) : fut{std::async(std::launch::async, [todo=std::move(todo)](){
		std::println("running todo");
		return std::any{todo()};
	})}, caster([caster=std::move(caster)](std::any res) {
		std::println("casting the value back");
		return caster(std::any_cast<decltype(todo())>(res));
	}) {}

	template<class T>
	Awaitable(T && todo) : Awaitable(todo,  [](decltype(todo()) v) { return py::cast(v); }) {}

public:
	template<typename ... T>
	static std::shared_ptr<Awaitable> create(T&& ... t) {
		return std::shared_ptr<Awaitable>(new Awaitable(std::forward<T>(t)...));
	}

	std::optional<py::object> try_get() {
		using namespace std::literals::chrono_literals;
		if (fut.wait_for(0ms) == std::future_status::ready) {
			return caster(fut.get());
		}
		return std::nullopt;
	}

};

struct AsyncTask
{
	std::shared_ptr<Awaitable> awaitable;
	std::optional<py::object> coro;
	coro_gen coro_generator;
};

struct AsyncRunner
{
	std::vector<AsyncTask> tasks;

	void add(coro_gen coro_gen, std::shared_ptr<Awaitable> initial) {
		tasks.emplace_back(initial, std::nullopt, coro_gen);
	}

	void step() {
		std::erase_if(tasks, [](AsyncTask& task) {
			auto maybe_res = task.awaitable->try_get();
			if (maybe_res) {
				auto res = *maybe_res;
				if (not task.coro) {
					std::println("no coro, generating it");
					task.coro = task.coro_generator(res);
					res = py::none();
				}

				auto & coro = *task.coro;
				try {
					std::println("sending");
					auto send_res = coro.attr("send")(res);
					auto new_awaitable = py::cast<std::shared_ptr<Awaitable>>(send_res);
					std::println("got a new awaitable");
					task.awaitable = new_awaitable;
				} catch(py::error_already_set &e) {
					std::println("stop iteration");
					if (not e.matches(PyExc_StopIteration)) {
						std::println("async task errored: {}", e.what());
					}
					return true;
				}
			}
			return false;
		});
	}
};
