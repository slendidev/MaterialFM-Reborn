#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "engine/Math.h"

namespace Gui
{

struct IconAtlas
{
	std::string texture_path {};
	std::unordered_map<std::string, Engine::Rect<>> rects {};
};

auto load_icon_atlas(std::string_view metadata_path)
    -> std::optional<IconAtlas>;

} // namespace Gui
