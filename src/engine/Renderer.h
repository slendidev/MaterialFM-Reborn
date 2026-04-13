#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <smath.hpp>

#include "engine/AssetManager.h"
#include "engine/Colors.h"
#include "engine/Common.h"
#include "engine/Math.h"
#include "engine/platform/Platform.h"

namespace Engine
{

enum class TextAlignX
{
	Left,
	Center,
	Right,
};

enum class TextAlignY
{
	Top,
	Center,
	Bottom,
};

struct Renderer
{
	explicit Renderer(AssetManager &assets);
	~Renderer();

	auto start_frame() -> void;
	auto end_frame() -> void;

	auto mode_2d() -> void;

	auto clear_background(smath::Vec4 const color = Color::BLACK) -> void;
	auto draw_rectangle(smath::Vec2 const position,
	    smath::Vec2 const size,
	    smath::Vec4 const color = Color::WHITE) -> void;
	auto draw_line(smath::Vec2 const start,
	    smath::Vec2 const end,
	    float thickness = 1.0f,
	    smath::Vec4 color = Color::WHITE) -> void;
	auto draw_circle(smath::Vec2 const center,
	    float radius,
	    smath::Vec4 color = Color::WHITE,
	    int segments = 20) -> void;
	auto draw_circle_sector(smath::Vec2 const center,
	    float radius,
	    float start_radians,
	    float end_radians,
	    smath::Vec4 color = Color::WHITE,
	    int segments = 12) -> void;
	auto draw_texture(Texture const &tex,
	    smath::Vec2 const position,
	    std::optional<smath::Vec2> const size = std::nullopt,
	    smath::Vec4 color = Color::WHITE) -> void;
	auto draw_texture_ex(Texture const &tex,
	    Rect<> const src,
	    Rect<> const dst,
	    smath::Vec4 color = Color::WHITE) -> void;
	auto draw_text(std::string_view text,
	    Rect<> const box,
	    float size = 16.0f,
	    smath::Vec4 color = Color::WHITE,
	    TextAlignX align_x = TextAlignX::Left,
	    TextAlignY align_y = TextAlignY::Top,
	    std::optional<FontHandle> font = std::nullopt) -> void;
	auto draw_text_boxed(std::string_view text,
	    Rect<> const box,
	    float size = 16.0f,
	    smath::Vec4 color = Color::WHITE,
	    TextAlignX align_x = TextAlignX::Left,
	    TextAlignY align_y = TextAlignY::Top,
	    std::optional<FontHandle> font = std::nullopt) -> void;
	auto push_clip_rect(Rect<> rect) -> void;
	auto pop_clip_rect() -> void;
	auto push_scissor(Rect<> rect) -> void;
	auto pop_scissor() -> void;
	auto measure_text(std::string_view text,
	    float size = 16.0f,
	    std::optional<FontHandle> font = std::nullopt) -> smath::Vec2;

private:
	struct ShapedGlyph
	{
		uint32_t glyph_id {};
		float x_advance {};
		float y_advance {};
		float x_offset {};
		float y_offset {};
	};

	struct FontShapeCache
	{
		void *context {};
		void *font {};
		uint8_t const *font_data {};
		size_t font_size {};
	};

	struct ShapeCacheKey
	{
		uint32_t font_id {};
		uint32_t size_bits {};
		std::string text {};

		auto operator==(ShapeCacheKey const &other) const -> bool
		{
			return font_id == other.font_id && size_bits == other.size_bits
			    && text == other.text;
		}
	};

	struct ShapeCacheKeyHash
	{
		auto operator()(ShapeCacheKey const &key) const noexcept -> size_t
		{
			auto seed { std::hash<uint32_t> {}(key.font_id) };
			seed ^= std::hash<uint32_t> {}(key.size_bits) + 0x9e3779b9u
			    + (seed << 6) + (seed >> 2);
			seed ^= std::hash<std::string> {}(key.text) + 0x9e3779b9u
			    + (seed << 6) + (seed >> 2);
			return seed;
		}
	};

	struct ShapeCacheEntry
	{
		std::vector<ShapedGlyph> glyphs {};
		std::list<ShapeCacheKey>::iterator lru_it {};
	};

	auto shape_line(
	    FontHandle handle, Font const &font, std::string_view text, float size)
	    -> std::vector<ShapedGlyph> const &;
	auto ensure_shape_cache(FontHandle handle, Font const &font)
	    -> FontShapeCache *;
	auto destroy_shape_caches() -> void;

	auto flush_batch() -> void;
	auto ensure_batch_capacity(size_t vertex_count, size_t index_count) -> void;
	auto push_quad(Texture const *texture,
	    Rect<> const src,
	    Rect<> const dst,
	    smath::Vec4 color) -> void;

	std::vector<detail::GraphicsVertex> m_batch_vertices {};
	std::vector<uint16_t> m_batch_indices {};
	Texture const *m_batch_texture {};
	std::unordered_map<uint32_t, FontShapeCache> m_font_shape_cache {};
	std::unordered_map<ShapeCacheKey, ShapeCacheEntry, ShapeCacheKeyHash>
	    m_shaped_line_cache {};
	std::list<ShapeCacheKey> m_shaped_line_lru {};
	AssetManager &m_assets;
	bool m_frame_started {};
};

} // namespace Engine
