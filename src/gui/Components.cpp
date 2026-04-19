#include "gui/Components.h"

#include "gui/System.h"
#include "gui/Theme.h"
#include <algorithm>

namespace Gui::components
{

auto register_default_scope_roles(System &system) -> void
{
	auto const sidebar_scope { system.register_scope("sidebar",
		System::ScopeConfig { .priority = 100, .focus_pass_through = false }) };
	auto const dialog_scope { system.register_scope("dialog",
		System::ScopeConfig { .priority = 200, .focus_pass_through = false }) };
	auto const hud_scope { system.register_scope("hud",
		System::ScopeConfig { .priority = -100, .focus_pass_through = true }) };
	system.set_scope_role(ScopeRole::Overlay, sidebar_scope);
	system.set_scope_role(ScopeRole::Exclusive, dialog_scope);
	system.set_scope_role(ScopeRole::Passive, hud_scope);
}

struct ToastRuntime
{
	struct State
	{
		bool active {};
		std::string message {};
		bool pending_show {};
		std::optional<std::string> pending_message {};
	};

	State state {};
	std::function<void()> request_recompose {};
	bool touched_this_frame {};
};

struct SidebarRuntime
{
	bool last_open {};
};

struct DialogRuntime
{
	bool last_open {};
};

namespace
{
struct ResolvedButtonStyle
{
	smath::Vec4 fill {};
	smath::Vec4 focused_fill {};
	smath::Vec4 selected_fill {};
	smath::Vec4 text_color {};
	smath::Vec4 icon_tint {};
	smath::Vec4 selected_text_color {};
	smath::Vec4 selected_icon_tint {};
	smath::Vec4 outline_color {};
};

auto key_string_from_id(Id const key) -> std::string
{
	return std::string(key.label());
}

auto mix_color(smath::Vec4 const a, smath::Vec4 const b, float const t)
    -> smath::Vec4
{
	auto const clamped { std::clamp(t, 0.0f, 1.0f) };
	return a + (b - a) * clamped;
}

auto resolve_color(std::optional<smath::Vec4> const &override_color,
    smath::Vec4 fallback) -> smath::Vec4
{
	return override_color.value_or(fallback);
}

auto resolve_button_style(Theme const &theme, ButtonStyle const &style)
    -> ResolvedButtonStyle
{
	return ResolvedButtonStyle {
		.fill = resolve_color(style.fill, theme.surface_container_low),
		.focused_fill
		= resolve_color(style.focused_fill, theme.secondary_container),
		.selected_fill = resolve_color(style.selected_fill, theme.primary),
		.text_color = resolve_color(style.text_color, theme.on_surface),
		.icon_tint = resolve_color(style.icon_tint, theme.on_surface_variant),
		.selected_text_color
		= resolve_color(style.selected_text_color, theme.on_primary),
		.selected_icon_tint
		= resolve_color(style.selected_icon_tint, theme.on_primary),
		.outline_color = resolve_color(style.outline_color, theme.outline),
	};
}

auto resolve_layer_fill(Theme const &theme,
    std::optional<smath::Vec4> const &fill,
    float const tonal_mix) -> smath::Vec4
{
	if (fill.has_value()) {
		return *fill;
	}
	return mix_color(
	    theme.surface, theme.primary, std::clamp(tonal_mix, 0.0f, 1.0f));
}

auto resolve_dialog_fill(Theme const &theme,
    std::optional<smath::Vec4> const &fill,
    float const tonal_mix) -> smath::Vec4
{
	if (fill.has_value()) {
		return *fill;
	}
	return mix_color(theme.surface_container_high,
	    theme.primary,
	    std::clamp(tonal_mix, 0.0f, 1.0f));
}

auto clamp_explicit_size(
    bool const has_value, float const value, float const max_value) -> float
{
	if (!has_value) {
		return 0.0f;
	}
	return std::min(std::max(0.0f, value), max_value);
}

auto build_button_surface_style(ButtonStyle const &style,
    ResolvedButtonStyle const &resolved) -> SurfaceStyle
{
	return SurfaceStyle::builder()
	    .draw_fill(true)
	    .draw_outline(style.draw_outline)
	    .use_pressable_state(true)
	    .radius(style.corner_radius)
	    .outline_thickness(style.outline_thickness)
	    .fill_color(resolved.fill)
	    .focus_fill_color(resolved.focused_fill)
	    .selected_fill_color(resolved.selected_fill)
	    .outline_color(resolved.outline_color)
	    .build();
}

auto build_button_icon_style(
    ButtonStyle const &style, ResolvedButtonStyle const &resolved) -> IconStyle
{
	return IconStyle::builder()
	    .size(style.icon_size)
	    .use_pressable_state(true)
	    .tint(resolved.icon_tint)
	    .selected_tint(resolved.selected_icon_tint)
	    .build();
}

auto build_button_label_style(
    ButtonStyle const &style, ResolvedButtonStyle const &resolved) -> TextStyle
{
	return TextStyle::builder()
	    .size(style.text_size)
	    .use_pressable_state(true)
	    .color(resolved.text_color)
	    .selected_color(resolved.selected_text_color)
	    .align_x(style.text_align_x)
	    .align_y(style.text_align_y)
	    .build();
}

auto render_button(Context &ctx,
    Id const key,
    std::string_view const label,
    std::optional<std::string_view> const icon_name,
    std::function<void()> on_activate,
    bool const selectable,
    FlexOptions const &options,
    ButtonStyle const &style) -> void
{
	auto const resolved { resolve_button_style(ctx.theme(), style) };

	ctx.pressable(key,
	    FlexOptions::builder().height(style.height).merge(options).build(),
	    std::move(on_activate),
	    selectable,
	    [&](Context &pressable) {
		    pressable.surface(pressable.id("surface"),
		        FlexOptions::builder().flex_grow(1.0f).build(),
		        build_button_surface_style(style, resolved),
		        [&](Context &surface) {
			        surface.flex(surface.id("content"),
			            FlexOptions::builder()
			                .row()
			                .flex_grow(1.0f)
			                .align_items(AlignItems::Center)
			                .padding(std::array<float, 2> {
			                    style.padding_y,
			                    style.padding_x,
			                })
			                .gap(style.icon_gap)
			                .build(),
			            [&](Context &content) {
				            if (icon_name) {
					            content.icon(content.id("icon"),
					                *icon_name,
					                build_button_icon_style(style, resolved));
				            }
				            content.text(content.id("label"),
				                label,
				                build_button_label_style(style, resolved),
				                FlexOptions::builder().flex_grow(1.0f).build());
			            });
		        });
	    });
}

auto render_sidebar(Context &ctx,
    Id const key,
    FlexOptions const &options,
    Context::ComposeFn const &fn,
    SidebarStyle const &style) -> void
{
	auto const key_string { key_string_from_id(key) };
	auto &runtime {
		ctx.remember<std::shared_ptr<SidebarRuntime>>(
		    key_string + "/runtime", std::make_shared<SidebarRuntime>()),
	};
	auto const previous_open { runtime->last_open };
	auto const open_changed { style.open != runtime->last_open };
	if (open_changed) {
		runtime->last_open = style.open;
	}
	auto const restart_animation { open_changed };
	auto const &theme { ctx.theme() };
	auto const scrim { style.scrim.value_or(theme.scrim) };
	auto const fill { resolve_layer_fill(theme, style.fill, style.tonal_mix) };
	auto const closed_left { -style.width };
	auto const left_from { previous_open ? 0.0f : closed_left };
	auto const left_to { style.open ? 0.0f : closed_left };
	auto const opacity_from { previous_open ? 1.0f : 0.0f };
	auto const opacity_to { style.open ? 1.0f : 0.0f };
	auto left_anim {
		Animation::Definition::builder(key_string + "/left")
		    .from(left_from)
		    .to(left_to)
		    .duration(0.15f)
		    .easing(Animation::Easing::EaseOutCubic)
		    .repeat(Animation::RepeatMode::Once)
		    .build(),
	};
	auto left_ref { left_anim.get_ref() };
	auto opacity_anim {
		Animation::Definition::builder(key_string + "/opacity")
		    .from(opacity_from)
		    .to(opacity_to)
		    .duration(0.22f)
		    .easing(Animation::Easing::EaseOutCubic)
		    .repeat(Animation::RepeatMode::Once)
		    .build(),
	};
	auto opacity_ref { opacity_anim.get_ref() };
	if (restart_animation) {
		ctx.restart_animation(left_ref);
		ctx.restart_animation(opacity_ref);
	}

	ctx.layer(key,
	    LayerSpec::builder()
	        .focus_mode(
	            style.open ? LayerFocusMode::Overlay : LayerFocusMode::Passive)
	        .top(0.0f)
	        .bottom(0.0f)
	        .left(left_ref)
	        .build(),
	    FlexOptions::builder().width(style.width).build(),
	    LayerStyle::builder()
	        .draw_scrim(true)
	        .scrim_color(scrim)
	        .scrim_opacity(opacity_ref)
	        .draw_fill(true)
	        .radius(style.corner_radius)
	        .fill_color(fill)
	        .opacity(1.0f)
	        .build(),
	    [&](Context &layer_ctx) {
		    layer_ctx.scrollable(layer_ctx.id("drawer_scrollable"),
		        Gui::ScrollOptions::builder().build(),
		        [&](Gui::Context &scroll) {
			        scroll.flex(scroll.id("content"), options, fn);
		        });
	    });
}

auto render_dialog(Context &ctx,
    Id const key,
    FlexOptions const &options,
    Context::ComposeFn const &fn,
    DialogStyle const &style) -> void
{
	auto const key_string { key_string_from_id(key) };
	auto &runtime {
		ctx.remember<std::shared_ptr<DialogRuntime>>(
		    key_string + "/runtime", std::make_shared<DialogRuntime>()),
	};
	auto const previous_open { runtime->last_open };
	if (style.open != runtime->last_open) {
		runtime->last_open = style.open;
	}
	auto const window_rect { ctx.window_rect() };
	auto const screen_width { window_rect.size.x() };
	auto const screen_height { window_rect.size.y() };
	auto const dialog_max_width {
		screen_width * std::clamp(style.max_width_ratio, 0.1f, 1.0f),
	};
	auto const dialog_max_height {
		screen_height * std::clamp(style.max_height_ratio, 0.1f, 1.0f),
	};
	auto const dialog_width {
		clamp_explicit_size(
		    options.has_width(), options.width(), dialog_max_width),
	};
	auto const dialog_height {
		clamp_explicit_size(
		    options.has_height(), options.height(), dialog_max_height),
	};
	auto const &theme { ctx.theme() };
	auto const scrim { style.scrim.value_or(theme.scrim) };
	auto const fill { resolve_dialog_fill(theme, style.fill, style.tonal_mix) };
	auto const opacity_from { previous_open ? 1.0f : 0.0f };
	auto const opacity_to { style.open ? 1.0f : 0.0f };
	auto opacity_anim {
		Animation::Definition::builder(key_string + "/opacity")
		    .from(opacity_from)
		    .to(opacity_to)
		    .duration(0.18f)
		    .easing(Animation::Easing::EaseOutCubic)
		    .repeat(Animation::RepeatMode::Once)
		    .build(),
	};
	auto opacity_ref { opacity_anim.get_ref() };
	if (style.open != previous_open) {
		ctx.restart_animation(opacity_ref);
	}
	ctx.layer(key,
	    LayerSpec::builder()
	        .focus_mode(style.open ? LayerFocusMode::Exclusive
	                               : LayerFocusMode::Passive)
	        .top(0.0f)
	        .right(0.0f)
	        .bottom(0.0f)
	        .left(0.0f)
	        .build(),
	    FlexOptions::builder()
	        .width(screen_width)
	        .height(screen_height)
	        .build(),
	    LayerStyle::builder()
	        .draw_scrim(true)
	        .scrim_color(scrim)
	        .scrim_opacity(opacity_ref)
	        .draw_fill(false)
	        .opacity(opacity_ref)
	        .build(),
	    [&](Context &layer_ctx) {
		    layer_ctx.flex(layer_ctx.id("center"),
		        FlexOptions::builder()
		            .row()
		            .width(screen_width)
		            .height(screen_height)
		            .justify_content(JustifyContent::Center)
		            .align_items(AlignItems::Center)
		            .build(),
		        [&](Context &center) {
			        center.surface(center.id("card"),
			            FlexOptions::builder()
			                .align_items(AlignItems::Start)
			                .build(),
			            SurfaceStyle::builder()
			                .draw_fill(true)
			                .draw_outline(false)
			                .use_pressable_state(false)
			                .radius(style.corner_radius)
			                .outline_thickness(1.0f)
			                .fill_color(fill)
			                .focus_fill_color(smath::Vec4 {})
			                .selected_fill_color(smath::Vec4 {})
			                .outline_color(smath::Vec4 {})
			                .build(),
			            [&](Context &card) {
				            auto scroll_options_builder {
					            ScrollOptions::builder().vertical().align_self(
					                AlignSelf::Start),
				            };
				            if (dialog_width > 0.0f) {
					            scroll_options_builder.width(dialog_width);
				            }
				            if (dialog_height > 0.0f) {
					            scroll_options_builder.height(dialog_height);
				            }
				            scroll_options_builder.max_width(dialog_max_width)
				                .max_height(dialog_max_height);
				            card.scrollable(card.id("scroll"),
				                scroll_options_builder.build(),
				                [&](Context &scroll) {
					                scroll.flex(
					                    scroll.id("content"), options, fn);
				                });
			            });
		        });
	    });
}

auto render_toast(Context &ctx, Id const key, ToastStyle const &style) -> Toast
{
	auto const key_string { key_string_from_id(key) };
	auto &runtime {
		ctx.remember<std::shared_ptr<ToastRuntime>>(
		    key_string + "/runtime", std::make_shared<ToastRuntime>()),
	};
	auto *system { &ctx.system() };
	runtime->request_recompose = [system]() {
		if (system != nullptr) {
			system->invalidate_compose();
		}
	};
	runtime->touched_this_frame = true;
	auto &toast_state { runtime->state };
	auto const rising_edge { toast_state.pending_show };
	if (toast_state.pending_show) {
		toast_state.active = true;
		if (toast_state.pending_message.has_value()) {
			toast_state.message = std::move(*toast_state.pending_message);
			toast_state.pending_message.reset();
		}
		toast_state.pending_show = false;
	}
	auto const &state { toast_state };
	if (state.message.empty() && !state.active) {
		runtime->touched_this_frame = false;
		return Toast { runtime, key_string };
	}

	auto const fade_in { std::max(0.001f, style.fade_in_s) };
	auto const hold { std::max(0.0f, style.hold_s) };
	auto const fade_out { std::max(0.001f, style.fade_out_s) };
	auto const total { fade_in + hold + fade_out };

	auto progress_anim {
		Animation::Definition::builder(key_string + "/alpha")
		    .from(0.0f)
		    .to(1.0f)
		    .duration(total)
		    .easing(Animation::Easing::Linear)
		    .repeat(Animation::RepeatMode::Once)
		    .build(),
	};
	auto progress_ref { progress_anim.get_ref() };
	auto opacity_anim {
		Animation::Definition::builder(key_string + "/opacity")
		    .from(0.0f)
		    .to(1.0f)
		    .duration(total)
		    .custom_easing([fade_in, hold, fade_out, total](float const t) {
		        auto const elapsed { std::clamp(t, 0.0f, 1.0f) * total };
		        if (elapsed < fade_in) {
			        return elapsed / fade_in;
		        }
		        if (elapsed < fade_in + hold) {
			        return 1.0f;
		        }
		        auto const out_t {
			        (elapsed - fade_in - hold) / std::max(0.001f, fade_out),
		        };
		        return 1.0f - std::clamp(out_t, 0.0f, 1.0f);
		    })
		    .repeat(Animation::RepeatMode::Once)
		    .build(),
	};
	auto opacity_ref { opacity_anim.get_ref() };
	if (rising_edge) {
		ctx.restart_animation(progress_ref);
		ctx.restart_animation(opacity_ref);
	}
	auto const progress { std::clamp(
		ctx.sample_animation(progress_ref), 0.0f, 1.0f) };
	if (state.active && progress >= 0.999f && !rising_edge) {
		toast_state.active = false;
	}
	if (!toast_state.active && progress >= 0.999f) {
		runtime->touched_this_frame = false;
		return Toast { runtime, key_string };
	}
	auto const fill { style.fill.value_or(ctx.theme().inverse_surface) };
	auto const text_color { style.text_color.value_or(
		ctx.theme().inverse_on_surface) };

	auto const window_rect { ctx.window_rect() };
	ctx.layer(key,
	    LayerSpec::builder()
	        .passive()
	        .top(0.0f)
	        .right(0.0f)
	        .bottom(0.0f)
	        .left(0.0f)
	        .build(),
	    FlexOptions::builder()
	        .width(window_rect.size.x())
	        .height(window_rect.size.y())
	        .build(),
	    LayerStyle::builder()
	        .draw_scrim(false)
	        .draw_fill(false)
	        .opacity(opacity_ref)
	        .build(),
	    [&](Context &layer_ctx) {
		    layer_ctx.flex(layer_ctx.id("bottom_center"),
		        FlexOptions::builder()
		            .row()
		            .width(window_rect.size.x())
		            .height(window_rect.size.y())
		            .padding(std::array<float, 4> {
		                0.0f,
		                0.0f,
		                std::max(0.0f, style.bottom_margin),
		                0.0f,
		            })
		            .justify_content(JustifyContent::Center)
		            .align_items(AlignItems::End)
		            .build(),
		        [&](Context &bottom_center) {
			        bottom_center.surface(bottom_center.id("toast_card"),
			            FlexOptions::builder()
			                .width(style.width)
			                .height(style.min_height)
			                .build(),
			            SurfaceStyle::builder()
			                .draw_fill(true)
			                .draw_outline(false)
			                .radius(std::max(
			                    style.corner_radius, style.min_height * 0.5f))
			                .fill_color(fill)
			                .build(),
			            [&](Context &card) {
				            card.flex(card.id("content"),
				                FlexOptions::builder()
				                    .row()
				                    .width(style.width)
				                    .height(style.min_height)
				                    .padding(std::array<float, 2> {
				                        style.padding_y,
				                        style.padding_x,
				                    })
				                    .justify_content(JustifyContent::Center)
				                    .align_items(AlignItems::Center)
				                    .build(),
				                [&](Context &content) {
					                content.text(content.id("message"),
					                    toast_state.message,
					                    TextStyle::builder()
					                        .size(style.text_size)
					                        .align_x(TextAlignX::Center)
					                        .align_y(TextAlignY::Center)
					                        .color(text_color)
					                        .selected_color(text_color)
					                        .build());
				                });
			            });
		        });
	    });

	runtime->touched_this_frame = false;
	return Toast { runtime, key_string };
}
} // namespace

auto ButtonStyle::builder() -> ButtonStyle::Builder
{
	return Builder {};
}

auto ButtonStyle::Builder::copy(ButtonStyle const style) -> Builder &
{
	m_style = style;
	return *this;
}

#define GUI_BUTTON_STYLE_SETTER(name, field, type) \
	auto ButtonStyle::Builder::name(type const value) -> Builder & \
	{ \
		m_style.field = value; \
		return *this; \
	}

GUI_BUTTON_STYLE_SETTER(height, height, float)
GUI_BUTTON_STYLE_SETTER(corner_radius, corner_radius, float)
GUI_BUTTON_STYLE_SETTER(text_size, text_size, float)
GUI_BUTTON_STYLE_SETTER(padding_x, padding_x, float)
GUI_BUTTON_STYLE_SETTER(padding_y, padding_y, float)
GUI_BUTTON_STYLE_SETTER(icon_size, icon_size, float)
GUI_BUTTON_STYLE_SETTER(icon_gap, icon_gap, float)
GUI_BUTTON_STYLE_SETTER(fill, fill, smath::Vec4)
GUI_BUTTON_STYLE_SETTER(focused_fill, focused_fill, smath::Vec4)
GUI_BUTTON_STYLE_SETTER(selected_fill, selected_fill, smath::Vec4)
GUI_BUTTON_STYLE_SETTER(text_color, text_color, smath::Vec4)
GUI_BUTTON_STYLE_SETTER(icon_tint, icon_tint, smath::Vec4)
GUI_BUTTON_STYLE_SETTER(selected_text_color, selected_text_color, smath::Vec4)
GUI_BUTTON_STYLE_SETTER(selected_icon_tint, selected_icon_tint, smath::Vec4)
GUI_BUTTON_STYLE_SETTER(draw_outline, draw_outline, bool)
GUI_BUTTON_STYLE_SETTER(outline_thickness, outline_thickness, float)
GUI_BUTTON_STYLE_SETTER(outline_color, outline_color, smath::Vec4)
GUI_BUTTON_STYLE_SETTER(text_align_x, text_align_x, TextAlignX)
GUI_BUTTON_STYLE_SETTER(text_align_y, text_align_y, TextAlignY)

#undef GUI_BUTTON_STYLE_SETTER

auto ButtonStyle::Builder::build() const -> ButtonStyle
{
	return m_style;
}

auto Button::builder(Context &ctx, Id const key) -> Button::Builder
{
	return Builder { ctx, key };
}

auto Button::builder(Context &ctx, std::string_view const key)
    -> Button::Builder
{
	return Builder { ctx, ctx.id(key) };
}

Button::Builder::Builder(Context &ctx, Id const key)
    : m_ctx(ctx), m_key(key) { }

auto Button::Builder::label(std::string_view const value) -> Builder &
{
	m_label = std::string(value);
	return *this;
}

auto Button::Builder::icon(std::string_view const value) -> Builder &
{
	m_icon_name = std::string(value);
	return *this;
}

auto Button::Builder::on_activate(std::function<void()> fn) -> Builder &
{
	m_on_activate = std::move(fn);
	return *this;
}

auto Button::Builder::selectable(bool const value) -> Builder &
{
	m_selectable = value;
	return *this;
}

auto Button::Builder::options(FlexOptions const value) -> Builder &
{
	m_options = value;
	return *this;
}

auto Button::Builder::style(ButtonStyle const value) -> Builder &
{
	m_style = value;
	return *this;
}

auto Button::Builder::build() -> void
{
	auto icon_name_view { m_icon_name
		    ? std::optional<std::string_view> { *m_icon_name }
		    : std::nullopt };
	render_button(m_ctx,
	    m_key,
	    m_label,
	    icon_name_view,
	    std::move(m_on_activate),
	    m_selectable,
	    m_options,
	    m_style);
}

auto Sidebar::builder(Context &ctx, Id const key) -> Sidebar::Builder
{
	return Builder { ctx, key };
}

auto Sidebar::builder(Context &ctx, std::string_view const key)
    -> Sidebar::Builder
{
	return Builder { ctx, ctx.id(key) };
}

Sidebar::Builder::Builder(Context &ctx, Id const key) : m_ctx(ctx), m_key(key)
{ }

auto Sidebar::Builder::open(bool const value) -> Builder &
{
	m_style.open = value;
	return *this;
}

auto Sidebar::Builder::options(FlexOptions const value) -> Builder &
{
	m_options = value;
	return *this;
}

auto Sidebar::Builder::style(SidebarStyle const value) -> Builder &
{
	m_style = value;
	return *this;
}

auto Sidebar::Builder::content(Context::ComposeFn fn) -> Builder &
{
	m_content = std::move(fn);
	return *this;
}

auto Sidebar::Builder::build() -> void
{
	render_sidebar(m_ctx, m_key, m_options, m_content, m_style);
}

auto Dialog::builder(Context &ctx, Id const key) -> Dialog::Builder
{
	return Builder { ctx, key };
}

auto Dialog::builder(Context &ctx, std::string_view const key)
    -> Dialog::Builder
{
	return Builder { ctx, ctx.id(key) };
}

Dialog::Builder::Builder(Context &ctx, Id const key)
    : m_ctx(ctx), m_key(key) { }

auto Dialog::Builder::open(bool const value) -> Builder &
{
	m_style.open = value;
	return *this;
}

auto Dialog::Builder::options(FlexOptions const value) -> Builder &
{
	m_options = value;
	return *this;
}

auto Dialog::Builder::style(DialogStyle const value) -> Builder &
{
	m_style = value;
	return *this;
}

auto Dialog::Builder::content(Context::ComposeFn fn) -> Builder &
{
	m_content = std::move(fn);
	return *this;
}

auto Dialog::Builder::build() -> void
{
	render_dialog(m_ctx, m_key, m_options, m_content, m_style);
}

Toast::Toast(std::shared_ptr<ToastRuntime> runtime, std::string key)
    : m_runtime(std::move(runtime)), m_key(std::move(key))
{ }

auto Toast::builder(Context &ctx, Id const key) -> Toast::Builder
{
	return Builder { ctx, key };
}

auto Toast::builder(Context &ctx, std::string_view const key) -> Toast::Builder
{
	return Builder { ctx, ctx.id(key) };
}

Toast::Builder::Builder(Context &ctx, Id const key) : m_ctx(ctx), m_key(key) { }

auto Toast::Builder::style(ToastStyle const value) -> Builder &
{
	m_style = value;
	return *this;
}

auto Toast::Builder::build() -> Toast
{
	return render_toast(m_ctx, m_key, m_style);
}

auto Toast::show() const -> void
{
	if (!m_runtime) {
		return;
	}
	m_runtime->state.pending_show = true;
	if (m_runtime->request_recompose) {
		m_runtime->request_recompose();
	}
}

auto Toast::show(std::string_view const message) const -> void
{
	if (!m_runtime) {
		return;
	}
	m_runtime->state.pending_message = std::string(message);
	m_runtime->state.pending_show = true;
	if (m_runtime->request_recompose) {
		m_runtime->request_recompose();
	}
}

} // namespace Gui::components
