#include "engine/Renderer.h"

#include <algorithm>
#include <bit>
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

template<typename EntryFn, typename EmitFn>
auto append_hard_wrapped_word(std::string_view const word,
    float const max_width,
    EntryFn const &entry_fn,
    EmitFn const &emit_fn) -> void
{
	if (word.empty()) {
		return;
	}
	if (max_width <= 0.0f) {
		auto const *entry { entry_fn(word) };
		emit_fn(word, entry, entry != nullptr ? entry->width : 0.0f);
		return;
	}

	auto const *entry { entry_fn(word) };
	if (entry == nullptr || entry->glyphs.empty()) {
		emit_fn(word, entry, entry != nullptr ? entry->width : 0.0f);
		return;
	}
	auto const &glyphs { entry->glyphs };

	size_t segment_start = 0;
	size_t segment_len = 0;
	float segment_width = 0.0f;

	for (size_t i = 0; i < glyphs.size() && i < word.size(); ++i) {
		float const next_width = segment_width + glyphs[i].x_advance;
		if (segment_len > 0 && next_width > max_width) {
			auto const segment { word.substr(segment_start, segment_len) };
			auto const *segment_entry { entry_fn(segment) };
			emit_fn(segment,
			    segment_entry,
			    segment_entry != nullptr ? segment_entry->width
			                             : segment_width);
			segment_start += segment_len;
			segment_len = 0;
			segment_width = 0.0f;
		}
		segment_len += 1;
		segment_width += glyphs[i].x_advance;
	}

	if (segment_len > 0) {
		auto const segment { word.substr(segment_start, segment_len) };
		auto const *segment_entry { entry_fn(segment) };
		emit_fn(segment,
		    segment_entry,
		    segment_entry != nullptr ? segment_entry->width : segment_width);
	}
}

template<typename EntryFn, typename EmitFn>
auto wrap_line_words(std::string_view const line,
    float const max_width,
    EntryFn const &entry_fn,
    EmitFn const &emit_fn) -> void
{
	using EntryPtr = decltype(entry_fn(std::string_view {}));

	auto emitted_any { false };
	auto emit_line { [&](std::string_view const wrapped_line,
		                 EntryPtr const entry,
		                 float const width) {
		emit_fn(wrapped_line, entry, width);
		emitted_any = true;
	} };

	if (line.empty()) {
		EntryPtr empty_entry { nullptr };
		emit_fn({}, empty_entry, 0.0f);
		emitted_any = true;
		return;
	}
	if (max_width <= 0.0f) {
		auto const *entry { entry_fn(line) };
		emit_fn(line, entry, entry != nullptr ? entry->width : 0.0f);
		return;
	}

	auto is_space = [](unsigned char c) {
		return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'
		    || c == '\v';
	};

	auto const *space_entry { entry_fn(" ") };
	auto const space_width {
		space_entry != nullptr ? space_entry->width : 0.0f,
	};

	std::string current_line;
	current_line.reserve(line.size());

	float current_width = 0.0f;
	auto flush_current_line { [&]() {
		auto const view { std::string_view(current_line) };
		auto const *entry { entry_fn(view) };
		emit_line(view, entry, entry != nullptr ? entry->width : current_width);
		current_line.clear();
		current_width = 0.0f;
	} };

	size_t i = 0;
	while (i < line.size()) {
		while (
		    i < line.size() && is_space(static_cast<unsigned char>(line[i]))) {
			++i;
		}
		if (i >= line.size()) {
			break;
		}

		size_t start = i;
		while (
		    i < line.size() && !is_space(static_cast<unsigned char>(line[i]))) {
			++i;
		}

		std::string_view word = line.substr(start, i - start);
		auto const *word_entry { entry_fn(word) };
		float const word_width {
			word_entry != nullptr ? word_entry->width : 0.0f,
		};

		if (current_line.empty()) {
			if (word_width > max_width) {
				append_hard_wrapped_word(word, max_width, entry_fn, emit_line);
			} else {
				current_line.append(word.data(), word.size());
				current_width = word_width;
			}
			continue;
		}

		float const candidate_width = current_width + space_width + word_width;
		if (candidate_width > max_width) {
			flush_current_line();

			if (word_width > max_width) {
				append_hard_wrapped_word(word, max_width, entry_fn, emit_line);
			} else {
				current_line.append(word.data(), word.size());
				current_width = word_width;
			}
			continue;
		}

		current_line.push_back(' ');
		current_line.append(word.data(), word.size());
		current_width = candidate_width;
	}

	if (!current_line.empty()) {
		flush_current_line();
	}
	if (!emitted_any) {
		emit_line({}, nullptr, 0.0f);
	}
}

} // namespace

