#include "engine/Renderer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <kb_text_shape.h>
#include <stb_truetype.h>

#include "engine/AssetManager.h"

namespace Engine
{

namespace
{
constexpr size_t MIN_BATCH_VERTEX_CAPACITY { 256 };
constexpr size_t MIN_BATCH_INDEX_CAPACITY { 384 };
constexpr int FONT_ATLAS_DIMENSION { 512 };
constexpr size_t SHAPED_LINE_CACHE_MAX { 1024 };

auto grow_capacity(size_t const current, size_t const required) -> size_t
{
	auto next { std::max(current, size_t { 1 }) };
	while (next < required) {
		next *= 2;
	}
	return next;
}

auto line_height(Font const &font, float const size_px) -> float
{
	auto const upem { static_cast<float>(std::max(font.units_per_em, 1)) };
	float const units { static_cast<float>(
		font.ascent - font.descent + font.line_gap) };
	return units * size_px / upem;
}

template<typename ShapeFn>
auto shaped_line_width(std::string_view const text, ShapeFn const &shape_fn)
    -> float
{
	auto const &shaped { shape_fn(text) };
	float width {};
	for (auto const &glyph : shaped) {
		width += glyph.x_advance;
	}
	return width;
}

template<typename WidthFn>
auto append_hard_wrapped_word(std::vector<std::string> &out,
    std::string_view const word,
    float const max_width,
    WidthFn const &width_fn) -> void
{
	if (word.empty()) {
		return;
	}
	if (max_width <= 0.0f) {
		out.emplace_back(word);
		return;
	}

	std::string current {};
	for (char const ch : word) {
		std::string candidate { current };
		candidate.push_back(ch);
		auto const candidate_width { width_fn(candidate) };
		if (!current.empty() && candidate_width > max_width) {
			out.push_back(current);
			current.clear();
		}
		current.push_back(ch);
	}
	if (!current.empty()) {
		out.push_back(current);
	}
}

template<typename ShapeFn>
auto wrap_line_words(
    std::string_view const line, float const max_width, ShapeFn const &shape_fn)
    -> std::vector<std::string>
{
	if (line.empty()) {
		return { std::string {} };
	}
	if (max_width <= 0.0f) {
		return { std::string(line) };
	}

	std::vector<std::string> words {};
	std::string current_word {};
	for (char const ch : line) {
		if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
			if (!current_word.empty()) {
				words.push_back(current_word);
				current_word.clear();
			}
			continue;
		}
		current_word.push_back(ch);
	}
	if (!current_word.empty()) {
		words.push_back(current_word);
	}
	if (words.empty()) {
		return { std::string {} };
	}

	std::unordered_map<std::string, float> width_cache {};
	auto const width_of = [&](std::string_view const value) {
		auto const key { std::string(value) };
		auto it { width_cache.find(key) };
		if (it != width_cache.end()) {
			return it->second;
		}
		auto const width { shaped_line_width(value, shape_fn) };
		width_cache.emplace(key, width);
		return width;
	};

