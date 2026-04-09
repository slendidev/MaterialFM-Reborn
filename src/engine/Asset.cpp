#include "engine/Asset.h"

#include <algorithm>
#include <cstddef>

#include "engine/Common.h"

namespace Engine
{

Texture::Texture(std::span<uint32_t const> const rgba_data,
    int const in_width,
    int const in_height)
    : content_width { in_width }, content_height { in_height }
{
	width = next_power_of_two(in_width);
	height = next_power_of_two(in_height);

	data.resize(static_cast<size_t>(width * height), 0x00000000u);

	auto it { rgba_data.begin() };
	for (int y = 0; y < content_height; ++y) {
		auto row_start {
			data.begin() + static_cast<std::ptrdiff_t>(y * width),
		};
		std::copy_n(it, static_cast<size_t>(content_width), row_start);
		it += content_width;
	}
}

} // namespace Engine
