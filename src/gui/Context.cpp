#include "gui/Context.h"

#include <utility>

#include "gui/System.h"

namespace Gui
{

namespace
{
auto animated_value_fallback(FlexOptions::AnimatedFloat const &value) -> float
{
	if (auto const *static_value { std::get_if<float>(&value) }) {
		return *static_value;
	}
	return std::get<Animation::Ref>(value).fallback;
}
} // namespace

auto FlexOptions::builder() -> Builder
{
	return Builder {};
}

auto FlexOptions::width() const -> float
{
	return animated_value_fallback(m_width);
}

auto FlexOptions::height() const -> float
{
	return animated_value_fallback(m_height);
}

auto FlexOptions::Builder::row() -> Builder &
{
	m_options.m_direction = FlexDirection::Row;
	return *this;
}

auto FlexOptions::Builder::row_reverse() -> Builder &
{
	m_options.m_direction = FlexDirection::RowReverse;
	return *this;
}

auto FlexOptions::Builder::column() -> Builder &
{
	m_options.m_direction = FlexDirection::Column;
	return *this;
}

auto FlexOptions::Builder::column_reverse() -> Builder &
{
	m_options.m_direction = FlexDirection::ColumnReverse;
	return *this;
}

auto FlexOptions::Builder::direction(FlexDirection const value) -> Builder &
{
	m_options.m_direction = value;
	return *this;
}

auto FlexOptions::Builder::wrap(FlexWrap const value) -> Builder &
{
	m_options.m_wrap = value;
	return *this;
}

auto FlexOptions::Builder::justify_content(JustifyContent const value)
    -> Builder &
{
	m_options.m_justify_content = value;
	return *this;
}

auto FlexOptions::Builder::align_items(AlignItems const value) -> Builder &
{
	m_options.m_align_items = value;
	return *this;
}

auto FlexOptions::Builder::align_content(AlignContent const value) -> Builder &
{
	m_options.m_align_content = value;
	return *this;
}

auto FlexOptions::Builder::align_self(AlignSelf const value) -> Builder &
{
	m_options.m_align_self = value;
	return *this;
}

auto FlexOptions::Builder::gap(float const value) -> Builder &
{
	m_options.m_gap = value;
	return *this;
}

auto FlexOptions::Builder::row_gap(float const value) -> Builder &
{
	m_options.m_row_gap = value;
	return *this;
}

auto FlexOptions::Builder::column_gap(float const value) -> Builder &
{
	m_options.m_column_gap = value;
	return *this;
}

auto FlexOptions::Builder::width(float const value) -> Builder &
{
	m_options.m_width = value;
	return *this;
}

auto FlexOptions::Builder::width(Animation::Ref value) -> Builder &
{
	m_options.m_width = std::move(value);
	return *this;
}

auto FlexOptions::Builder::height(float const value) -> Builder &
{
	m_options.m_height = value;
	return *this;
}

auto FlexOptions::Builder::height(Animation::Ref value) -> Builder &
{
	m_options.m_height = std::move(value);
	return *this;
}

auto FlexOptions::Builder::flex(float const grow, float const shrink)
    -> Builder &
{
	m_options.m_flex_grow = grow;
	m_options.m_flex_shrink = shrink;
	return *this;
}

auto FlexOptions::Builder::flex_grow(float const value) -> Builder &
{
	m_options.m_flex_grow = value;
	return *this;
}

auto FlexOptions::Builder::flex_shrink(float const value) -> Builder &
{
	m_options.m_flex_shrink = value;
	return *this;
}

auto FlexOptions::Builder::flex_basis_px(float const value) -> Builder &
{
	m_options.m_flex_basis = value;
	return *this;
}

auto FlexOptions::Builder::flex_basis_auto() -> Builder &
{
	m_options.m_flex_basis = -1.0f;
	return *this;
}

auto FlexOptions::Builder::min_width(float const value) -> Builder &
{
	m_options.m_min_width = value;
	return *this;
}

auto FlexOptions::Builder::min_height(float const value) -> Builder &
{
	m_options.m_min_height = value;
	return *this;
}

auto FlexOptions::Builder::max_width(float const value) -> Builder &
{
	m_options.m_max_width = value;
	return *this;
}

auto FlexOptions::Builder::max_height(float const value) -> Builder &
{
	m_options.m_max_height = value;
	return *this;
}

auto FlexOptions::Builder::padding(float const value) -> Builder &
{
	m_options.m_padding = value;
	return *this;
}

auto FlexOptions::Builder::padding(std::array<float, 2> const value)
    -> Builder &
{
	m_options.m_padding = value;
	return *this;
}

auto FlexOptions::Builder::padding(std::array<float, 4> const value)
    -> Builder &
{
	m_options.m_padding = value;
	return *this;
}

auto FlexOptions::Builder::merge(FlexOptions const &options) -> Builder &
{
	if (options.width() > 0.0f
	    || std::holds_alternative<Animation::Ref>(options.width_value())) {
		m_options.m_width = options.width_value();
	}
	if (options.height() > 0.0f
	    || std::holds_alternative<Animation::Ref>(options.height_value())) {
		m_options.m_height = options.height_value();
	}

	if (options.gap() != 0.0f) {
		m_options.m_gap = options.gap();
	}
	if (options.row_gap() != 0.0f) {
		m_options.m_row_gap = options.row_gap();
	}
	if (options.column_gap() != 0.0f) {
		m_options.m_column_gap = options.column_gap();
	}

	if (options.flex_grow() != 0.0f) {
		m_options.m_flex_grow = options.flex_grow();
	}
	if (options.flex_shrink() != 1.0f) {
		m_options.m_flex_shrink = options.flex_shrink();
	}
	if (options.flex_basis() >= 0.0f) {
		m_options.m_flex_basis = options.flex_basis();
	}

	if (options.min_width() > 0.0f) {
		m_options.m_min_width = options.min_width();
	}
	if (options.min_height() > 0.0f) {
		m_options.m_min_height = options.min_height();
	}
	if (options.max_width() > 0.0f) {
		m_options.m_max_width = options.max_width();
	}
	if (options.max_height() > 0.0f) {
		m_options.m_max_height = options.max_height();
	}

	if (options.align_self() != AlignSelf::Auto) {
		m_options.m_align_self = options.align_self();
	}

	return *this;
}

auto FlexOptions::Builder::build() const -> FlexOptions
{
	return m_options;
}

auto ScrollOptions::builder() -> Builder
{
	return Builder {};
}

auto ScrollOptions::Builder::axis(ScrollAxis const value) -> Builder &
{
	m_options.m_axis = value;
	return *this;
}

auto ScrollOptions::Builder::vertical() -> Builder &
{
	m_options.m_axis = ScrollAxis::Vertical;
	return *this;
}

auto ScrollOptions::Builder::horizontal() -> Builder &
{
	m_options.m_axis = ScrollAxis::Horizontal;
	return *this;
}

auto ScrollOptions::Builder::both() -> Builder &
{
	m_options.m_axis = ScrollAxis::Both;
	return *this;
}

auto ScrollOptions::Builder::step(float const value) -> Builder &
{
	m_options.m_step = value;
	return *this;
}

auto ScrollOptions::Builder::max_width(float const value) -> Builder &
{
	m_options.m_max_width = value;
	return *this;
}

auto ScrollOptions::Builder::max_height(float const value) -> Builder &
{
	m_options.m_max_height = value;
	return *this;
}

auto ScrollOptions::Builder::min_width(float const value) -> Builder &
{
	m_options.m_min_width = value;
	return *this;
}

auto ScrollOptions::Builder::min_height(float const value) -> Builder &
{
	m_options.m_min_height = value;
	return *this;
}

auto ScrollOptions::Builder::width(float const value) -> Builder &
{
	m_options.m_width = value;
	return *this;
}

auto ScrollOptions::Builder::height(float const value) -> Builder &
{
	m_options.m_height = value;
	return *this;
}

auto ScrollOptions::Builder::flex(float const grow, float const shrink)
    -> Builder &
{
	m_options.m_flex_grow = grow;
	m_options.m_flex_shrink = shrink;
	return *this;
}

auto ScrollOptions::Builder::flex_grow(float const value) -> Builder &
{
	m_options.m_flex_grow = value;
	return *this;
}

auto ScrollOptions::Builder::flex_shrink(float const value) -> Builder &
{
	m_options.m_flex_shrink = value;
	return *this;
}

auto ScrollOptions::Builder::flex_basis_px(float const value) -> Builder &
{
	m_options.m_flex_basis = value;
	return *this;
}

auto ScrollOptions::Builder::flex_basis_auto() -> Builder &
{
	m_options.m_flex_basis = -1.0f;
	return *this;
}

auto ScrollOptions::Builder::align_self(AlignSelf const value) -> Builder &
{
	m_options.m_align_self = value;
	return *this;
}

auto ScrollOptions::Builder::padding(float const value) -> Builder &
{
	m_options.m_padding = value;
	return *this;
}

auto ScrollOptions::Builder::padding(std::array<float, 2> const value)
    -> Builder &
{
	m_options.m_padding = value;
	return *this;
}

auto ScrollOptions::Builder::padding(std::array<float, 4> const value)
    -> Builder &
{
	m_options.m_padding = value;
	return *this;
}

auto ScrollOptions::Builder::build() const -> ScrollOptions
{
	return m_options;
}

auto ScrollOptions::as_flex_options() const -> FlexOptions
{
	auto builder { FlexOptions::builder() };
	std::visit([&](auto const &value) { builder.padding(value); }, m_padding);
	builder.width(m_width)
	    .height(m_height)
	    .min_width(m_min_width)
	    .min_height(m_min_height)
	    .max_width(m_max_width)
	    .max_height(m_max_height)
	    .flex_grow(m_flex_grow)
	    .flex_shrink(m_flex_shrink)
	    .flex_basis_px(m_flex_basis)
	    .align_self(m_align_self);
	if (m_flex_basis < 0.0f) {
		builder.flex_basis_auto();
	}
	return builder.build();
}

auto TextStyle::builder() -> Builder
{
	return Builder {};
}

auto TextStyle::Builder::copy(TextStyle style) -> Builder &
{
	m_style = style;
	return *this;
}

auto TextStyle::Builder::size(float const value) -> Builder &
{
	m_style.size = value;
	return *this;
}

auto TextStyle::Builder::align_x(TextAlignX const value) -> Builder &
{
	m_style.align_x = value;
	return *this;
}

auto TextStyle::Builder::align_y(TextAlignY const value) -> Builder &
{
	m_style.align_y = value;
	return *this;
}

auto TextStyle::Builder::use_pressable_state(bool const value) -> Builder &
{
	m_style.use_pressable_state = value;
	return *this;
}

auto TextStyle::Builder::color(smath::Vec4 const value) -> Builder &
{
	m_style.color = value;
	return *this;
}

auto TextStyle::Builder::selected_color(smath::Vec4 const value) -> Builder &
{
	m_style.selected_color = value;
	return *this;
}

auto TextStyle::Builder::build() const -> TextStyle
{
	return m_style;
}

auto SurfaceStyle::builder() -> Builder
{
	return Builder {};
}

auto SurfaceStyle::Builder::draw_fill(bool const value) -> Builder &
{
	m_style.draw_fill = value;
	return *this;
}

auto SurfaceStyle::Builder::draw_outline(bool const value) -> Builder &
{
	m_style.draw_outline = value;
	return *this;
}

auto SurfaceStyle::Builder::use_pressable_state(bool const value) -> Builder &
{
	m_style.use_pressable_state = value;
	return *this;
}

auto SurfaceStyle::Builder::radius(float const value) -> Builder &
{
	m_style.radius = value;
	return *this;
}

auto SurfaceStyle::Builder::outline_thickness(float const value) -> Builder &
{
	m_style.outline_thickness = value;
	return *this;
}

auto SurfaceStyle::Builder::fill_color(smath::Vec4 const value) -> Builder &
{
	m_style.fill_color = value;
	return *this;
}

auto SurfaceStyle::Builder::focus_fill_color(smath::Vec4 const value)
    -> Builder &
{
	m_style.focus_fill_color = value;
	return *this;
}

auto SurfaceStyle::Builder::selected_fill_color(smath::Vec4 const value)
    -> Builder &
{
	m_style.selected_fill_color = value;
	return *this;
}

auto SurfaceStyle::Builder::outline_color(smath::Vec4 const value) -> Builder &
{
	m_style.outline_color = value;
	return *this;
}

auto SurfaceStyle::Builder::build() const -> SurfaceStyle
{
	return m_style;
}

auto IconStyle::builder() -> Builder
{
	return Builder {};
}

auto IconStyle::Builder::size(float const value) -> Builder &
{
	m_style.size = value;
	return *this;
}

auto IconStyle::Builder::use_pressable_state(bool const value) -> Builder &
{
	m_style.use_pressable_state = value;
	return *this;
}

auto IconStyle::Builder::tint(smath::Vec4 const value) -> Builder &
{
	m_style.tint = value;
	return *this;
}

auto IconStyle::Builder::selected_tint(smath::Vec4 const value) -> Builder &
{
	m_style.selected_tint = value;
	return *this;
}

auto IconStyle::Builder::build() const -> IconStyle
{
	return m_style;
}

auto LayerStyle::builder() -> Builder
{
	return Builder {};
}

auto LayerStyle::Builder::draw_scrim(bool const value) -> Builder &
{
	m_style.draw_scrim = value;
	return *this;
}

auto LayerStyle::Builder::scrim_color(smath::Vec4 const value) -> Builder &
{
	m_style.scrim_color = value;
	return *this;
}

auto LayerStyle::Builder::draw_fill(bool const value) -> Builder &
{
	m_style.draw_fill = value;
	return *this;
}

auto LayerStyle::Builder::radius(float const value) -> Builder &
{
	m_style.radius = value;
	return *this;
}

auto LayerStyle::Builder::fill_color(smath::Vec4 const value) -> Builder &
{
	m_style.fill_color = value;
	return *this;
}

auto LayerStyle::Builder::build() const -> LayerStyle
{
	return m_style;
}

Context::Context(System &system, Node *const root)
    : m_system(system), m_root(root), m_current(root)
{ }

auto Context::push_node(Kind const kind,
    Id const key,
    Scope const scope,
    FlexOptions const &options) -> Node *
{
	Node *node { m_system.reconcile_node(
		m_current, kind, scope, key, options) };
	m_current = node;
	return node;
}

auto Context::push_node(Kind const kind, Id const key, Scope const scope)
    -> Node *
{
	static auto const defaults { FlexOptions::builder().build() };
	return push_node(kind, key, scope, defaults);
}

auto Context::pop_node() -> void
{
	if (m_current != nullptr && m_current->parent != nullptr) {
		m_current = m_current->parent;
	}
}

auto Context::text(Id const key,
    std::string_view const label,
    TextStyle style,
    FlexOptions const &options) -> void
{
	auto *node { push_node(Kind::Text, key, m_scope, options) };
	node->label = std::string(label);
	node->text_size = style.size;
	node->text_align_x = style.align_x;
	node->text_align_y = style.align_y;
	node->use_pressable_state = style.use_pressable_state;
	node->text_color = style.color;
	node->selected_text_color = style.selected_color;
	pop_node();
}

auto Context::text(std::string_view const key,
    std::string_view const label,
    TextStyle style,
    FlexOptions const &options) -> void
{
	text(id(key), label, style, options);
}

auto Context::icon(
    Id const key, std::string_view const icon_name, IconStyle style) -> void
{
	auto *node { push_node(Kind::Icon, key, m_scope) };
	node->icon_name = std::string(icon_name);
	node->icon_size = style.size;
	node->use_pressable_state = style.use_pressable_state;
	node->icon_tint = style.tint;
	node->selected_icon_tint = style.selected_tint;
	pop_node();
}

auto Context::icon(std::string_view const key,
    std::string_view const icon_name,
    IconStyle style) -> void
{
	icon(id(key), icon_name, style);
}

auto Context::surface(Id const key,
    FlexOptions const &options,
    SurfaceStyle const style,
    ComposeFn const &fn) -> void
{
	auto *node { push_node(Kind::Surface, key, m_scope, options) };
	node->draw_fill = style.draw_fill;
	node->draw_outline = style.draw_outline;
	node->use_pressable_state = style.use_pressable_state;
	node->corner_radius = style.radius;
	node->outline_thickness = style.outline_thickness;
	node->fill_color = style.fill_color;
	node->focus_fill_color = style.focus_fill_color;
	node->selected_fill_color = style.selected_fill_color;
	node->outline_color = style.outline_color;
	fn(*this);
	pop_node();
}

auto Context::surface(std::string_view const key,
    FlexOptions const &options,
    SurfaceStyle const style,
    ComposeFn const &fn) -> void
{
	surface(id(key), options, style, fn);
}

auto Context::pressable(Id const key,
    FlexOptions const &options,
    std::function<void()> on_activate,
    bool const selectable,
    ComposeFn const &fn) -> void
{
	auto *node { push_node(Kind::Pressable, key, m_scope, options) };
	node->interactive = true;
	node->selectable = selectable;
	node->on_activate = std::move(on_activate);
	fn(*this);
	pop_node();
}

auto Context::pressable(std::string_view const key,
    FlexOptions const &options,
    std::function<void()> on_activate,
    bool const selectable,
    ComposeFn const &fn) -> void
{
	pressable(id(key), options, std::move(on_activate), selectable, fn);
}

auto Context::layer(Id const key,
    LayerPresentation const presentation,
    FlexOptions const &options,
    LayerStyle const style,
    ComposeFn const &fn) -> void
{
	auto const previous_scope { m_scope };
	m_scope = presentation == LayerPresentation::Modal ? Scope::Dialog
	                                                   : Scope::Sidebar;
	auto *node { push_node(Kind::Layer, key, m_scope, options) };
	node->layer_presentation = presentation;
	node->draw_scrim = style.draw_scrim;
	node->scrim_color = style.scrim_color;
	node->draw_fill = style.draw_fill;
	node->corner_radius = style.radius;
	node->fill_color = style.fill_color;
	fn(*this);
	pop_node();
	m_scope = previous_scope;
}

auto Context::layer(std::string_view const key,
    LayerPresentation const presentation,
    FlexOptions const &options,
    LayerStyle const style,
    ComposeFn const &fn) -> void
{
	layer(id(key), presentation, options, style, fn);
}

auto Context::memo(Id const key, uint32_t const deps_hash, ComposeFn const &fn)
    -> void
{
	auto *node { push_node(Kind::Memo, key, m_scope) };
	if (m_system.memo_should_recompose(node->key, deps_hash)) {
		fn(*this);
		m_system.memo_store(*node);
	} else {
		m_system.memo_restore(*node);
	}
	pop_node();
}

auto Context::memo(
    std::string_view const key, uint32_t const deps_hash, ComposeFn const &fn)
    -> void
{
	memo(id(key), deps_hash, fn);
}

auto Context::spacer(Id const key, float const height) -> void
{
	auto *node { push_node(Kind::Spacer, key, m_scope) };
	node->fixed_height = height;
	pop_node();
}

auto Context::spacer(std::string_view const key, float const height) -> void
{
	spacer(id(key), height);
}

auto Context::flex(
    Id const key, FlexOptions const &options, ComposeFn const &fn) -> void
{
	push_node(Kind::Flex, key, m_scope, options);
	fn(*this);
	pop_node();
}

auto Context::flex(
    std::string_view const key, FlexOptions const &options, ComposeFn const &fn)
    -> void
{
	flex(id(key), options, fn);
}

auto Context::scrollable(
    Id const key, ScrollOptions const &options, ComposeFn const &fn) -> void
{
	auto *node { push_node(
		Kind::Scrollable, key, m_scope, options.as_flex_options()) };
	node->scroll_axis = options.axis();
	node->scroll_step = options.step();
	node->max_width = options.max_width();
	node->max_height = options.max_height();
	fn(*this);
	pop_node();
}

auto Context::scrollable(std::string_view const key,
    ScrollOptions const &options,
    ComposeFn const &fn) -> void
{
	scrollable(id(key), options, fn);
}

auto Context::sidebar_open() const -> bool
{
	return m_system.sidebar_open();
}

auto Context::sidebar_visible() const -> bool
{
	return m_system.sidebar_visible();
}

auto Context::dialog_open() const -> bool
{
	return m_system.dialog_open();
}

auto Context::is_visible() const -> bool
{
	if (m_scope == Scope::Sidebar) {
		return m_system.sidebar_visible();
	}
	if (m_scope == Scope::Dialog) {
		return m_system.dialog_open();
	}
	return true;
}

auto Context::visibility_pause_condition() const -> std::function<bool()>
{
	auto const scope { m_scope };
	auto *system { &m_system };
	return [scope, system]() {
		if (scope == Scope::Sidebar) {
			return !system->sidebar_visible();
		}
		if (scope == Scope::Dialog) {
			return !system->dialog_open();
		}
		return false;
	};
}

auto Context::sample_animation(Animation::Ref const &ref) -> float
{
	if (m_current == nullptr) {
		return ref.fallback;
	}
	return m_system.sample_animation_ref(ref, m_current->key);
}

auto Context::new_id(std::string_view const prefix) -> std::string
{
	auto const base {
		(prefix.empty() ? std::string { "id" } : std::string { prefix }),
	};
	auto const scope_key {
		m_current != nullptr ? std::format("{:08x}", m_current->key.value)
		                     : std::string { "root" },
	};
	auto const counter_key { scope_key + "/@id/" + base };
	auto &counter { m_id_counters[counter_key] };
	auto const value { counter++ };
	return std::format("{}_{}", base, value);
}

auto Context::request_recompose() -> void
{
	m_system.request_recompose();
}

auto Context::selection_mode() const -> bool
{
	return m_system.selection_mode();
}

auto Context::window_rect() const -> Engine::Rect<> const &
{
	return m_system.window_rect();
}

auto Context::theme() const -> Theme const &
{
	return m_system.theme();
}

} // namespace Gui