	std::vector<std::string> lines {};
	std::string current_line {};
	for (auto const &word : words) {
		std::string candidate {
			current_line.empty() ? word : (current_line + " " + word),
		};
		auto const candidate_width { width_of(candidate) };
		if (current_line.empty() && candidate_width > max_width) {
			append_hard_wrapped_word(lines, word, max_width, width_of);
			continue;
		}
		if (!current_line.empty() && candidate_width > max_width) {
			lines.push_back(current_line);
			if (width_of(word) > max_width) {
				append_hard_wrapped_word(lines, word, max_width, width_of);
				current_line.clear();
			} else {
				current_line = word;
			}
			continue;
		}
		current_line = std::move(candidate);
	}
	if (!current_line.empty()) {
		lines.push_back(current_line);
	}
	if (lines.empty()) {
		lines.emplace_back();
	}
	return lines;
}

auto float_bits(float const value) -> uint32_t
{
	uint32_t bits {};
	std::memcpy(&bits, &value, sizeof(bits));
	return bits;
}

auto ensure_glyph(Font const &font, uint32_t const glyph_id)
    -> Font::Glyph const *
{
	auto const existing { font.glyphs.find(glyph_id) };
	if (existing != font.glyphs.end()) {
		return &existing->second;
	}

	stbtt_fontinfo info {};
	if (stbtt_InitFont(&info, font.ttf_data.data(), 0) == 0) {
		return nullptr;
	}

	if (font.atlas.width <= 0 || font.atlas.height <= 0) {
		font.atlas.width = FONT_ATLAS_DIMENSION;
		font.atlas.height = FONT_ATLAS_DIMENSION;
		font.atlas.content_width = FONT_ATLAS_DIMENSION;
		font.atlas.content_height = FONT_ATLAS_DIMENSION;
		font.atlas.data.assign(static_cast<size_t>(FONT_ATLAS_DIMENSION)
		        * static_cast<size_t>(FONT_ATLAS_DIMENSION),
		    0);
		font.atlas_pen_x = 1;
		font.atlas_pen_y = 1;
		font.atlas_row_height = 0;
	}

	auto const scale { stbtt_ScaleForPixelHeight(
		&info, font.atlas_base_size_px) };
	int x0 {};
	int y0 {};
	int x1 {};
	int y1 {};
	stbtt_GetGlyphBitmapBox(
	    &info, static_cast<int>(glyph_id), scale, scale, &x0, &y0, &x1, &y1);
	auto const width { std::max(0, x1 - x0) };
	auto const height { std::max(0, y1 - y0) };

	if (width == 0 || height == 0) {
		auto [it, _] { font.glyphs.emplace(glyph_id,
			Font::Glyph {
			    .u0 = 0.0f,
			    .v0 = 0.0f,
			    .u1 = 0.0f,
			    .v1 = 0.0f,
			    .x0 = x0,
			    .y0 = y0,
			    .x1 = x1,
			    .y1 = y1,
			}) };
		return &it->second;
	}

	constexpr int padding { 1 };
	auto padded_width { width + padding };
	if (font.atlas_pen_x + padded_width >= font.atlas.content_width) {
		font.atlas_pen_x = 1;
		font.atlas_pen_y += font.atlas_row_height + padding;
		font.atlas_row_height = 0;
	}
	if (font.atlas_pen_y + height + padding >= font.atlas.content_height) {
		return nullptr;
	}

	auto const dst_x { font.atlas_pen_x };
	auto const dst_y { font.atlas_pen_y };
	font.atlas_pen_x += padded_width;
	font.atlas_row_height = std::max(font.atlas_row_height, height);

	std::vector<uint8_t> bitmap(static_cast<size_t>(width * height));
	stbtt_MakeGlyphBitmap(&info,
	    bitmap.data(),
	    width,
	    height,
	    width,
	    scale,
	    scale,
	    static_cast<int>(glyph_id));

	for (int row { 0 }; row < height; ++row) {
		for (int col { 0 }; col < width; ++col) {
			auto const alpha {
				static_cast<float>(
				    bitmap[static_cast<size_t>(row * width + col)])
				/ 255.0f
			};
			auto const packed { smath::pack_unorm4x8(
				smath::Vec4 { 1.0f, 1.0f, 1.0f, alpha }) };
			font.atlas.data[static_cast<size_t>(dst_y + row)
			        * static_cast<size_t>(font.atlas.content_width)
			    + static_cast<size_t>(dst_x + col)]
			    = packed;
		}
	}
	font.atlas.mark_dirty();

	auto [it, _] { font.glyphs.emplace(glyph_id,
		Font::Glyph {
		    .u0 = static_cast<float>(dst_x),
		    .v0 = static_cast<float>(dst_y),
		    .u1 = static_cast<float>(dst_x + width),
		    .v1 = static_cast<float>(dst_y + height),
		    .x0 = x0,
		    .y0 = y0,
		    .x1 = x1,
		    .y1 = y1,
		}) };
	return &it->second;
}
} // namespace

