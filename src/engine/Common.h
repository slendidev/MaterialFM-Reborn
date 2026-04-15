#pragma once

#include <cstddef>
#include <type_traits>

namespace Engine
{

template<typename T>
requires std::is_integral_v<T>
constexpr auto is_power_of_two(T const x) -> bool
{
	return x > 0 && (x & (x - 1)) == 0;
}

template<typename T>
requires std::is_integral_v<T>
constexpr auto next_power_of_two(T x) -> T
{
	if (x <= 1) {
		return 1;
	}

	--x;
	for (size_t shift = 1; shift < (sizeof(T) * 8); shift <<= 1) {
		x |= (x >> shift);
	}

	return x + 1;
}

namespace detail
{
[[noreturn]] void panic_impl(
    char const message[], char const file[], int const line);
void assert_impl(bool const condition,
    char const message[],
    char const file[],
    int const line);
} // namespace detail

} // namespace Engine

#ifdef NDEBUG
#define sassert(cond, message) ((void)(cond))
#else
#define sassert(cond, message) \
	::Engine::detail::assert_impl(cond, message, __FILE__, __LINE__)
#endif // NDEBUG

#define panic(message) ::Engine::detail::panic_impl(message, __FILE__, __LINE__)

namespace Engine::detail
{
template<typename F> struct PrivDefer
{
	F f;
	PrivDefer(F f) : f(f) { }
	~PrivDefer() { f(); }
};

template<typename F> PrivDefer<F> defer_func(F f)
{
	return PrivDefer<F>(f);
}
} // namespace Engine::detail

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x) DEFER_2(x, __COUNTER__)
#define defer(code) \
	auto const DEFER_3(_defer_) \
	{ \
		::Engine::detail::defer_func([&]() { code; }) \
	}
