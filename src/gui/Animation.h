#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace Gui
{
class Context;
}

namespace Gui::Animation
{

enum class Easing
{
	Linear,
	EaseInQuad,
	EaseOutQuad,
	EaseInOutQuad,
	EaseInCubic,
	EaseOutCubic,
	EaseInOutCubic,
	EaseInOutSine,
	Custom,
};

enum class RepeatMode
{
	Once,
	Loop,
	PingPong,
};

struct TweenSpec
{
	float from {};
	float to { 1.0f };
	float duration_seconds { 0.2f };
	float delay_seconds {};
	Easing easing { Easing::Linear };
	std::function<float(float)> custom_easing {};
	RepeatMode repeat { RepeatMode::Once };
};

struct Ref
{
	std::string key {};
	TweenSpec spec {};
	std::function<bool()> pause_if {};
	float fallback {};

	Ref() = default;
	Ref(std::string key,
	    TweenSpec spec,
	    std::function<bool()> pause_if,
	    float fallback)
	    : key(std::move(key)), spec(std::move(spec)),
	      pause_if(std::move(pause_if)), fallback(fallback)
	{ }

	auto valid() const -> bool { return !key.empty(); }
	auto generation() const -> uint32_t { return m_generation; }

private:
	auto set_generation(uint32_t const generation) -> void
	{
		m_generation = generation;
	}

	friend class Gui::Context;
	uint32_t m_generation {};
};

class Definition
{
public:
	class Builder;

	static auto builder(std::string_view key) -> Builder;
	auto get_ref() const -> Ref
	{
		return Ref { m_key, m_spec, m_pause_if, m_fallback };
	}

private:
	std::string m_key {};
	TweenSpec m_spec {};
	std::function<bool()> m_pause_if {};
	float m_fallback {};

	friend class Builder;
};

class Definition::Builder
{
public:
	explicit Builder(std::string_view const key)
	{
		m_definition.m_key = std::string(key);
		m_definition.m_spec = TweenSpec {};
		m_definition.m_fallback = m_definition.m_spec.from;
	}
	auto from(float const value) -> Builder &
	{
		m_definition.m_spec.from = value;
		m_definition.m_fallback = value;
		return *this;
	}
	auto to(float const value) -> Builder &
	{
		m_definition.m_spec.to = value;
		return *this;
	}
	auto duration(float const seconds) -> Builder &
	{
		m_definition.m_spec.duration_seconds = seconds;
		return *this;
	}
	auto delay(float const seconds) -> Builder &
	{
		m_definition.m_spec.delay_seconds = seconds;
		return *this;
	}
	auto easing(Easing const value) -> Builder &
	{
		m_definition.m_spec.easing = value;
		return *this;
	}
	auto custom_easing(std::function<float(float)> fn) -> Builder &
	{
		m_definition.m_spec.easing = Easing::Custom;
		m_definition.m_spec.custom_easing = std::move(fn);
		return *this;
	}
	auto repeat(RepeatMode const value) -> Builder &
	{
		m_definition.m_spec.repeat = value;
		return *this;
	}
	auto pause_if(std::function<bool()> fn) -> Builder &
	{
		m_definition.m_pause_if = std::move(fn);
		return *this;
	}
	auto fallback(float const value) -> Builder &
	{
		m_definition.m_fallback = value;
		return *this;
	}
	auto build() const -> Definition { return m_definition; }

private:
	Definition m_definition {};
};

inline auto Definition::builder(std::string_view const key) -> Builder
{
	return Builder { key };
}

class Tween
{
public:
	Tween() = default;
	explicit Tween(TweenSpec spec);

	auto configure(TweenSpec spec) -> void;
	auto restart() -> void;
	auto stop() -> void;
	auto set_paused(bool const paused) -> void { m_paused = paused; }
	auto paused() const -> bool { return m_paused; }
	auto running() const -> bool { return m_running; }
	auto value() const -> float;
	auto tick(float dt_seconds) -> bool;

private:
	auto cycle_length() const -> float;
	auto normalized_progress() const -> float;

	TweenSpec m_spec {};
	float m_elapsed_seconds {};
	bool m_running {};
	bool m_paused {};
	bool m_forward { true };
};

auto easing_sample(
    Easing easing, float t, std::function<float(float)> const &custom = {})
    -> float;

} // namespace Gui::Animation