Renderer::Renderer(AssetManager &assets) : m_assets(assets)
{
	Platform::renderer_create();
}

Renderer::~Renderer()
{
	destroy_shape_caches();

	if (m_frame_started) {
		flush_batch();
		Platform::renderer_end_frame();
		m_frame_started = false;
	}
	Platform::renderer_destroy();
}

auto Renderer::ensure_shape_cache(FontHandle const handle, Font const &font)
    -> FontShapeCache *
{
	if (handle.id == 0xFFFFFFFFu || font.ttf_data.empty()) {
		return nullptr;
	}

	auto &entry { m_font_shape_cache[handle.id] };
	auto const *font_data { font.ttf_data.data() };
	auto const font_size { font.ttf_data.size() };
	if (entry.context != nullptr
	    && (entry.font_data != font_data || entry.font_size != font_size)) {
		kbts_DestroyShapeContext(
		    static_cast<kbts_shape_context *>(entry.context));
		entry = FontShapeCache {};
	}

	if (entry.context == nullptr) {
		auto *ctx { kbts_CreateShapeContext(nullptr, nullptr) };
		if (ctx == nullptr) {
			m_font_shape_cache.erase(handle.id);
			return nullptr;
		}
		auto *shape_font { kbts_ShapePushFontFromMemory(ctx,
			const_cast<uint8_t *>(font.ttf_data.data()),
			static_cast<int>(font.ttf_data.size()),
			0) };
		if (shape_font == nullptr) {
			kbts_DestroyShapeContext(ctx);
			m_font_shape_cache.erase(handle.id);
			return nullptr;
		}

		entry.context = ctx;
		entry.font = shape_font;
		entry.font_data = font_data;
		entry.font_size = font_size;
	}

	return &entry;
}

auto Renderer::shape_line(FontHandle const handle,
    Font const &font,
    std::string_view const text,
    float const size_px) -> std::vector<ShapedGlyph> const &
{
	static std::vector<ShapedGlyph> const empty {};
	if (text.empty() || font.ttf_data.empty()) {
		return empty;
	}

	ShapeCacheKey key {
		.font_id = handle.id,
		.size_bits = float_bits(size_px),
		.text = std::string(text),
	};
	auto const cached_it { m_shaped_line_cache.find(key) };
	if (cached_it != m_shaped_line_cache.end()) {
		m_shaped_line_lru.splice(m_shaped_line_lru.end(),
		    m_shaped_line_lru,
		    cached_it->second.lru_it);
		return cached_it->second.glyphs;
	}

	auto *cache { ensure_shape_cache(handle, font) };
	if (cache == nullptr || cache->context == nullptr) {
		return empty;
	}
	auto *ctx { static_cast<kbts_shape_context *>(cache->context) };
	std::vector<ShapedGlyph> shaped {};

	kbts_ShapeBegin(ctx, KBTS_DIRECTION_DONT_KNOW, KBTS_LANGUAGE_DONT_KNOW);
	kbts_ShapeUtf8(ctx,
	    text.data(),
	    static_cast<int>(text.size()),
	    KBTS_USER_ID_GENERATION_MODE_CODEPOINT_INDEX);
	kbts_ShapeEnd(ctx);

	auto const scale {
		size_px / static_cast<float>(std::max(font.units_per_em, 1)),
	};
	kbts_run run {};
	while (kbts_ShapeRun(ctx, &run) != 0) {
		kbts_glyph *glyph {};
		while (kbts_GlyphIteratorNext(&run.Glyphs, &glyph) != 0) {
			shaped.push_back(ShapedGlyph {
			    .glyph_id = glyph->Id,
			    .x_advance = static_cast<float>(glyph->AdvanceX) * scale,
			    .y_advance = static_cast<float>(glyph->AdvanceY) * scale,
			    .x_offset = static_cast<float>(glyph->OffsetX) * scale,
			    .y_offset = static_cast<float>(glyph->OffsetY) * scale,
			});
		}
	}

	m_shaped_line_lru.push_back(std::move(key));
	auto const lru_it { std::prev(m_shaped_line_lru.end()) };
	auto [it, _] { m_shaped_line_cache.emplace(*lru_it,
		Renderer::ShapeCacheEntry {
		    .glyphs = std::move(shaped),
		    .lru_it = lru_it,
		}) };
	bool inserted_entry_erased {};
	while (m_shaped_line_cache.size() > SHAPED_LINE_CACHE_MAX
	    && !m_shaped_line_lru.empty()) {
		auto const oldest { m_shaped_line_lru.front() };
		if (it != m_shaped_line_cache.end() && oldest == it->first) {
			inserted_entry_erased = true;
		}
		m_shaped_line_lru.pop_front();
		m_shaped_line_cache.erase(oldest);
	}
	if (inserted_entry_erased || it == m_shaped_line_cache.end()) {
		return empty;
	}
	return it->second.glyphs;
}

