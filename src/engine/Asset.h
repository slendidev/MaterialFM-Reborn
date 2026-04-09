#pragma once

#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine
{

struct Texture
{
	Texture() = default;

	Texture(std::span<uint32_t const> rgba_data, int in_width, int in_height);

	int width {};
	int height {};
	int content_width {};
	int content_height {};
	std::vector<uint32_t> data {};
	mutable uint32_t revision { 1 };
	mutable uint32_t uploaded_revision {};

	auto u_max() const -> float { return static_cast<float>(content_width); }
	auto v_max() const -> float { return static_cast<float>(content_height); }

	auto set(bool upload_data = true) const -> void;
	auto mark_dirty() -> void
	{
		if (revision == std::numeric_limits<uint32_t>::max()) {
			revision = 1;
			uploaded_revision = 0;
			return;
		}
		revision += 1;
	}
	auto needs_upload() const -> bool { return uploaded_revision != revision; }
	auto mark_uploaded() const -> void { uploaded_revision = revision; }
};

struct Sound
{
	int channels {};
	int sample_rate {};
	std::vector<short> samples {};
};

struct Song
{
	std::string path {};
	int channels {};
	int sample_rate {};
};

struct Font
{
	struct Glyph
	{
		float u0 {};
		float v0 {};
		float u1 {};
		float v1 {};
		int x0 {};
		int y0 {};
		int x1 {};
		int y1 {};
	};

	std::string path {};
	std::vector<uint8_t> ttf_data {};
	int units_per_em { 1 };
	int ascent {};
	int descent {};
	int line_gap {};
	float atlas_base_size_px { 64.0f };
	mutable Texture atlas {};
	mutable std::unordered_map<uint32_t, Glyph> glyphs {};
	mutable int atlas_pen_x { 1 };
	mutable int atlas_pen_y { 1 };
	mutable int atlas_row_height {};
};

} // namespace Engine
