#include "gui/Animation.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Gui::Animation
{
namespace
{
constexpr float PI { 3.14159265358979323846f };

auto clamp01(float const value) -> float
{
	return std::clamp(value, 0.0f, 1.0f);
}
} // namespace

auto easing_sample(Easing const easing,
    float const t,
    std::function<float(float)> const &custom) -> float
{
	auto const x { clamp01(t) };
	switch (easing) {
	case Easing::Linear:
		return x;
	case Easing::EaseInQuad:
		return x * x;
	case Easing::EaseOutQuad:
		return 1.0f - (1.0f - x) * (1.0f - x);
	case Easing::EaseInOutQuad:
		return x < 0.5f ? 2.0f * x * x
		                : 1.0f - std::pow(-2.0f * x + 2.0f, 2.0f) * 0.5f;
	case Easing::EaseInCubic:
		return x * x * x;
	case Easing::EaseOutCubic:
		return 1.0f - std::pow(1.0f - x, 3.0f);
	case Easing::EaseInOutCubic:
		return x < 0.5f ? 4.0f * x * x * x
		                : 1.0f - std::pow(-2.0f * x + 2.0f, 3.0f) * 0.5f;
	case Easing::EaseInOutSine:
		return -(std::cos(PI * x) - 1.0f) * 0.5f;
	case Easing::Custom:
		if (custom) {
			return clamp01(custom(x));
		}
		return x;
	}
	return x;
}

auto Definition::builder(std::string_view const key) -> Builder
{
	return Builder { key };
}

auto Definition::get_ref() const -> Ref
{
	return Ref {
		.key = m_key,
		.spec = m_spec,
		.pause_if = m_pause_if,
		.fallback = m_fallback,
	};
}

Definition::Builder::Builder(std::string_view const key)
{
	m_definition.m_key = std::string(key);
	m_definition.m_spec = TweenSpec {};
	m_definition.m_fallback = m_definition.m_spec.from;
}

auto Definition::Builder::from(float const value) -> Builder &
{
	m_definition.m_spec.from = value;
	m_definition.m_fallback = value;
	return *this;
}

auto Definition::Builder::to(float const value) -> Builder &
{
	m_definition.m_spec.to = value;
	return *this;
}

auto Definition::Builder::duration(float const seconds) -> Builder &
{
	m_definition.m_spec.duration_seconds = seconds;
	return *this;
}

auto Definition::Builder::delay(float const seconds) -> Builder &
{
	m_definition.m_spec.delay_seconds = seconds;
	return *this;
}

auto Definition::Builder::easing(Easing const value) -> Builder &
{
	m_definition.m_spec.easing = value;
	return *this;
}

auto Definition::Builder::custom_easing(std::function<float(float)> fn)
    -> Builder &
{
	m_definition.m_spec.easing = Easing::Custom;
	m_definition.m_spec.custom_easing = std::move(fn);
	return *this;
}

auto Definition::Builder::repeat(RepeatMode const value) -> Builder &
{
	m_definition.m_spec.repeat = value;
	return *this;
}

auto Definition::Builder::pause_if(std::function<bool()> fn) -> Builder &
{
	m_definition.m_pause_if = std::move(fn);
	return *this;
}

auto Definition::Builder::fallback(float const value) -> Builder &
{
	m_definition.m_fallback = value;
	return *this;
}

auto Definition::Builder::build() const -> Definition
{
	return m_definition;
}

Tween::Tween(TweenSpec const spec)
{
	configure(spec);
}

auto Tween::configure(TweenSpec spec) -> void
{
	m_spec = std::move(spec);
	m_spec.duration_seconds = std::max(0.0001f, m_spec.duration_seconds);
	m_spec.delay_seconds = std::max(0.0f, m_spec.delay_seconds);
	restart();
}

auto Tween::restart() -> void
{
	m_elapsed_seconds = 0.0f;
	m_running = true;
	m_forward = true;
}

auto Tween::stop() -> void
{
	m_running = false;
}

auto Tween::set_paused(bool const paused) -> void
{
	m_paused = paused;
}

auto Tween::cycle_length() const -> float
{
	return m_spec.delay_seconds + m_spec.duration_seconds;
}

auto Tween::normalized_progress() const -> float
{
	if (m_elapsed_seconds <= m_spec.delay_seconds) {
		return m_forward ? 0.0f : 1.0f;
	}
	auto const local_time {
		(m_elapsed_seconds - m_spec.delay_seconds) / m_spec.duration_seconds,
	};
	auto const base { clamp01(local_time) };
	return m_forward ? base : (1.0f - base);
}

auto Tween::value() const -> float
{
	auto const eased {
		easing_sample(
		    m_spec.easing, normalized_progress(), m_spec.custom_easing),
	};
	return m_spec.from + (m_spec.to - m_spec.from) * eased;
}

auto Tween::tick(float const dt_seconds) -> bool
{
	if (!m_running || m_paused || dt_seconds <= 0.0f) {
		return false;
	}

	auto const before { value() };
	m_elapsed_seconds += dt_seconds;
	auto const len { cycle_length() };

	while (m_elapsed_seconds >= len) {
		if (m_spec.repeat == RepeatMode::Once) {
			m_elapsed_seconds = len;
			m_running = false;
			break;
		}
		m_elapsed_seconds -= len;
		if (m_spec.repeat == RepeatMode::PingPong) {
			m_forward = !m_forward;
		}
	}

	auto const after { value() };
	return std::abs(after - before) > 0.0001f;
}

} // namespace Gui::Animation