auto Renderer::destroy_shape_caches() -> void
{
	for (auto &[_, cache] : m_font_shape_cache) {
		if (cache.context != nullptr) {
			kbts_DestroyShapeContext(
			    static_cast<kbts_shape_context *>(cache.context));
		}
	}
	m_font_shape_cache.clear();
	m_shaped_line_cache.clear();
	m_shaped_line_lru.clear();
}

auto Renderer::start_frame() -> void
{
	Platform::renderer_begin_frame();
	m_batch_texture = nullptr;
	m_batch_vertices.clear();
	m_batch_indices.clear();
	m_frame_started = true;
}

auto Renderer::end_frame() -> void
{
	flush_batch();
	Platform::renderer_end_frame();
	m_frame_started = false;
}

auto Renderer::mode_2d() -> void
{
	Platform::renderer_mode_2d();
}

auto Renderer::clear_background(smath::Vec4 const color) -> void
{
	flush_batch();
	Platform::renderer_clear(color);
}

auto Renderer::draw_rectangle(
    smath::Vec2 const position, smath::Vec2 const size, smath::Vec4 const color)
    -> void
{
	push_quad(nullptr,
	    Rect<> {
	        .position = smath::Vec2 { 0.0f, 0.0f },
	        .size = smath::Vec2 { 1.0f, 1.0f },
	    },
	    Rect<> { .position = position, .size = size },
	    color);
}

