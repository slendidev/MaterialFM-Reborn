#pragma once

#include <functional>
#include <optional>
#include <utility>

namespace Gui
{

template<typename T> class MutableState
{
public:
	using Getter = std::function<T const &()>;
	using Setter = std::function<void(T)>;
	using Updater = std::function<void(std::function<void(T &)>)>;

	MutableState() = default;
	MutableState(Getter getter, Setter setter, Updater updater)
	    : m_get(std::move(getter)), m_set(std::move(setter)),
	      m_update(std::move(updater))
	{ }
	MutableState(Getter getter, Setter setter, Updater updater, T fallback)
	    : m_get(std::move(getter)), m_set(std::move(setter)),
	      m_update(std::move(updater)), m_fallback(std::move(fallback))
	{ }

	auto get() const -> T const &
	{
		if (m_get) {
			return m_get();
		}
		if (!m_fallback.has_value()) {
			m_fallback.emplace();
		}
		return *m_fallback;
	}
	auto set(T value) const -> void
	{
		if (m_set) {
			m_set(std::move(value));
		}
	}
	template<typename Fn> auto update(Fn &&fn) const -> void
	{
		if (!m_update) {
			return;
		}
		m_update([&](T &value) { fn(value); });
	}

private:
	Getter m_get {};
	Setter m_set {};
	Updater m_update {};
	mutable std::optional<T> m_fallback {};
};

} // namespace Gui
