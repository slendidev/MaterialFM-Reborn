#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

#include "engine/Math.h"
#include "gui/Animation.h"
#include "gui/Id.h"
#include "gui/Node.h"
#include "gui/System.h"

namespace Gui
{

struct Theme;

using LayoutPadding
    = std::variant<float, std::array<float, 2>, std::array<float, 4>>;
using LayoutAnimatedFloat = std::variant<float, Animation::Ref>;

struct CommonLayoutOptionsData
{
	LayoutPadding padding { 0.0f };
	std::optional<LayoutAnimatedFloat> width {};
	std::optional<LayoutAnimatedFloat> height {};
	float flex_grow { 0.0f };
	float flex_shrink { 1.0f };
	std::optional<float> flex_basis {};
	std::optional<float> min_width {};
	std::optional<float> min_height {};
	std::optional<float> max_width {};
	std::optional<float> max_height {};
	AlignSelf align_self { AlignSelf::Auto };
};

class FlexOptions
{
public:
	using Padding = LayoutPadding;
	using AnimatedFloat = LayoutAnimatedFloat;
	class Builder;

	static auto builder() -> Builder;

	auto padding() const -> Padding const & { return m_common.padding; }
	auto has_width() const -> bool { return m_common.width.has_value(); }
	auto has_height() const -> bool { return m_common.height.has_value(); }
	auto has_gap() const -> bool { return m_gap.has_value(); }
	auto has_row_gap() const -> bool { return m_row_gap.has_value(); }
	auto has_column_gap() const -> bool { return m_column_gap.has_value(); }
	auto has_flex_basis() const -> bool
	{
		return m_common.flex_basis.has_value();
	}
	auto has_min_width() const -> bool
	{
		return m_common.min_width.has_value();
	}
	auto has_min_height() const -> bool
	{
		return m_common.min_height.has_value();
	}
	auto has_max_width() const -> bool
	{
		return m_common.max_width.has_value();
	}
	auto has_max_height() const -> bool
	{
		return m_common.max_height.has_value();
	}
	auto width() const -> float;
	auto height() const -> float;
	auto width_value() const -> std::optional<AnimatedFloat> const &
	{
		return m_common.width;
	}
	auto height_value() const -> std::optional<AnimatedFloat> const &
	{
		return m_common.height;
	}
	auto gap() const -> float { return m_gap.value_or(0.0f); }
	auto row_gap() const -> float { return m_row_gap.value_or(gap()); }
	auto column_gap() const -> float { return m_column_gap.value_or(gap()); }
	auto direction() const -> FlexDirection { return m_direction; }
	auto wrap() const -> FlexWrap { return m_wrap; }
	auto justify_content() const -> JustifyContent { return m_justify_content; }
	auto align_items() const -> AlignItems { return m_align_items; }
	auto align_content() const -> AlignContent { return m_align_content; }
	auto align_self() const -> AlignSelf { return m_common.align_self; }
	auto flex_grow() const -> float { return m_common.flex_grow; }
	auto flex_shrink() const -> float { return m_common.flex_shrink; }
	auto flex_basis() const -> float
	{
		return m_common.flex_basis.value_or(-1.0f);
	}
	auto min_width() const -> float
	{
		return m_common.min_width.value_or(0.0f);
	}
	auto min_height() const -> float
	{
		return m_common.min_height.value_or(0.0f);
	}
	auto max_width() const -> float
	{
		return m_common.max_width.value_or(0.0f);
	}
	auto max_height() const -> float
	{
		return m_common.max_height.value_or(0.0f);
	}

private:
	FlexOptions() = default;

	CommonLayoutOptionsData m_common {};
	std::optional<float> m_gap {};
	std::optional<float> m_row_gap {};
	std::optional<float> m_column_gap {};
	FlexDirection m_direction { FlexDirection::Column };
	FlexWrap m_wrap { FlexWrap::NoWrap };
	JustifyContent m_justify_content { JustifyContent::Start };
	AlignItems m_align_items { AlignItems::Stretch };
	AlignContent m_align_content { AlignContent::Start };

