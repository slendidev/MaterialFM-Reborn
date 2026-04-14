#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Gui
{

struct Id
{
	uint32_t value {};
	std::string_view literal_label {};
	std::optional<std::string> backing_label {};

	auto operator==(Id const &) const -> bool = default;
	auto valid() const -> bool { return value != 0u; }

	auto label() const -> std::string_view
	{
		if (backing_label) {
			return *backing_label;
		}
		return literal_label;
	}

	auto short_label() const -> std::string_view
	{
		auto full = label();
		auto pos = full.find_last_of('/');

		if (pos == std::string_view::npos) {
			return full;
		}
		return full.substr(pos + 1);
	}

	struct Hash
	{
		auto operator()(Id const id) const noexcept -> size_t
		{
			return static_cast<size_t>(id.value);
		}
	};
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
	return Id { id_hash_bytes(literal, size),
		std::string_view(literal, size),
		std::nullopt };
}

template<size_t N> consteval auto id(char const (&literal)[N]) -> Id
{
	static_assert(N > 0, "id literal size must be positive");
	return id(literal, N - 1);
}

inline auto id(std::string_view const value) -> Id
{
	return Id {
		id_hash_bytes(value.data(), value.size()), value, std::nullopt
	};
}

inline auto combine_id(Id const &a, Id const &b) -> Id
{
	uint32_t mixed { a.value };
	mixed ^= b.value + 0x9e3779b9u + (mixed << 6u) + (mixed >> 2u);
	if (mixed == 0u) {
		mixed = 1u;
	}

	Id out;
	out.value = mixed;
	out.backing_label = std::string(a.label()) + "/" + std::string(b.label());
	return out;
}

} // namespace Gui