auto Renderer::draw_line(smath::Vec2 const start,
    smath::Vec2 const end,
    float const thickness,
    smath::Vec4 const color) -> void
{
	auto const dx { end.x() - start.x() };
	auto const dy { end.y() - start.y() };
	auto const length { std::sqrt(dx * dx + dy * dy) };
	auto const half_thickness { std::max(0.5f, thickness * 0.5f) };

	if (length <= 0.0001f) {
		draw_rectangle(start - smath::Vec2 { half_thickness, half_thickness },
		    smath::Vec2 { half_thickness * 2.0f, half_thickness * 2.0f },
		    color);
		return;
	}

	if (m_batch_texture != nullptr) {
		flush_batch();
		m_batch_texture = nullptr;
	}

	ensure_batch_capacity(4, 6);
	auto const color_u32 { smath::pack_unorm4x8(color) };
	auto const base_index { static_cast<uint16_t>(m_batch_vertices.size()) };

	auto const nx { -dy / length * half_thickness };
	auto const ny { dx / length * half_thickness };
	auto const p0 { smath::Vec2 { start.x() + nx, start.y() + ny } };
	auto const p1 { smath::Vec2 { end.x() + nx, end.y() + ny } };
	auto const p2 { smath::Vec2 { start.x() - nx, start.y() - ny } };
	auto const p3 { smath::Vec2 { end.x() - nx, end.y() - ny } };

	m_batch_vertices.push_back(detail::GraphicsVertex {
	    .u = 0.0f,
	    .v = 0.0f,
	    .color = color_u32,
	    .x = p0.x(),
	    .y = p0.y(),
	    .z = 0.0f,
	});
	m_batch_vertices.push_back(detail::GraphicsVertex {
	    .u = 0.0f,
	    .v = 0.0f,
	    .color = color_u32,
	    .x = p1.x(),
	    .y = p1.y(),
	    .z = 0.0f,
	});
	m_batch_vertices.push_back(detail::GraphicsVertex {
	    .u = 0.0f,
	    .v = 0.0f,
	    .color = color_u32,
	    .x = p2.x(),
	    .y = p2.y(),
	    .z = 0.0f,
	});
	m_batch_vertices.push_back(detail::GraphicsVertex {
	    .u = 0.0f,
	    .v = 0.0f,
	    .color = color_u32,
	    .x = p3.x(),
	    .y = p3.y(),
	    .z = 0.0f,
	});

	m_batch_indices.push_back(base_index);
	m_batch_indices.push_back(static_cast<uint16_t>(base_index + 1));
	m_batch_indices.push_back(static_cast<uint16_t>(base_index + 2));
	m_batch_indices.push_back(static_cast<uint16_t>(base_index + 1));
	m_batch_indices.push_back(static_cast<uint16_t>(base_index + 3));
	m_batch_indices.push_back(static_cast<uint16_t>(base_index + 2));
}

auto Renderer::draw_circle(smath::Vec2 const center,
    float const radius,
    smath::Vec4 const color,
    int const segments) -> void
{
	constexpr float PI { 3.14159265358979323846f };
	draw_circle_sector(center, radius, 0.0f, PI * 2.0f, color, segments);
}

auto Renderer::draw_circle_sector(smath::Vec2 const center,
    float const radius,
    float const start_radians,
    float const end_radians,
    smath::Vec4 const color,
    int const segments) -> void
{
	if (radius <= 0.0f) {
		return;
	}

	auto const sweep { end_radians - start_radians };
	if (std::abs(sweep) <= 0.0001f) {
		return;
	}

	if (m_batch_texture != nullptr) {
		flush_batch();
		m_batch_texture = nullptr;
	}

	auto const seg_count { std::max(3, segments) };
	auto const needed_vertices { static_cast<size_t>(seg_count + 2) };
	auto const needed_indices { static_cast<size_t>(seg_count * 3) };

	if (m_batch_vertices.size() + needed_vertices
	    > static_cast<size_t>(std::numeric_limits<uint16_t>::max())) {
		flush_batch();
	}

	ensure_batch_capacity(needed_vertices, needed_indices);

	auto const color_u32 { smath::pack_unorm4x8(color) };
	auto const base { static_cast<uint16_t>(m_batch_vertices.size()) };

	m_batch_vertices.push_back(detail::GraphicsVertex {
	    .u = 0.0f,
	    .v = 0.0f,
	    .color = color_u32,
	    .x = center.x(),
	    .y = center.y(),
	    .z = 0.0f,
	});

	for (int i {}; i <= seg_count; ++i) {
		auto const t {
			static_cast<float>(i) / static_cast<float>(seg_count),
		};
		auto const angle { start_radians + sweep * t };
		auto const x { center.x() + std::cos(angle) * radius };
		auto const y { center.y() + std::sin(angle) * radius };
		m_batch_vertices.push_back(detail::GraphicsVertex {
		    .u = 0.0f,
		    .v = 0.0f,
		    .color = color_u32,
		    .x = x,
		    .y = y,
		    .z = 0.0f,
		});
	}

	for (int i {}; i < seg_count; ++i) {
		m_batch_indices.push_back(base);
		m_batch_indices.push_back(static_cast<uint16_t>(base + i + 1));
		m_batch_indices.push_back(static_cast<uint16_t>(base + i + 2));
	}
}