auto Renderer::ensure_font_atlas(Font const &font) -> void
{
	if (font.atlas.width > 0 && font.atlas.height > 0) {
		font.atlas.can_be_solid_source = true;
		if (!font.atlas.data.empty()) {
			font.atlas.data[0] = 0xFFFFFFFFu;
		}
		return;
	}

	font.atlas.width = FONT_ATLAS_DIMENSION;
	font.atlas.height = FONT_ATLAS_DIMENSION;
	font.atlas.content_width = FONT_ATLAS_DIMENSION;
	font.atlas.content_height = FONT_ATLAS_DIMENSION;
	font.atlas.data.assign(static_cast<size_t>(FONT_ATLAS_DIMENSION)
	        * static_cast<size_t>(FONT_ATLAS_DIMENSION),
	    0);
	font.atlas.can_be_solid_source = true;
	font.atlas.data[0] = 0xFFFFFFFFu;
	font.atlas_pen_x = 1;
	font.atlas_pen_y = 1;
	font.atlas_row_height = 0;
	font.atlas.mark_dirty();
}

auto Renderer::ensure_glyph(
    Font const &font, FontShapeCache &shape_cache, uint32_t const glyph_id)
    -> Font::Glyph const *
{
	auto const existing { font.glyphs.find(glyph_id) };
	if (existing != font.glyphs.end()) {
		return &existing->second;
	}

	if (!ensure_stb_font_static(shape_cache, font)) {
		printf("[TEXT] ensure_glyph: stb init failed for glyph %lu (font path: "
		       "%s)\n",
		    static_cast<unsigned long>(glyph_id),
		    font.path.c_str());
		return nullptr;
	}
	auto const *info { &shape_cache.stb_info };

	ensure_font_atlas(font);

	auto const scale { shape_cache.atlas_scale };
	int x0 {};
	int y0 {};
	int x1 {};
	int y1 {};
	stbtt_GetGlyphBitmapBox(
	    info, static_cast<int>(glyph_id), scale, scale, &x0, &y0, &x1, &y1);
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
		printf("[TEXT] ensure_glyph: atlas full for glyph %lu (font: %s)\n",
		    static_cast<unsigned long>(glyph_id),
		    font.path.c_str());
		return nullptr;
	}

	auto const dst_x { font.atlas_pen_x };
	auto const dst_y { font.atlas_pen_y };
	font.atlas_pen_x += padded_width;
	font.atlas_row_height = std::max(font.atlas_row_height, height);

	std::vector<uint8_t> bitmap(static_cast<size_t>(width * height));
	stbtt_MakeGlyphBitmap(info,
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
	printf("[TEXT] ensure_glyph: added glyph %lu to atlas at (%d,%d) %dx%d "
	       "(font: %s)\n",
	    static_cast<unsigned long>(glyph_id),
	    dst_x,
	    dst_y,
	    width,
	    height,
	    font.path.c_str());
	return &it->second;
}

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

auto Renderer::ensure_shape(FontHandle const handle, Font const &font)
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