	friend class Builder;
};

class FlexOptions::Builder
{
public:
	auto row() -> Builder &;
	auto row_reverse() -> Builder &;
	auto column() -> Builder &;
	auto column_reverse() -> Builder &;
	auto direction(FlexDirection value) -> Builder &;
	auto wrap(FlexWrap value) -> Builder &;
	auto justify_content(JustifyContent value) -> Builder &;
	auto align_items(AlignItems value) -> Builder &;
	auto align_content(AlignContent value) -> Builder &;
	auto align_self(AlignSelf value) -> Builder &;
	auto gap(float value) -> Builder &;
	auto row_gap(float value) -> Builder &;
	auto column_gap(float value) -> Builder &;
	auto width(float value) -> Builder &;
	auto width(Animation::Ref value) -> Builder &;
	auto height(float value) -> Builder &;
	auto height(Animation::Ref value) -> Builder &;
	auto flex(float grow, float shrink = 1.0f) -> Builder &;
	auto flex_grow(float value) -> Builder &;
	auto flex_shrink(float value) -> Builder &;
	auto flex_basis_px(float value) -> Builder &;
	auto flex_basis_auto() -> Builder &;
	auto merge(FlexOptions const &options) -> Builder &;
	auto min_width(float value) -> Builder &;
	auto min_height(float value) -> Builder &;
	auto max_width(float value) -> Builder &;
	auto max_height(float value) -> Builder &;
	auto padding(float value) -> Builder &;
	auto padding(std::array<float, 2> value) -> Builder &;
	auto padding(std::array<float, 4> value) -> Builder &;
	auto build() const -> FlexOptions;

private:
	FlexOptions m_options {};
};

class ScrollOptions
{
public:
	using Padding = LayoutPadding;
	class Builder;

	static auto builder() -> Builder;

	auto axis() const -> ScrollAxis { return m_axis; }
	auto reveal_mode() const -> ScrollRevealMode { return m_reveal_mode; }
	auto step() const -> float { return m_step; }
	auto has_width() const -> bool { return m_common.width.has_value(); }
	auto has_height() const -> bool { return m_common.height.has_value(); }
	auto has_flex_basis() const -> bool
	{
		return m_common.flex_basis.has_value();
	}
	auto has_min_width() const -> bool
	{
		return m_common.min_width.has_value();
	}
	auto has_min_height() const -> bool
	{
		return m_common.min_height.has_value();
	}
	auto has_max_width() const -> bool
	{
		return m_common.max_width.has_value();
	}
	auto has_max_height() const -> bool
	{
		return m_common.max_height.has_value();
	}
	auto width() const -> float
	{
		if (!m_common.width.has_value()) {
			return 0.0f;
		}
		if (auto const *value { std::get_if<float>(&*m_common.width) }) {
			return *value;
		}
		return std::get<Animation::Ref>(*m_common.width).fallback;
	}
	auto height() const -> float
	{
		if (!m_common.height.has_value()) {
			return 0.0f;
		}
		if (auto const *value { std::get_if<float>(&*m_common.height) }) {
			return *value;
		}
		return std::get<Animation::Ref>(*m_common.height).fallback;
	}
	auto as_flex_options() const -> FlexOptions;
	auto max_width() const -> float
	{
		return m_common.max_width.value_or(0.0f);
	}
	auto max_height() const -> float
	{
		return m_common.max_height.value_or(0.0f);
	}
	auto min_width() const -> float
	{
		return m_common.min_width.value_or(0.0f);
	}
	auto min_height() const -> float
	{
		return m_common.min_height.value_or(0.0f);
	}

private:
	ScrollOptions() = default;

	CommonLayoutOptionsData m_common {};
	ScrollAxis m_axis { ScrollAxis::Vertical };
	ScrollRevealMode m_reveal_mode { ScrollRevealMode::Minimal };
	float m_step { 24.0f };

	friend class Builder;
};

class ScrollOptions::Builder
{
public:
	auto axis(ScrollAxis value) -> Builder &;
	auto vertical() -> Builder &;
	auto horizontal() -> Builder &;
	auto both() -> Builder &;
	auto reveal_mode(ScrollRevealMode value) -> Builder &;
	auto step(float value) -> Builder &;
	auto max_width(float value) -> Builder &;
	auto max_height(float value) -> Builder &;
	auto min_width(float value) -> Builder &;
	auto min_height(float value) -> Builder &;
	auto width(float value) -> Builder &;
	auto height(float value) -> Builder &;
	auto flex(float grow, float shrink = 1.0f) -> Builder &;
	auto flex_grow(float value) -> Builder &;
	auto flex_shrink(float value) -> Builder &;
	auto flex_basis_px(float value) -> Builder &;
	auto flex_basis_auto() -> Builder &;
	auto align_self(AlignSelf value) -> Builder &;
	auto padding(float value) -> Builder &;
	auto padding(std::array<float, 2> value) -> Builder &;
	auto padding(std::array<float, 4> value) -> Builder &;
	auto build() const -> ScrollOptions;

private:
	ScrollOptions m_options {};
};

struct TextStyle
{
	class Builder;
	static auto builder() -> Builder;