auto Renderer::draw_texture(Texture const &tex,
    smath::Vec2 const position,
    std::optional<smath::Vec2> const size,
    smath::Vec4 const color) -> void
{
	smath::Vec2 dst_size;
	if (!size) {
		dst_size = smath::Vec2 {
			static_cast<float>(tex.content_width),
			static_cast<float>(tex.content_height),
		};
	} else {
		dst_size = *size;
	}

	draw_texture_ex(tex,
	    Rect<> {
	        .position = smath::Vec2 { 0.0f, 0.0f },
	        .size = smath::Vec2 {
	            static_cast<float>(tex.content_width),
	            static_cast<float>(tex.content_height),
	        },
	    },
	    Rect<> { .position = position, .size = dst_size },
	    color);
}

auto Renderer::draw_texture_ex(Texture const &tex,
    Rect<> const src,
    Rect<> const dst,
    smath::Vec4 const color) -> void
{
	push_quad(&tex, src, dst, color);
}

auto Renderer::draw_text(std::string_view const text,
    Rect<> const box,
    float const size,
    smath::Vec4 const color,
    TextAlignX const align_x,
    TextAlignY const align_y,
    std::optional<FontHandle> const font_handle) -> void
{
	auto handle { font_handle.value_or(m_assets.active_font_handle()) };
	auto const *font { m_assets.font(handle) };
	if (font == nullptr || text.empty() || size <= 0.0f) {
		return;
	}

	auto const lh { line_height(*font, size) };
	if (lh <= 0.0f) {
		return;
	}
	auto const upem { static_cast<float>(std::max(font->units_per_em, 1)) };
	auto const asc_px { static_cast<float>(font->ascent) * size / upem };
	auto const desc_px { static_cast<float>(-font->descent) * size / upem };

	auto const shape_fn {
		[&](std::string_view const line) -> std::vector<ShapedGlyph> const & {
		    return shape_line(handle, *font, line, size);
		},
	};
	std::unordered_map<std::string, float> width_cache {};
	auto const width_of = [&](std::string_view const line) {
		auto const key { std::string(line) };
		auto it { width_cache.find(key) };
		if (it != width_cache.end()) {
			return it->second;
		}
		auto const width { shaped_line_width(line, shape_fn) };
		width_cache.emplace(key, width);
		return width;
	};

	std::vector<std::string> lines {};
	size_t line_start {};
	while (line_start <= text.size()) {
		auto const line_end { text.find('\n', line_start) };
		auto const segment_length { line_end == std::string_view::npos
			    ? text.size() - line_start
			    : line_end - line_start };
		auto const source_line { text.substr(line_start, segment_length) };
		auto wrapped {
			wrap_line_words(source_line, box.size.x(), shape_fn),
		};
		for (auto &segment : wrapped) {
			lines.push_back(std::move(segment));
		}
		if (line_end == std::string_view::npos) {
			break;
		}
		line_start = line_end + 1;
	}

	std::vector<float> line_widths {};
	line_widths.reserve(lines.size());
	for (auto const &line : lines) {
		auto const width { width_of(line) };
		line_widths.push_back(width);
	}

	auto const total_height {
		(asc_px + desc_px)
		    + std::max(0.0f, static_cast<float>(lines.size() - 1)) * lh,
	};
	auto start_y { box.position.y() };
	if (align_y == TextAlignY::Center) {
		start_y += (box.size.y() - total_height) * 0.5f;
	} else if (align_y == TextAlignY::Bottom) {
		start_y += box.size.y() - total_height;
	}

	auto const atlas_scale { size / font->atlas_base_size_px };
	for (size_t line_index {}; line_index < lines.size(); ++line_index) {
		auto const &line { lines[line_index] };
		auto const &shaped { shape_fn(line) };
		auto const line_width { line_widths[line_index] };

		auto pen_x { box.position.x() };
		if (align_x == TextAlignX::Center) {
			pen_x += (box.size.x() - line_width) * 0.5f;
		} else if (align_x == TextAlignX::Right) {
			pen_x += box.size.x() - line_width;
		}
		auto pen_y { start_y + asc_px + static_cast<float>(line_index) * lh };

		for (auto const &glyph : shaped) {
			auto const *cached { ensure_glyph(*font, glyph.glyph_id) };
			if (cached != nullptr && cached->u1 > cached->u0
			    && cached->v1 > cached->v0) {
				auto const x { pen_x + glyph.x_offset
					+ static_cast<float>(cached->x0) * atlas_scale };
				auto const y { pen_y + glyph.y_offset
					+ static_cast<float>(cached->y0) * atlas_scale };
				push_quad(&font->atlas,
				    Rect<> {
				        .position = smath::Vec2 { cached->u0, cached->v0 },
				        .size = smath::Vec2 {
				            cached->u1 - cached->u0,
				            cached->v1 - cached->v0,
				        },
				    },
				    Rect<> {
				        .position = smath::Vec2 { x, y },
				        .size = smath::Vec2 {
				            (cached->u1 - cached->u0) * atlas_scale,
				            (cached->v1 - cached->v0) * atlas_scale,
				        },
				    },
				    color);
			}

			pen_x += glyph.x_advance;
			pen_y += glyph.y_advance;
		}
	}
}

