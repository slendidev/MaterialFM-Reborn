#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace Gui
{

namespace detail
{
struct IdEntry
{
	std::string value {};
	std::size_t short_offset {};
};
} // namespace detail

struct Id
{
	using Entry = detail::IdEntry;

	Entry const *entry {};

	auto operator==(Id const &) const -> bool = default;
	auto valid() const -> bool { return entry != nullptr; }

	auto label() const -> std::string_view
	{
		if (entry == nullptr) {
			return {};
		}
		return entry->value;
	}

	auto short_label() const -> std::string_view
	{
		if (entry == nullptr) {
			return {};
		}
		return std::string_view(entry->value).substr(entry->short_offset);
	}

	struct Hash
	{
		auto operator()(Id const id) const noexcept -> std::size_t
		{
			return std::hash<Entry const *> {}(id.entry);
		}
	};
};

} // namespace Gui