	float size { 14.0f };
	TextAlignX align_x { TextAlignX::Left };
	TextAlignY align_y { TextAlignY::Top };
	bool use_pressable_state {};
	std::optional<smath::Vec4> color {};
	std::optional<smath::Vec4> selected_color {};
};

class TextStyle::Builder
{
public:
	auto copy(TextStyle style) -> Builder &;
	auto size(float value) -> Builder &;
	auto align_x(TextAlignX value) -> Builder &;
	auto align_y(TextAlignY value) -> Builder &;
	auto use_pressable_state(bool value) -> Builder &;
	auto color(smath::Vec4 value) -> Builder &;
	auto selected_color(smath::Vec4 value) -> Builder &;
	auto build() const -> TextStyle;

private:
	TextStyle m_style {};
};

struct SurfaceStyle
{
	class Builder;
	static auto builder() -> Builder;

	bool draw_fill { true };
	bool draw_outline {};
	bool use_pressable_state {};
	float radius {};
	float outline_thickness { 1.0f };
	std::optional<smath::Vec4> fill_color {};
	std::optional<smath::Vec4> focus_fill_color {};
	std::optional<smath::Vec4> selected_fill_color {};
	std::optional<smath::Vec4> outline_color {};
};

class SurfaceStyle::Builder
{
public:
	auto draw_fill(bool value) -> Builder &;
	auto draw_outline(bool value) -> Builder &;
	auto use_pressable_state(bool value) -> Builder &;
	auto radius(float value) -> Builder &;
	auto outline_thickness(float value) -> Builder &;
	auto fill_color(smath::Vec4 value) -> Builder &;
	auto focus_fill_color(smath::Vec4 value) -> Builder &;
	auto selected_fill_color(smath::Vec4 value) -> Builder &;
	auto outline_color(smath::Vec4 value) -> Builder &;
	auto build() const -> SurfaceStyle;

private:
	SurfaceStyle m_style {};
};

struct IconStyle
{
	class Builder;
	static auto builder() -> Builder;

	float size { 24.0f };
	bool use_pressable_state {};
	std::optional<smath::Vec4> tint {};
	std::optional<smath::Vec4> selected_tint {};
};

class IconStyle::Builder
{
public:
	auto size(float value) -> Builder &;
	auto use_pressable_state(bool value) -> Builder &;
	auto tint(smath::Vec4 value) -> Builder &;
	auto selected_tint(smath::Vec4 value) -> Builder &;
	auto build() const -> IconStyle;

private:
	IconStyle m_style {};
};

struct LayerStyle
{
	class Builder;
	static auto builder() -> Builder;

	bool draw_scrim { true };
	std::optional<smath::Vec4> scrim_color {};
	float scrim_opacity { 1.0f };
	std::optional<Animation::Ref> animated_scrim_opacity {};
	bool draw_fill { true };
	float radius {};
	std::optional<smath::Vec4> fill_color {};
	float opacity { 1.0f };
	std::optional<Animation::Ref> animated_opacity {};
};

class LayerStyle::Builder
{
public:
	auto draw_scrim(bool value) -> Builder &;
	auto scrim_color(smath::Vec4 value) -> Builder &;
	auto scrim_opacity(float value) -> Builder &;
	auto scrim_opacity(Animation::Ref value) -> Builder &;
	auto draw_fill(bool value) -> Builder &;
	auto radius(float value) -> Builder &;
	auto fill_color(smath::Vec4 value) -> Builder &;
	auto opacity(float value) -> Builder &;
	auto opacity(Animation::Ref value) -> Builder &;
	auto build() const -> LayerStyle;

private:
	LayerStyle m_style {};
};

struct OverlayHostSpec
{
	class Builder;
	static auto builder() -> Builder;

