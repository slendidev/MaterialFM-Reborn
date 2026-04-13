#pragma once

#include <cstdint>
#include <smath.hpp>

namespace Gui
{

enum class ThemeMode
{
	Light,
	Dark
};

struct Theme
{
	smath::Vec4 background { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 on_background { 1.0f, 1.0f, 1.0f, 1.0f };

	smath::Vec4 surface { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 surface_dim { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 surface_bright { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 surface_container_lowest { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 surface_container_low { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 surface_container { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 surface_container_high { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 surface_container_highest { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 surface_variant { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 on_surface { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 primary { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 on_primary { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 primary_container { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 on_primary_container { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 inverse_primary { 0.0f, 0.0f, 0.0f, 1.0f };

	smath::Vec4 secondary { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 on_secondary { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 secondary_container { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 on_secondary_container { 0.0f, 0.0f, 0.0f, 1.0f };

	smath::Vec4 tertiary { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 on_tertiary { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 tertiary_container { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 on_tertiary_container { 0.0f, 0.0f, 0.0f, 1.0f };

	smath::Vec4 on_surface_variant { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 outline { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 outline_variant { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 inverse_surface { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 inverse_on_surface { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 shadow { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 scrim { 0.0f, 0.0f, 0.0f, 0.55f };
	smath::Vec4 surface_tint { 0.0f, 0.0f, 0.0f, 1.0f };

	smath::Vec4 error { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 on_error { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 error_container { 0.0f, 0.0f, 0.0f, 1.0f };
	smath::Vec4 on_error_container { 0.0f, 0.0f, 0.0f, 1.0f };

	float spacing_small { 6.0f };
	float spacing_medium { 10.0f };
	float spacing_large { 14.0f };
	float radius_medium { 8.0f };
	float title_size { 18.0f };
	float body_size { 14.0f };
	float label_size { 13.0f };
	float button_height { 30.0f };
};

struct MaterialThemeBuilder
{
	static auto build_from_seed(const smath::Vec4 &seed, ThemeMode mode)
	    -> Theme;
	static auto build_from_seed_rgba(std::uint32_t seed_rgba, ThemeMode mode)
	    -> Theme;

private:
	static auto rgba_from_vec4(const smath::Vec4 &color) -> std::uint32_t;
	static auto vec4_from_rgba(std::uint32_t rgba, float alpha = 1.0f)
	    -> smath::Vec4;

	static auto rgba_to_argb(std::uint32_t rgba) -> std::uint32_t;
	static auto argb_to_rgba(std::uint32_t argb) -> std::uint32_t;
};

} // namespace Gui
