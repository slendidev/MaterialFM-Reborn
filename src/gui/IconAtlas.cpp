#include "gui/IconAtlas.h"

#include <fstream>
#include <sstream>

namespace Gui
{

auto load_icon_atlas(std::string_view const metadata_path)
    -> std::optional<IconAtlas>
{
	std::ifstream in { std::string(metadata_path) };
	if (!in) {
		return std::nullopt;
	}

	IconAtlas atlas {};
	std::string line {};
	while (std::getline(in, line)) {
		if (line.empty() || line[0] == '#') {
			continue;
		}

		auto const eq_pos { line.find('=') };
		if (eq_pos != std::string::npos) {
			auto const key { line.substr(0, eq_pos) };
			auto const value { line.substr(eq_pos + 1) };
			if (key == "atlas") {
				atlas.texture_path = value;
			}
			continue;
		}

		std::istringstream iss { line };
		std::string name {};
		float x {};
		float y {};
		float w {};
		float h {};
		if (!(iss >> name >> x >> y >> w >> h)) {
			continue;
		}

		atlas.rects.insert_or_assign(name,
		    Engine::Rect<> {
		        .position = smath::Vec2 { x, y },
		        .size = smath::Vec2 { w, h },
		    });
	}

	if (atlas.texture_path.empty() || atlas.rects.empty()) {
		return std::nullopt;
	}

	return atlas;
}

} // namespace Gui
