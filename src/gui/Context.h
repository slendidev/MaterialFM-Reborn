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

class FlexOptions
{
public:
	using Padding
	    = std::variant<float, std::array<float, 2>, std::array<float, 4>>;
	using AnimatedFloat = std::variant<float, Animation::Ref>;
	class Builder;

	static auto builder() -> Builder;

	auto padding() const -> Padding const & { return m_padding; }
	auto width() const -> float;
	auto height() const -> float;
	auto width_value() const -> AnimatedFloat const & { return m_width; }
	auto height_value() const -> AnimatedFloat const & { return m_height; }
	auto gap() const -> float { return m_gap; }
	auto row_gap() const -> float
	{
		return m_row_gap >= 0.0f ? m_row_gap : m_gap;
	}
	auto column_gap() const -> float
	{
		return m_column_gap >= 0.0f ? m_column_gap : m_gap;
	}
	auto direction() const -> FlexDirection { return m_direction; }
	auto wrap() const -> FlexWrap { return m_wrap; }
	auto justify_content() const -> JustifyContent { return m_justify_content; }
	auto align_items() const -> AlignItems { return m_align_items; }
	auto align_content() const -> AlignContent { return m_align_content; }
	auto align_self() const -> AlignSelf { return m_align_self; }
	auto flex_grow() const -> float { return m_flex_grow; }
	auto flex_shrink() const -> float { return m_flex_shrink; }
	auto flex_basis() const -> float { return m_flex_basis; }
	auto min_width() const -> float { return m_min_width; }
	auto min_height() const -> float { return m_min_height; }
	auto max_width() const -> float { return m_max_width; }
	auto max_height() const -> float { return m_max_height; }

private:
	FlexOptions() = default;

	Padding m_padding { 0.0f };
	float m_gap { 0.0f };
	float m_row_gap { -1.0f };
	float m_column_gap { -1.0f };
	AnimatedFloat m_width { 0.0f };
	AnimatedFloat m_height { 0.0f };
	float m_flex_grow { 0.0f };
	float m_flex_shrink { 1.0f };
	float m_flex_basis { -1.0f };
	float m_min_width { 0.0f };
	float m_min_height { 0.0f };
	float m_max_width { 0.0f };
	float m_max_height { 0.0f };
	FlexDirection m_direction { FlexDirection::Column };
	FlexWrap m_wrap { FlexWrap::NoWrap };
	JustifyContent m_justify_content { JustifyContent::Start };
	AlignItems m_align_items { AlignItems::Stretch };
	AlignContent m_align_content { AlignContent::Start };
	AlignSelf m_align_self { AlignSelf::Auto };

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
	using Padding = FlexOptions::Padding;
	class Builder;

	static auto builder() -> Builder;

	auto axis() const -> ScrollAxis { return m_axis; }
	auto step() const -> float { return m_step; }
	auto as_flex_options() const -> FlexOptions;
	auto max_width() const -> float { return m_max_width; }
	auto max_height() const -> float { return m_max_height; }
	auto min_width() const -> float { return m_min_width; }
	auto min_height() const -> float { return m_min_height; }

private:
	ScrollOptions() = default;

	Padding m_padding { 0.0f };
	float m_width { 0.0f };
	float m_height { 0.0f };
	float m_flex_grow { 0.0f };
	float m_flex_shrink { 1.0f };
	float m_flex_basis { -1.0f };
	AlignSelf m_align_self { AlignSelf::Auto };
	ScrollAxis m_axis { ScrollAxis::Vertical };
	float m_step { 24.0f };
	float m_min_width { 0.0f };
	float m_min_height { 0.0f };
	float m_max_width {};
	float m_max_height {};

	friend class Builder;
};

class ScrollOptions::Builder
{
public:
	auto axis(ScrollAxis value) -> Builder &;
	auto vertical() -> Builder &;
	auto horizontal() -> Builder &;
	auto both() -> Builder &;
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
	smath::Vec4 color {};
	smath::Vec4 selected_color {};
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
	smath::Vec4 fill_color {};
	smath::Vec4 focus_fill_color {};
	smath::Vec4 selected_fill_color {};
	smath::Vec4 outline_color {};
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
	smath::Vec4 tint {};
	smath::Vec4 selected_tint {};
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
	smath::Vec4 scrim_color {};
	bool draw_fill { true };
	float radius {};
	smath::Vec4 fill_color {};
};

class LayerStyle::Builder
{
public:
	auto draw_scrim(bool value) -> Builder &;
	auto scrim_color(smath::Vec4 value) -> Builder &;
	auto draw_fill(bool value) -> Builder &;
	auto radius(float value) -> Builder &;
	auto fill_color(smath::Vec4 value) -> Builder &;
	auto build() const -> LayerStyle;

private:
	LayerStyle m_style {};
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

	auto get() const -> T const &
	{
		if (m_get) {
			return m_get();
		}
		static T fallback {};
		return fallback;
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
	auto memo(Id key, uint32_t deps_hash, ComposeFn const &fn) -> void;
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
	auto layer(Id key,
	    LayerPresentation presentation,
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
	    FlexOptions const &options = FlexOptions::builder().build()) -> void;
	auto memo(std::string_view key, uint32_t deps_hash, ComposeFn const &fn)
	    -> void;
	auto spacer(std::string_view key, float height) -> void;
	auto icon(
	    std::string_view key, std::string_view icon_name, IconStyle style = {})
	    -> void;
	auto surface(std::string_view key,
	    FlexOptions const &options,
	    SurfaceStyle style,
	    ComposeFn const &fn) -> void;
	auto pressable(std::string_view key,
	    FlexOptions const &options,
	    std::function<void()> on_activate,
	    bool selectable,
	    ComposeFn const &fn) -> void;
	auto layer(std::string_view key,
	    LayerPresentation presentation,
	    FlexOptions const &options,
	    LayerStyle style,
	    ComposeFn const &fn) -> void;
	auto flex(
	    std::string_view key, FlexOptions const &options, ComposeFn const &fn)
	    -> void;
	auto scrollable(
	    std::string_view key, ScrollOptions const &options, ComposeFn const &fn)
	    -> void;

	auto sidebar_open() const -> bool;
	auto sidebar_visible() const -> bool;
	auto dialog_open() const -> bool;
	auto is_visible() const -> bool;
	auto visibility_pause_condition() const -> std::function<bool()>;
	auto sample_animation(Animation::Ref const &ref) -> float;
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
	return remember<T>(id(key), std::move(init));
}

template<typename T> auto Context::remember(Id const key, T init) -> T &
{
	if (m_current == nullptr) {
		static T fallback {};
		fallback = std::move(init);
		return fallback;
	}
	auto const full_key {
		combine_id(combine_id(m_current->key, id("@state")), key),
	};
	return m_system.remember_state<T>(full_key, std::move(init));
}

template<typename T>
auto Context::mutable_state_of(std::string_view const key, T init)
    -> MutableState<T>
{
	return mutable_state_of<T>(id(key), std::move(init));
}

template<typename T>
auto Context::mutable_state_of(Id const key, T init) -> MutableState<T>
{
	if (m_current == nullptr) {
		return MutableState<T> {};
	}
	auto const full_key {
		combine_id(combine_id(m_current->key, id("@state")), key),
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
