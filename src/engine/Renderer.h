#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <stb_truetype.h>

#include <smath.hpp>

#include "engine/AssetManager.h"
#include "engine/Colors.h"
#include "engine/Math.h"
#include "engine/platform/Platform.h"

namespace Engine
{

using GraphicsVertex = detail::GraphicsVertex;

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
	    std::optional<FontHandle> font = std::nullopt,
	    bool const wrap = true) -> void;
	auto push_clip_rect(Rect<> rect) -> void;
	auto pop_clip_rect() -> void;
	auto push_scissor(Rect<> rect) -> void;
	auto pop_scissor() -> void;
	auto measure_text(std::string_view text,
	    float size = 16.0f,
	    std::optional<FontHandle> font = std::nullopt) -> smath::Vec2;

	void draw_polygons(const std::vector<GraphicsVertex> &vertices,
	    const std::vector<uint16_t> &indices);

	void flush_batch();

private:
	struct ShapedGlyph
	{
		uint32_t glyph_id {};
		void *shaping_font {}; // kbts_font* from the shaper run that produced
		                       // this glyph
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
		stbtt_fontinfo stb_info {};
		bool stb_ready { false };
		float atlas_scale { 0.0f };
	};

	struct ShapeCache
	{
		struct LookupKey
		{
			uint32_t font_id {};
			uint32_t size_bits {};
			std::string_view text {};
		};

		struct Key
		{
			uint32_t font_id {};
			uint32_t size_bits {};
			std::string text {};

			struct Hash
			{
				using is_transparent = void;

				static auto hash_parts(uint32_t const font_id,
				    uint32_t const size_bits,
				    std::string_view const text) noexcept -> size_t
				{
					auto seed { std::hash<uint32_t> {}(font_id) };
					seed ^= std::hash<uint32_t> {}(size_bits) + 0x9e3779b9u
					    + (seed << 6) + (seed >> 2);
					seed ^= std::hash<std::string_view> {}(text) + 0x9e3779b9u
					    + (seed << 6) + (seed >> 2);
					return seed;
				}

				auto operator()(ShapeCache::Key const &key) const noexcept
				    -> size_t
				{
					return hash_parts(key.font_id, key.size_bits, key.text);
				}

				auto operator()(ShapeCache::LookupKey const &key) const noexcept
				    -> size_t
				{
					return hash_parts(key.font_id, key.size_bits, key.text);
				}
			};

			struct Equal
			{
				using is_transparent = void;

				static auto equals(uint32_t const lhs_font_id,
				    uint32_t const lhs_size_bits,
				    std::string_view const lhs_text,
				    uint32_t const rhs_font_id,
				    uint32_t const rhs_size_bits,
				    std::string_view const rhs_text) noexcept -> bool
				{
					return lhs_font_id == rhs_font_id
					    && lhs_size_bits == rhs_size_bits
					    && lhs_text == rhs_text;
				}

				auto operator()(ShapeCache::Key const &lhs,
				    ShapeCache::Key const &rhs) const noexcept -> bool
				{
					return equals(lhs.font_id,
					    lhs.size_bits,
					    lhs.text,
					    rhs.font_id,
					    rhs.size_bits,
					    rhs.text);
				}

				auto operator()(ShapeCache::Key const &lhs,
				    ShapeCache::LookupKey const &rhs) const noexcept -> bool
				{
					return equals(lhs.font_id,
					    lhs.size_bits,
					    lhs.text,
					    rhs.font_id,
					    rhs.size_bits,
					    rhs.text);
				}

				auto operator()(ShapeCache::LookupKey const &lhs,
				    ShapeCache::Key const &rhs) const noexcept -> bool
				{
					return equals(lhs.font_id,
					    lhs.size_bits,
					    lhs.text,
					    rhs.font_id,
					    rhs.size_bits,
					    rhs.text);
				}
			};

			auto operator==(ShapeCache::Key const &other) const -> bool
			{
				return font_id == other.font_id && size_bits == other.size_bits
				    && text == other.text;
			}
		};

		struct Entry
		{
			std::vector<ShapedGlyph> glyphs {};
			float width {};
			std::list<ShapeCache::Key>::iterator lru_it {};
		};
	};

	struct TextLayoutLine
	{
		std::vector<ShapedGlyph> const *glyphs {};
		float width {};
	};

	auto shape_line_entry(
	    FontHandle handle, Font const &font, std::string_view text, float size)
	    -> ShapeCache::Entry const *;
	auto shape_line(
	    FontHandle handle, Font const &font, std::string_view text, float size)
	    -> std::vector<ShapedGlyph> const &;
	auto ensure_shape(FontHandle handle, Font const &font) -> FontShapeCache *;
	auto destroy_shape_caches() -> void;
	static auto ensure_stb_font_static(FontShapeCache &cache, Font const &font)
	    -> bool;
	static auto ensure_glyph(
	    Font const &font, FontShapeCache &shape_cache, uint32_t const glyph_id)
	    -> Font::Glyph const *;
	static auto ensure_font_atlas(Font const &font) -> void;

	auto find_font_shape_cache(void const *shaping_font) const
	    -> std::pair<Font const *, FontShapeCache *>;
	auto find_font_by_atlas(Texture const *texture) const -> Font const *;
	auto solid_batch_texture() -> Texture const *;

	auto ensure_batch_capacity(size_t vertex_count, size_t index_count) -> void;
	auto push_quad(Texture const *texture,
	    Rect<> const src,
	    Rect<> const dst,
	    smath::Vec4 color) -> void;

	std::vector<detail::GraphicsVertex> m_batch_vertices {};
	std::vector<uint16_t> m_batch_indices {};
	Texture const *m_batch_texture {};
	std::unordered_map<uint32_t, FontShapeCache> m_font_shape_cache {};
	std::unordered_map<ShapeCache::Key,
	    ShapeCache::Entry,
	    ShapeCache::Key::Hash,
	    ShapeCache::Key::Equal>
	    m_shaped_line_cache {};
	std::list<ShapeCache::Key> m_shaped_line_lru {};
	AssetManager &m_assets;
	bool m_frame_started {};
};

} // namespace Engine
