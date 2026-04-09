#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include <smath.hpp>

#include "engine/Asset.h"
#include "engine/Math.h"

namespace Engine
{

enum class Button : uint32_t
{
	Up = 0,
	Down,
	Left,
	Right,
	Cross,
	Circle,
	Start,
	Select,
	Triangle,
	LeftTrigger,
	RightTrigger,
};

namespace detail
{
struct InputState
{
	uint32_t buttons {};
	smath::Vec2 stick {};
};

struct GraphicsVertex
{
	float u {};
	float v {};
	uint32_t color {};
	float x {};
	float y {};
	float z {};
};

constexpr auto button_mask(Button const button) -> uint32_t
{
	return 1u << static_cast<uint32_t>(button);
}
} // namespace detail

namespace Platform
{

struct RendererStats
{
	uint32_t batch_submits {};
	uint32_t textured_submits {};
	uint32_t solid_submits {};
	uint32_t texture_binds {};
	uint32_t texture_uploads {};
	uint32_t texture_upload_bytes {};
	uint32_t clip_pushes {};
	uint32_t clip_pops {};
};

auto initialize() -> void;
auto shutdown() -> void;
auto now_seconds() -> double;
auto poll_input() -> ::Engine::detail::InputState;
auto is_exit_requested() -> bool;

auto renderer_create() -> void;
auto renderer_destroy() -> void;
auto renderer_begin_frame() -> void;
auto renderer_end_frame() -> void;
auto renderer_mode_2d() -> void;
auto renderer_clear(smath::Vec4 color) -> void;
auto renderer_submit_batch(Texture const *texture,
    std::span<detail::GraphicsVertex const> vertices,
    std::span<uint16_t const> indices) -> void;
auto renderer_push_scissor(Rect<> rect) -> void;
auto renderer_pop_scissor() -> void;
auto renderer_stats() -> RendererStats;

} // namespace Platform

} // namespace Engine
