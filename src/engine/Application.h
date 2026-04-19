#pragma once

#include "engine/AssetManager.h"
#include "engine/Renderer.h"
#include "engine/platform/Platform.h"

namespace Engine
{

class BaseApplication
{
public:
	BaseApplication();
	virtual ~BaseApplication() = default;

	auto run() -> void;

protected:
	virtual auto on_init() -> void;
	virtual auto on_update(float dt) -> void = 0;
	virtual auto on_shutdown() -> void;

	auto is_down(Button const button) const -> bool
	{
		return (m_input.buttons & detail::button_mask(button)) != 0;
	}
	auto is_up(Button const button) const -> bool { return !is_down(button); }
	auto is_pressed(Button const button) const -> bool
	{
		auto const mask { detail::button_mask(button) };
		return (m_input.buttons & mask) != 0
		    && (m_prev_input.buttons & mask) == 0;
	}
	auto is_released(Button const button) const -> bool
	{
		auto const mask { detail::button_mask(button) };
		return (m_input.buttons & mask) == 0
		    && (m_prev_input.buttons & mask) != 0;
	}
	auto stick() const -> smath::Vec2 { return m_input.stick; }
	auto request_exit() -> void { m_running = false; }
	auto renderer() -> Renderer & { return m_renderer; }
	auto renderer() const -> Renderer const & { return m_renderer; }
	auto assets() -> AssetManager & { return m_assets; }
	auto assets() const -> AssetManager const & { return m_assets; }

private:
	bool m_running {};
	AssetManager m_assets {};
	Renderer m_renderer;
	detail::InputState m_input {};
	detail::InputState m_prev_input {};
};

} // namespace Engine
