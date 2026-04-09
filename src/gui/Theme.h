#pragma once

#include <smath.hpp>

namespace Gui
{

struct Theme
{
	smath::Vec4 background { 0.96f, 0.96f, 0.97f, 1.0f };
	smath::Vec4 surface { 1.0f, 1.0f, 1.0f, 1.0f };
	smath::Vec4 surface_variant { 0.90f, 0.92f, 0.94f, 1.0f };
	smath::Vec4 primary { 0.12f, 0.31f, 0.85f, 1.0f };
	smath::Vec4 on_primary { 1.0f, 1.0f, 1.0f, 1.0f };
	smath::Vec4 on_surface { 0.08f, 0.10f, 0.12f, 1.0f };
	smath::Vec4 on_surface_variant { 0.28f, 0.30f, 0.34f, 1.0f };
	smath::Vec4 outline { 0.62f, 0.66f, 0.72f, 1.0f };
	smath::Vec4 scrim { 0.02f, 0.04f, 0.08f, 0.55f };

	float spacing_small { 6.0f };
	float spacing_medium { 10.0f };
	float spacing_large { 14.0f };
	float radius_medium { 8.0f };
	float title_size { 18.0f };
	float body_size { 14.0f };
	float label_size { 13.0f };
	float button_height { 30.0f };
};

} // namespace Gui
