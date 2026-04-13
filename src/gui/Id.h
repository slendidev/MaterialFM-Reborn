#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Gui
{

struct Id
{
	uint32_t value {};

	auto operator==(Id const &) const -> bool = default;
	auto valid() const -> bool { return value != 0u; }
};

struct IdHash
{
	auto operator()(Id const id) const noexcept -> size_t
	{
		return static_cast<size_t>(id.value);
	}
};

constexpr auto id_hash_bytes(char const *const data, size_t const size)
    -> uint32_t
{
	uint32_t hash { 2166136261u };
	for (size_t i {}; i < size; ++i) {
		hash ^= static_cast<uint32_t>(static_cast<unsigned char>(data[i]));
		hash *= 16777619u;
	}
	if (hash == 0u) {
		return 1u;
	}
	return hash;
}

consteval auto id(char const *const literal, size_t const size) -> Id
{
	return Id { id_hash_bytes(literal, size) };
}

template<size_t N> consteval auto id(char const (&literal)[N]) -> Id
{
	static_assert(N > 0, "id literal size must be positive");
	return id(literal, N - 1);
}

inline auto id(std::string_view const value) -> Id
{
	return Id { id_hash_bytes(value.data(), value.size()) };
}

constexpr auto combine_id(Id const a, Id const b) -> Id
{
	uint32_t mixed { a.value };
	mixed ^= b.value + 0x9e3779b9u + (mixed << 6u) + (mixed >> 2u);
	if (mixed == 0u) {
		mixed = 1u;
	}
	return Id { mixed };
}

} // namespace Gui