	bool clip_to_bounds {};
	int z_index {};
};

class OverlayHostSpec::Builder
{
public:
	auto clip_to_bounds(bool value) -> Builder &;
	auto z_index(int value) -> Builder &;
	auto build() const -> OverlayHostSpec;

private:
	OverlayHostSpec m_spec {};
};

struct LayerSpec
{
	class Builder;
	static auto builder() -> Builder;

	LayerFocusMode focus_mode { LayerFocusMode::Overlay };
	std::optional<AnimatedScalar> top {};
	std::optional<AnimatedScalar> right {};
	std::optional<AnimatedScalar> bottom {};
	std::optional<AnimatedScalar> left {};
	int z_index {};
};

class LayerSpec::Builder
{
public:
	auto focus_mode(LayerFocusMode value) -> Builder &;
	auto z_index(int value) -> Builder &;
	auto overlay() -> Builder &;
	auto exclusive() -> Builder &;
	auto passive() -> Builder &;
	auto top(float value) -> Builder &;
	auto top(Animation::Ref value) -> Builder &;
	auto right(float value) -> Builder &;
	auto right(Animation::Ref value) -> Builder &;
	auto bottom(float value) -> Builder &;
	auto bottom(Animation::Ref value) -> Builder &;
	auto left(float value) -> Builder &;
	auto left(Animation::Ref value) -> Builder &;
	auto build() const -> LayerSpec;

private:
	LayerSpec m_spec {};
};

template<typename T> class MutableState
{
public:
	using Getter = std::function<T const &()>;
	using Setter = std::function<void(T)>;
	using Updater = std::function<void(std::function<void(T &)>)>;

	MutableState() = default;
	MutableState(Getter getter, Setter setter, Updater updater)
	    : m_get(std::move(getter)), m_set(std::move(setter)),
	      m_update(std::move(updater))
	{ }
	MutableState(Getter getter, Setter setter, Updater updater, T fallback)
	    : m_get(std::move(getter)), m_set(std::move(setter)),
	      m_update(std::move(updater)), m_fallback(std::move(fallback))
	{ }

	auto get() const -> T const &
	{
		if (m_get) {
			return m_get();
		}
		if (!m_fallback.has_value()) {
			m_fallback.emplace();
		}
		return *m_fallback;
	}
	auto set(T value) const -> void
	{
		if (m_set) {
			m_set(std::move(value));
		}
	}
	template<typename Fn> auto update(Fn &&fn) const -> void
	{
		if (!m_update) {
			return;
		}
		m_update([&](T &value) { fn(value); });
	}

private:
	Getter m_get {};
	Setter m_set {};
	Updater m_update {};
	mutable std::optional<T> m_fallback {};
};

class Context
{
public:
	using ComposeFn = std::function<void(Context &)>;

	Context(System &system, Node *root);

	auto text(Id key,
	    std::string_view label,
	    TextStyle style = {},
	    FlexOptions const &options = FlexOptions::builder().build()) -> void;
	auto spacer(Id key, float height) -> void;
	auto icon(Id key, std::string_view icon_name, IconStyle style = {}) -> void;
	auto surface(Id key,
	    FlexOptions const &options,
	    SurfaceStyle style,
	    ComposeFn const &fn) -> void;
	auto pressable(Id key,
	    FlexOptions const &options,
	    std::function<void()> on_activate,
	    bool selectable,
	    ComposeFn const &fn) -> void;
	auto overlay_host(Id key,
	    FlexOptions const &options,
	    OverlayHostSpec const &spec,
	    ComposeFn const &fn) -> void;
	auto layer(Id key,
	    LayerSpec const &spec,
	    FlexOptions const &options,
	    LayerStyle style,
	    ComposeFn const &fn) -> void;
	auto flex(Id key, FlexOptions const &options, ComposeFn const &fn) -> void;
	auto scrollable(Id key,
	    ScrollOptions const &options = Gui::ScrollOptions::builder().build(),
	    ComposeFn const &fn = nullptr) -> void;