auto Renderer::draw_text_boxed(std::string_view const text,
    Rect<> const box,
    float const size,
    smath::Vec4 const color,
    TextAlignX const align_x,
    TextAlignY const align_y,
    std::optional<FontHandle> const font_handle) -> void
{
	draw_text(text, box, size, color, align_x, align_y, font_handle);
}

auto Renderer::push_clip_rect(Rect<> const rect) -> void
{
	push_scissor(rect);
}

auto Renderer::pop_clip_rect() -> void
{
	pop_scissor();
}

auto Renderer::push_scissor(Rect<> const rect) -> void
{
	flush_batch();
	Platform::renderer_push_scissor(rect);
}

auto Renderer::pop_scissor() -> void
{
	flush_batch();
	Platform::renderer_pop_scissor();
}

auto Renderer::measure_text(std::string_view const text,
    float const size,
    std::optional<FontHandle> const font_handle) -> smath::Vec2
{
	auto handle { font_handle.value_or(m_assets.active_font_handle()) };
	auto const *font { m_assets.font(handle) };
	if (font == nullptr || text.empty() || size <= 0.0f) {
		return smath::Vec2 { 0.0f, 0.0f };
	}

	auto const lh { line_height(*font, size) };
	auto const upem { static_cast<float>(std::max(font->units_per_em, 1)) };
	auto const asc_px { static_cast<float>(font->ascent) * size / upem };
	auto const desc_px { static_cast<float>(-font->descent) * size / upem };
	float max_width {};
	float total_height { asc_px + desc_px };

	size_t line_start {};
	while (line_start <= text.size()) {
		auto const line_end { text.find('\n', line_start) };
		auto const segment_length { line_end == std::string_view::npos
			    ? text.size() - line_start
			    : line_end - line_start };
		auto const line_text { text.substr(line_start, segment_length) };
		auto const &shaped { shape_line(handle, *font, line_text, size) };

		float width {};
		for (auto const &glyph : shaped) {
			width += glyph.x_advance;
		}
		max_width = std::max(max_width, width);

		if (line_end == std::string_view::npos) {
			break;
		}
		total_height += lh;
		line_start = line_end + 1;
	}

	return smath::Vec2 { max_width, total_height };
}

