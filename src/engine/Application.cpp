#include "engine/Application.h"

#include "engine/Common.h"

namespace Engine
{

BaseApplication::BaseApplication() : m_assets(), m_renderer(m_assets) { }

auto BaseApplication::run() -> void
{
	Platform::initialize();
	defer(Platform::shutdown());

	m_running = true;
	on_init();

	auto last { Platform::now_seconds() };
	while (m_running) {
		if (Platform::is_exit_requested()) {
			request_exit();
			continue;
		}

		auto const now { Platform::now_seconds() };
		auto const dt { static_cast<float>(now - last) };
		last = now;

		m_prev_input = m_input;
		m_input = Platform::poll_input();

		on_update(dt);
	}

	on_shutdown();
}

auto BaseApplication::on_init() -> void { }

auto BaseApplication::on_shutdown() -> void { }

auto BaseApplication::is_down(Button const button) const -> bool
{
	return (m_input.buttons & detail::button_mask(button)) != 0;
}

auto BaseApplication::is_up(Button const button) const -> bool
{
	return !is_down(button);
}

auto BaseApplication::is_pressed(Button const button) const -> bool
{
	auto const mask { detail::button_mask(button) };
	return (m_input.buttons & mask) != 0 && (m_prev_input.buttons & mask) == 0;
}

auto BaseApplication::is_released(Button const button) const -> bool
{
	auto const mask { detail::button_mask(button) };
	return (m_input.buttons & mask) == 0 && (m_prev_input.buttons & mask) != 0;
}

auto BaseApplication::stick() const -> smath::Vec2
{
	return m_input.stick;
}

} // namespace Engine