	auto text(std::string_view key,
	    std::string_view label,
	    TextStyle style = {},
	    FlexOptions const &options = FlexOptions::builder().build()) -> void
	{
		text(this->id(key), label, style, options);
	}
	auto spacer(std::string_view key, float height) -> void
	{
		spacer(this->id(key), height);
	}
	auto icon(
	    std::string_view key, std::string_view icon_name, IconStyle style = {})
	    -> void
	{
		icon(this->id(key), icon_name, style);
	}
	auto surface(std::string_view key,
	    FlexOptions const &options,
	    SurfaceStyle style,
	    ComposeFn const &fn) -> void
	{
		surface(this->id(key), options, style, fn);
	}
	auto pressable(std::string_view key,
	    FlexOptions const &options,
	    std::function<void()> on_activate,
	    bool selectable,
	    ComposeFn const &fn) -> void
	{
		pressable(
		    this->id(key), options, std::move(on_activate), selectable, fn);
	}
	auto overlay_host(std::string_view key,
	    FlexOptions const &options,
	    OverlayHostSpec const &spec,
	    ComposeFn const &fn) -> void
	{
		overlay_host(this->id(key), options, spec, fn);
	}
	auto layer(std::string_view key,
	    LayerSpec const &spec,
	    FlexOptions const &options,
	    LayerStyle style,
	    ComposeFn const &fn) -> void
	{
		layer(this->id(key), spec, options, style, fn);
	}
	auto flex(
	    std::string_view key, FlexOptions const &options, ComposeFn const &fn)
	    -> void
	{
		flex(this->id(key), options, fn);
	}
	auto scrollable(
	    std::string_view key, ScrollOptions const &options, ComposeFn const &fn)
	    -> void
	{
		scrollable(this->id(key), options, fn);
	}

	auto sidebar_open() const -> bool;
	auto sidebar_visible() const -> bool;
	auto dialog_open() const -> bool;
	auto is_visible() const -> bool;
	auto visibility_pause_condition() const -> std::function<bool()>;
	auto restart_animation(Animation::Ref const &ref) -> void;
	auto sample_animation(Animation::Ref const &ref) -> float;
	auto id(std::string_view key) -> Id;
	auto new_id(std::string_view prefix = "id") -> std::string;
	auto request_recompose() -> void;
	auto system() -> System & { return m_system; }
	template<typename T> auto remember(std::string_view key, T init) -> T &;
	template<typename T> auto remember(Id key, T init) -> T &;
	template<typename T>
	auto mutable_state_of(std::string_view key, T init) -> MutableState<T>;
	template<typename T>
	auto mutable_state_of(Id key, T init) -> MutableState<T>;
	auto selection_mode() const -> bool;
	auto window_rect() const -> Engine::Rect<> const &;
	auto theme() const -> Theme const &;

private:
	auto push_node(Kind kind, Id key, Scope scope, FlexOptions const &options)
	    -> Node *;
	auto push_node(Kind kind, Id key, Scope scope) -> Node *;
	auto resolve_animation_ref(Animation::Ref const &ref, bool restart = false)
	    -> Animation::Ref;
	auto resolve_animated_scalar(std::optional<AnimatedScalar> const &value)
	    -> std::optional<AnimatedScalar>;
	auto pop_node() -> void;

	System &m_system;
	Node *m_root {};
	Node *m_current {};
	Scope m_scope { Scope::Root };
	std::unordered_map<std::string, uint32_t> m_id_counters {};
};

template<typename T>
auto Context::remember(std::string_view const key, T init) -> T &
{
	return remember<T>(this->id(key), std::move(init));
}

template<typename T> auto Context::remember(Id const key, T init) -> T &
{
	if (m_current == nullptr) {
		static thread_local std::optional<T> fallback {};
		fallback = std::move(init);
		return *fallback;
	}
	auto const full_key {
		m_system.state_id(m_current->key, key),
	};
	return m_system.remember_state<T>(full_key, std::move(init));
}

template<typename T>
auto Context::mutable_state_of(std::string_view const key, T init)
    -> MutableState<T>
{
	return mutable_state_of<T>(this->id(key), std::move(init));
}

template<typename T>
auto Context::mutable_state_of(Id const key, T init) -> MutableState<T>
{
	if (m_current == nullptr) {
		return MutableState<T> { {}, {}, {}, std::move(init) };
	}
	auto const full_key {
		m_system.state_id(m_current->key, key),
	};
	m_system.remember_state<T>(full_key, std::move(init));
	return MutableState<T> {
		[system = &m_system, full_key]() -> T const & {
		    return system->remember_state<T>(full_key, T {});
		},
		[system = &m_system, full_key](T value) {
		    system->set_state_if_changed<T>(full_key, std::move(value));
		},
		[system = &m_system, full_key](std::function<void(T &)> updater) {
		    system->update_state<T>(full_key, std::move(updater));
		},
	};
}

} // namespace Gui