auto Renderer::flush_batch() -> void
{
	if (m_batch_indices.empty()) {
		return;
	}

	Platform::renderer_submit_batch(
	    m_batch_texture, m_batch_vertices, m_batch_indices);
	m_batch_vertices.clear();
	m_batch_indices.clear();
}

auto Renderer::ensure_batch_capacity(
    size_t const vertex_count, size_t const index_count) -> void
{
	auto const required_vertices { m_batch_vertices.size() + vertex_count };
	auto const required_indices { m_batch_indices.size() + index_count };

	if (required_vertices > m_batch_vertices.capacity()) {
		auto const new_capacity { grow_capacity(
			std::max(m_batch_vertices.capacity(), MIN_BATCH_VERTEX_CAPACITY),
			required_vertices) };
		m_batch_vertices.reserve(new_capacity);
	}

	if (required_indices > m_batch_indices.capacity()) {
		auto const new_capacity { grow_capacity(
			std::max(m_batch_indices.capacity(), MIN_BATCH_INDEX_CAPACITY),
			required_indices) };
		m_batch_indices.reserve(new_capacity);
	}
}

auto Renderer::push_quad(Texture const *const texture,
    Rect<> const src,
    Rect<> const dst,
    smath::Vec4 const color) -> void
{
	if (m_batch_texture != texture) {
		flush_batch();
		m_batch_texture = texture;
	}

	if (m_batch_vertices.size() + 4
	    > static_cast<size_t>(std::numeric_limits<uint16_t>::max())) {
		flush_batch();
	}

	ensure_batch_capacity(4, 6);

	auto const color_u32 { smath::pack_unorm4x8(color) };
	auto const base_index { static_cast<uint16_t>(m_batch_vertices.size()) };

	float u0 {};
	float v0 {};
	float u1 { 1.0f };
	float v1 { 1.0f };
	if (texture != nullptr) {
		auto const tex_w { static_cast<float>(texture->content_width) };
		auto const tex_h { static_cast<float>(texture->content_height) };
		u0 = std::clamp(src.position.x(), 0.0f, tex_w);
		v0 = std::clamp(src.position.y(), 0.0f, tex_h);
		u1 = std::clamp(src.position.x() + src.size.x(), 0.0f, tex_w);
		v1 = std::clamp(src.position.y() + src.size.y(), 0.0f, tex_h);
	}

	auto const x0 { dst.position.x() };
	auto const y0 { dst.position.y() };
	auto const x1 { dst.position.x() + dst.size.x() };
	auto const y1 { dst.position.y() + dst.size.y() };

	m_batch_vertices.push_back(detail::GraphicsVertex {
	    .u = u0,
	    .v = v0,
	    .color = color_u32,
	    .x = x0,
	    .y = y0,
	    .z = 0.0f,
	});
	m_batch_vertices.push_back(detail::GraphicsVertex {
	    .u = u1,
	    .v = v0,
	    .color = color_u32,
	    .x = x1,
	    .y = y0,
	    .z = 0.0f,
	});
	m_batch_vertices.push_back(detail::GraphicsVertex {
	    .u = u0,
	    .v = v1,
	    .color = color_u32,
	    .x = x0,
	    .y = y1,
	    .z = 0.0f,
	});
	m_batch_vertices.push_back(detail::GraphicsVertex {
	    .u = u1,
	    .v = v1,
	    .color = color_u32,
	    .x = x1,
	    .y = y1,
	    .z = 0.0f,
	});

	m_batch_indices.push_back(base_index);
	m_batch_indices.push_back(static_cast<uint16_t>(base_index + 1));
	m_batch_indices.push_back(static_cast<uint16_t>(base_index + 2));
	m_batch_indices.push_back(static_cast<uint16_t>(base_index + 1));
	m_batch_indices.push_back(static_cast<uint16_t>(base_index + 3));
	m_batch_indices.push_back(static_cast<uint16_t>(base_index + 2));
}

} // namespace Engine
