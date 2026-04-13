#include "Theme.h"

#include "cpp/cam/hct.h"
#include "cpp/scheme/scheme_tonal_spot.h"

namespace Gui
{

auto MaterialThemeBuilder::build_from_seed(
    const smath::Vec4 &seed, ThemeMode mode) -> Theme
{
	return build_from_seed_rgba(rgba_from_vec4(seed), mode);
}

auto MaterialThemeBuilder::build_from_seed_rgba(
    std::uint32_t seed_rgba, ThemeMode mode) -> Theme
{
	const auto seed_argb = rgba_to_argb(seed_rgba);

	const auto source = material_color_utilities::Hct(
	    static_cast<material_color_utilities::Argb>(seed_argb));

	const auto is_dark = mode == ThemeMode::Dark;
	const auto contrast_level = 0.0;

	const auto scheme = material_color_utilities::SchemeTonalSpot(
	    source, is_dark, contrast_level);

	Theme theme {};
	theme.background = vec4_from_rgba(argb_to_rgba(scheme.GetBackground()));
	theme.on_background
	    = vec4_from_rgba(argb_to_rgba(scheme.GetOnBackground()));
	theme.surface = vec4_from_rgba(argb_to_rgba(scheme.GetSurface()));
	theme.surface_dim = vec4_from_rgba(argb_to_rgba(scheme.GetSurfaceDim()));
	theme.surface_bright
	    = vec4_from_rgba(argb_to_rgba(scheme.GetSurfaceBright()));
	theme.surface_container_lowest
	    = vec4_from_rgba(argb_to_rgba(scheme.GetSurfaceContainerLowest()));
	theme.surface_container_low
	    = vec4_from_rgba(argb_to_rgba(scheme.GetSurfaceContainerLow()));
	theme.surface_container
	    = vec4_from_rgba(argb_to_rgba(scheme.GetSurfaceContainer()));
	theme.surface_container_high
	    = vec4_from_rgba(argb_to_rgba(scheme.GetSurfaceContainerHigh()));
	theme.surface_container_highest
	    = vec4_from_rgba(argb_to_rgba(scheme.GetSurfaceContainerHighest()));
	theme.surface_variant
	    = vec4_from_rgba(argb_to_rgba(scheme.GetSurfaceVariant()));
	theme.primary = vec4_from_rgba(argb_to_rgba(scheme.GetPrimary()));
	theme.on_primary = vec4_from_rgba(argb_to_rgba(scheme.GetOnPrimary()));
	theme.primary_container
	    = vec4_from_rgba(argb_to_rgba(scheme.GetPrimaryContainer()));
	theme.on_primary_container
	    = vec4_from_rgba(argb_to_rgba(scheme.GetOnPrimaryContainer()));
	theme.inverse_primary
	    = vec4_from_rgba(argb_to_rgba(scheme.GetInversePrimary()));
	theme.secondary = vec4_from_rgba(argb_to_rgba(scheme.GetSecondary()));
	theme.on_secondary = vec4_from_rgba(argb_to_rgba(scheme.GetOnSecondary()));
	theme.secondary_container
	    = vec4_from_rgba(argb_to_rgba(scheme.GetSecondaryContainer()));
	theme.on_secondary_container
	    = vec4_from_rgba(argb_to_rgba(scheme.GetOnSecondaryContainer()));
	theme.tertiary = vec4_from_rgba(argb_to_rgba(scheme.GetTertiary()));
	theme.on_tertiary = vec4_from_rgba(argb_to_rgba(scheme.GetOnTertiary()));
	theme.tertiary_container
	    = vec4_from_rgba(argb_to_rgba(scheme.GetTertiaryContainer()));
	theme.on_tertiary_container
	    = vec4_from_rgba(argb_to_rgba(scheme.GetOnTertiaryContainer()));
	theme.on_surface = vec4_from_rgba(argb_to_rgba(scheme.GetOnSurface()));
	theme.on_surface_variant
	    = vec4_from_rgba(argb_to_rgba(scheme.GetOnSurfaceVariant()));
	theme.outline = vec4_from_rgba(argb_to_rgba(scheme.GetOutline()));
	theme.outline_variant
	    = vec4_from_rgba(argb_to_rgba(scheme.GetOutlineVariant()));
	theme.inverse_surface
	    = vec4_from_rgba(argb_to_rgba(scheme.GetInverseSurface()));
	theme.inverse_on_surface
	    = vec4_from_rgba(argb_to_rgba(scheme.GetInverseOnSurface()));
	theme.shadow = vec4_from_rgba(argb_to_rgba(scheme.GetShadow()));
	theme.scrim = vec4_from_rgba(argb_to_rgba(scheme.GetScrim()), 0.55f);
	theme.surface_tint = vec4_from_rgba(argb_to_rgba(scheme.GetSurfaceTint()));
	theme.error = vec4_from_rgba(argb_to_rgba(scheme.GetError()));
	theme.on_error = vec4_from_rgba(argb_to_rgba(scheme.GetOnError()));
	theme.error_container
	    = vec4_from_rgba(argb_to_rgba(scheme.GetErrorContainer()));
	theme.on_error_container
	    = vec4_from_rgba(argb_to_rgba(scheme.GetOnErrorContainer()));

	return theme;
}

auto MaterialThemeBuilder::rgba_from_vec4(const smath::Vec4 &color)
    -> std::uint32_t
{
	return smath::pack_unorm4x8(color);
}

auto MaterialThemeBuilder::vec4_from_rgba(std::uint32_t rgba, float alpha)
    -> smath::Vec4
{
	auto color = smath::unpack_unorm4x8(rgba);
	color.a() = alpha;
	return color;
}

auto MaterialThemeBuilder::rgba_to_argb(std::uint32_t rgba) -> std::uint32_t
{
	const auto r = (rgba >> 0u) & 0xFFu;
	const auto g = (rgba >> 8u) & 0xFFu;
	const auto b = (rgba >> 16u) & 0xFFu;
	const auto a = (rgba >> 24u) & 0xFFu;

	return (a << 24u) | (r << 16u) | (g << 8u) | b;
}

auto MaterialThemeBuilder::argb_to_rgba(std::uint32_t argb) -> std::uint32_t
{
	const auto a = (argb >> 24u) & 0xFFu;
	const auto r = (argb >> 16u) & 0xFFu;
	const auto g = (argb >> 8u) & 0xFFu;
	const auto b = (argb >> 0u) & 0xFFu;

	return (r << 0u) | (g << 8u) | (b << 16u) | (a << 24u);
}

} // namespace Gui