auto Renderer::shape_line_entry(FontHandle const handle,
    Font const &font,
    std::string_view const text,
    float const size_px) -> ShapeCache::Entry const *
{
	if (text.empty() || font.ttf_data.empty()) {
		return nullptr;
	}

	ShapeCache::LookupKey lookup_key {
		.font_id = handle.id,
		.size_bits = std::bit_cast<uint32_t>(size_px),
		.text = text,
	};
	auto const cached_it { m_shaped_line_cache.find(lookup_key) };
	if (cached_it != m_shaped_line_cache.end()) {
		m_shaped_line_lru.splice(m_shaped_line_lru.end(),
		    m_shaped_line_lru,
		    cached_it->second.lru_it);
		return &cached_it->second;
	}

	auto *cache { ensure_shape(handle, font) };
	if (cache == nullptr || cache->context == nullptr) {
		return nullptr;
	}
	auto *ctx { static_cast<kbts_shape_context *>(cache->context) };
	std::vector<ShapedGlyph> shaped {};
	shaped.reserve(text.size());
	float width {};

	kbts_ShapeBegin(ctx, KBTS_DIRECTION_DONT_KNOW, KBTS_LANGUAGE_DONT_KNOW);
	kbts_ShapeUtf8(ctx,
	    text.data(),
	    static_cast<int>(text.size()),
	    KBTS_USER_ID_GENERATION_MODE_CODEPOINT_INDEX);
	kbts_ShapeEnd(ctx);

	kbts_run run {};
	while (kbts_ShapeRun(ctx, &run) != 0) {
		auto [run_font, _] = find_font_shape_cache(run.Font);
		auto const run_upem = static_cast<float>(std::max(
		    run_font != nullptr ? run_font->units_per_em : font.units_per_em,
		    1));
		auto const run_scale = size_px / run_upem;

		kbts_glyph *glyph {};
		while (kbts_GlyphIteratorNext(&run.Glyphs, &glyph) != 0) {
			auto const x_advance
			    = static_cast<float>(glyph->AdvanceX) * run_scale;
			shaped.push_back(ShapedGlyph {
			    .glyph_id = glyph->Id,
			    .shaping_font = run.Font,
			    .x_advance = x_advance,
			    .y_advance = static_cast<float>(glyph->AdvanceY) * run_scale,
			    .x_offset = static_cast<float>(glyph->OffsetX) * run_scale,
			    .y_offset = static_cast<float>(glyph->OffsetY) * run_scale,
			});
			width += x_advance;
		}
	}

	m_shaped_line_lru.emplace_back(ShapeCache::Key {
	    .font_id = lookup_key.font_id,
	    .size_bits = lookup_key.size_bits,
	    .text = std::string(text),
	});
	auto const lru_it { std::prev(m_shaped_line_lru.end()) };
	auto [it, _] { m_shaped_line_cache.emplace(*lru_it,
		Renderer::ShapeCache::Entry {
		    .glyphs = std::move(shaped),
		    .width = width,
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
		return nullptr;
	}
	return &it->second;
}

auto Renderer::shape_line(FontHandle const handle,
    Font const &font,
    std::string_view const text,
    float const size_px) -> std::vector<ShapedGlyph> const &
{
	static std::vector<ShapedGlyph> const empty {};
	auto const *entry { shape_line_entry(handle, font, text, size_px) };
	if (entry == nullptr) {
		return empty;
	}
	return entry->glyphs;
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

auto Renderer::ensure_stb_font_static(FontShapeCache &cache, Font const &font)
    -> bool
{
	if (cache.stb_ready) {
		return true;
	}
	if (font.ttf_data.empty()) {
		return false;
	}
	if (stbtt_InitFont(&cache.stb_info, font.ttf_data.data(), 0) == 0) {
		return false;
	}
	cache.atlas_scale
	    = stbtt_ScaleForPixelHeight(&cache.stb_info, font.atlas_base_size_px);
	cache.stb_ready = true;
	return true;
}

auto Renderer::find_font_shape_cache(void const *shaping_font) const
    -> std::pair<Font const *, FontShapeCache *>
{
	for (auto &[handle_id, cache] : m_font_shape_cache) {
		if (cache.font == shaping_font) {
			auto const *engine_font { m_assets.font(FontHandle { handle_id }) };
			if (engine_font != nullptr) {
				return { engine_font, const_cast<FontShapeCache *>(&cache) };
			}
		}
	}
	return { nullptr, nullptr };
}

auto Renderer::find_font_by_atlas(Texture const *const texture) const
    -> Font const *
{
	if (texture == nullptr) {
		return nullptr;
	}

	for (auto const &[handle_id, _] : m_font_shape_cache) {
		auto const *font { m_assets.font(FontHandle { handle_id }) };
		if (font != nullptr && &font->atlas == texture) {
			return font;
		}
	}

	auto const active_handle { m_assets.active_font_handle() };
	auto const *active_font { m_assets.font(active_handle) };
	if (active_font != nullptr && &active_font->atlas == texture) {
		return active_font;
	}

	return nullptr;
}

auto Renderer::solid_batch_texture() -> Texture const *
{
	if (m_batch_texture != nullptr) {
		auto const *font { find_font_by_atlas(m_batch_texture) };
		if (font != nullptr) {
			ensure_font_atlas(*font);
			return &font->atlas;
		}

		if (m_batch_texture->can_be_solid_source) {
			return m_batch_texture;
		}
	}

	auto const active_handle { m_assets.active_font_handle() };
	auto const *active_font { m_assets.font(active_handle) };
	if (active_font == nullptr) {
		return nullptr;
	}

	ensure_font_atlas(*active_font);
	return &active_font->atlas;
}

auto Renderer::white_atlas_src() -> Rect<>
{
	return Rect<> {
		.position = smath::Vec2 { 0.0f, 0.0f },
		.size = smath::Vec2 { 1.0f, 1.0f },
	};
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
	push_quad(solid_batch_texture(),
	    white_atlas_src(),
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

	auto const *texture { solid_batch_texture() };
	if (texture == nullptr) {
		flush_batch();
		m_batch_texture = nullptr;
		return;
	}
	if (m_batch_texture != texture) {
		flush_batch();
		m_batch_texture = texture;
	}
	ensure_batch_capacity(4, 6);
	auto const color_u32 { smath::pack_unorm4x8(color) };
	auto const base_index { static_cast<uint16_t>(m_batch_vertices.size()) };
	auto const vertex_start { m_batch_vertices.size() };
	m_batch_vertices.resize(vertex_start + 4);
	auto *const vertices { m_batch_vertices.data() + vertex_start };
	auto const index_start { m_batch_indices.size() };
	m_batch_indices.resize(index_start + 6);
	auto *const indices { m_batch_indices.data() + index_start };

	auto const nx { -dy / length * half_thickness };
	auto const ny { dx / length * half_thickness };
	auto const p0 { smath::Vec2 { start.x() + nx, start.y() + ny } };
	auto const p1 { smath::Vec2 { end.x() + nx, end.y() + ny } };
	auto const p2 { smath::Vec2 { start.x() - nx, start.y() - ny } };
	auto const p3 { smath::Vec2 { end.x() - nx, end.y() - ny } };
	auto const src { white_atlas_src() };
	auto const u0 { src.position.x() };
	auto const v0 { src.position.y() };

	vertices[0] = detail::GraphicsVertex {
		.u = u0,
		.v = v0,
		.color = color_u32,
		.x = p0.x(),
		.y = p0.y(),
		.z = 0.0f,
	};
	vertices[1] = detail::GraphicsVertex {
		.u = u0,
		.v = v0,
		.color = color_u32,
		.x = p1.x(),
		.y = p1.y(),
		.z = 0.0f,
	};
	vertices[2] = detail::GraphicsVertex {
		.u = u0,
		.v = v0,
		.color = color_u32,
		.x = p2.x(),
		.y = p2.y(),
		.z = 0.0f,
	};
	vertices[3] = detail::GraphicsVertex {
		.u = u0,
		.v = v0,
		.color = color_u32,
		.x = p3.x(),
		.y = p3.y(),
		.z = 0.0f,
	};

	indices[0] = base_index;
	indices[1] = static_cast<uint16_t>(base_index + 1);
	indices[2] = static_cast<uint16_t>(base_index + 2);
	indices[3] = static_cast<uint16_t>(base_index + 1);
	indices[4] = static_cast<uint16_t>(base_index + 3);
	indices[5] = static_cast<uint16_t>(base_index + 2);
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

	auto const *texture { solid_batch_texture() };
	if (texture == nullptr) {
		flush_batch();
		m_batch_texture = nullptr;
		return;
	}
	if (m_batch_texture != texture) {
		flush_batch();
		m_batch_texture = texture;
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
	auto const vertex_start { m_batch_vertices.size() };
	m_batch_vertices.resize(vertex_start + needed_vertices);
	auto *const vertices { m_batch_vertices.data() + vertex_start };
	auto const index_start { m_batch_indices.size() };
	m_batch_indices.resize(index_start + needed_indices);
	auto *const indices { m_batch_indices.data() + index_start };
	auto const src { white_atlas_src() };
	auto const u0 { src.position.x() };
	auto const v0 { src.position.y() };

	vertices[0] = detail::GraphicsVertex {
		.u = u0,
		.v = v0,
		.color = color_u32,
		.x = center.x(),
		.y = center.y(),
		.z = 0.0f,
	};

	for (int i {}; i <= seg_count; ++i) {
		auto const t {
			static_cast<float>(i) / static_cast<float>(seg_count),
		};
		auto const angle { start_radians + sweep * t };
		auto const x { center.x() + std::cos(angle) * radius };
		auto const y { center.y() + std::sin(angle) * radius };
		vertices[static_cast<size_t>(i) + 1] = detail::GraphicsVertex {
			.u = u0,
			.v = v0,
			.color = color_u32,
			.x = x,
			.y = y,
			.z = 0.0f,
		};
	}

	for (int i {}; i < seg_count; ++i) {
		auto const idx { static_cast<size_t>(i) * 3 };
		indices[idx] = base;
		indices[idx + 1] = static_cast<uint16_t>(base + i + 1);
		indices[idx + 2] = static_cast<uint16_t>(base + i + 2);
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
    std::optional<FontHandle> const font_handle,
    bool const wrap) -> void
{
	auto handle { font_handle.value_or(m_assets.active_font_handle()) };
	auto const *font { m_assets.font(handle) };
	if (font == nullptr || text.empty() || size <= 0.0f) {
		return;
	}

	auto *font_shape_cache { ensure_shape(handle, *font) };
	if (font_shape_cache == nullptr) {
		return;
	}

	auto const lh { line_height(*font, size) };
	if (lh <= 0.0f) {
		return;
	}

	auto const upem { static_cast<float>(std::max(font->units_per_em, 1)) };
	auto const asc_px { static_cast<float>(font->ascent) * size / upem };
	auto const desc_px { static_cast<float>(-font->descent) * size / upem };
	auto const atlas_scale { size / font->atlas_base_size_px };

	auto const entry_fn {
		[&](std::string_view const line) {
		    return shape_line_entry(handle, *font, line, size);
		},
	};

	static std::vector<TextLayoutLine> layout_lines {};
	layout_lines.clear();
	static std::vector<ShapedGlyph> const empty_glyphs {};

	auto append_layout_line {
		[&](std::string_view const,
		    auto const *const entry,
		    float const width) {
		    layout_lines.push_back(TextLayoutLine {
		        .glyphs = entry != nullptr ? &entry->glyphs : &empty_glyphs,
		        .width = width,
		    });
		},
	};

	size_t line_start {};
	while (line_start <= text.size()) {
		auto const line_end { text.find('\n', line_start) };
		auto const len { line_end == std::string_view::npos
			    ? text.size() - line_start
			    : line_end - line_start };
		auto const source_line { text.substr(line_start, len) };

		if (!wrap) {
			auto const *entry { entry_fn(source_line) };
			append_layout_line(
			    source_line, entry, entry != nullptr ? entry->width : 0.0f);
		} else {
			wrap_line_words(
			    source_line, box.size.x(), entry_fn, append_layout_line);
		}

		if (line_end == std::string_view::npos) {
			break;
		}
		line_start = line_end + 1;
	}

	if (layout_lines.empty()) {
		layout_lines.push_back(TextLayoutLine {
		    .glyphs = &empty_glyphs,
		    .width = 0.0f,
		});
	}

	auto const total_height { (asc_px + desc_px)
		+ std::max(0.0f, static_cast<float>(layout_lines.size() - 1)) * lh };

	auto start_y { box.position.y() };
	if (align_y == TextAlignY::Center) {
		start_y += (box.size.y() - total_height) * 0.5f;
	} else if (align_y == TextAlignY::Bottom) {
		start_y += box.size.y() - total_height;
	}

	for (size_t i {}; i < layout_lines.size(); ++i) {
		auto const &layout_line { layout_lines[i] };
		auto const &shaped { *layout_line.glyphs };
		auto const line_width { layout_line.width };

		auto pen_x { box.position.x() };
		if (align_x == TextAlignX::Center) {
			pen_x += (box.size.x() - line_width) * 0.5f;
		} else if (align_x == TextAlignX::Right) {
			pen_x += box.size.x() - line_width;
		}

		auto pen_y { start_y + asc_px + static_cast<float>(i) * lh };

		for (auto const &glyph : shaped) {
			auto const *glyph_font { font };
			auto *glyph_shape_cache { font_shape_cache };
			auto glyph_atlas_scale { atlas_scale };

			auto [resolved_font,
			    resolved_cache] { find_font_shape_cache(glyph.shaping_font) };
			if (resolved_font != nullptr && resolved_cache != nullptr) {
				glyph_font = resolved_font;
				glyph_shape_cache = resolved_cache;
				glyph_atlas_scale = size / glyph_font->atlas_base_size_px;
			}

			auto const *cached { ensure_glyph(
				*glyph_font, *glyph_shape_cache, glyph.glyph_id) };
			if (cached && cached->u1 > cached->u0 && cached->v1 > cached->v0) {
				auto const x { pen_x + glyph.x_offset
					+ static_cast<float>(cached->x0) * glyph_atlas_scale };
				auto const y { pen_y + glyph.y_offset
					+ static_cast<float>(cached->y0) * glyph_atlas_scale };

				push_quad(&glyph_font->atlas,
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
							(cached->u1 - cached->u0) * glyph_atlas_scale,
							(cached->v1 - cached->v0) * glyph_atlas_scale,
						},
					},
					color);
			}

			pen_x += glyph.x_advance;
			pen_y += glyph.y_advance;
		}
	}
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
		auto const *entry { shape_line_entry(handle, *font, line_text, size) };
		max_width = std::max(max_width, entry != nullptr ? entry->width : 0.0f);

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
	auto const vertex_start { m_batch_vertices.size() };
	m_batch_vertices.resize(vertex_start + 4);
	auto *const vertices { m_batch_vertices.data() + vertex_start };
	auto const index_start { m_batch_indices.size() };
	m_batch_indices.resize(index_start + 6);
	auto *const indices { m_batch_indices.data() + index_start };

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

	vertices[0] = detail::GraphicsVertex {
		.u = u0,
		.v = v0,
		.color = color_u32,
		.x = x0,
		.y = y0,
		.z = 0.0f,
	};
	vertices[1] = detail::GraphicsVertex {
		.u = u1,
		.v = v0,
		.color = color_u32,
		.x = x1,
		.y = y0,
		.z = 0.0f,
	};
	vertices[2] = detail::GraphicsVertex {
		.u = u0,
		.v = v1,
		.color = color_u32,
		.x = x0,
		.y = y1,
		.z = 0.0f,
	};
	vertices[3] = detail::GraphicsVertex {
		.u = u1,
		.v = v1,
		.color = color_u32,
		.x = x1,
		.y = y1,
		.z = 0.0f,
	};

	indices[0] = base_index;
	indices[1] = static_cast<uint16_t>(base_index + 1);
	indices[2] = static_cast<uint16_t>(base_index + 2);
	indices[3] = static_cast<uint16_t>(base_index + 1);
	indices[4] = static_cast<uint16_t>(base_index + 3);
	indices[5] = static_cast<uint16_t>(base_index + 2);
}

void Renderer::draw_polygons(const std::vector<GraphicsVertex> &vertices,
    const std::vector<uint16_t> &indices)
{
	if (vertices.empty() || indices.empty()) {
		return;
	}

	auto const *texture { solid_batch_texture() };
	if (texture == nullptr) {
		flush_batch();
		m_batch_texture = nullptr;
		return;
	}

	if (m_batch_texture != texture) {
		flush_batch();
		m_batch_texture = texture;
	}

	if (m_batch_vertices.size() + vertices.size()
	    > static_cast<size_t>(std::numeric_limits<uint16_t>::max())) {
		flush_batch();
	}

	ensure_batch_capacity(vertices.size(), indices.size());

	auto const base_index { static_cast<uint16_t>(m_batch_vertices.size()) };
	auto const vertex_start { m_batch_vertices.size() };
	m_batch_vertices.resize(vertex_start + vertices.size());

	auto const src { white_atlas_src() };
	auto const u0 { src.position.x() };
	auto const v0 { src.position.y() };

	for (size_t i = 0; i < vertices.size(); ++i) {
		m_batch_vertices[vertex_start + i] = vertices[i];
		m_batch_vertices[vertex_start + i].u = u0;
		m_batch_vertices[vertex_start + i].v = v0;
	}

	auto const index_start { m_batch_indices.size() };
	m_batch_indices.resize(index_start + indices.size());
	for (size_t i = 0; i < indices.size(); ++i) {
		m_batch_indices[index_start + i]
		    = static_cast<uint16_t>(base_index + indices[i]);
	}
}

} // namespace Engine
