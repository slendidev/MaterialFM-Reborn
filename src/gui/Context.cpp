#include "gui/Context.h"

#include <utility>

#include "gui/System.h"

namespace Gui
{

namespace
{
auto animation_ref_equal(Animation::Ref const &a, Animation::Ref const &b)
    -> bool
{
	return a.key == b.key && a.generation == b.generation
	    && std::abs(a.fallback - b.fallback) <= 0.0001f;
}

auto animated_scalar_equal(AnimatedScalar const &a, AnimatedScalar const &b)
    -> bool
{
	if (a.index() != b.index()) {
		return false;
	}
	if (auto const *lhs { std::get_if<float>(&a) }) {
		return std::abs(*lhs - std::get<float>(b)) <= 0.0001f;
	}
	return animation_ref_equal(
	    std::get<Animation::Ref>(a), std::get<Animation::Ref>(b));
}

auto layout_value_equal(std::optional<AnimatedScalar> const &a,
    std::optional<AnimatedScalar> const &b) -> bool
{
	if (a.has_value() != b.has_value()) {
		return false;
	}
	if (!a.has_value()) {
		return true;
	}
	return animated_scalar_equal(*a, *b);
}

auto animated_value_fallback(
    std::optional<FlexOptions::AnimatedFloat> const &value) -> float
{
	if (!value.has_value()) {
		return 0.0f;
	}
	if (auto const *static_value { std::get_if<float>(&*value) }) {
		return *static_value;
	}
	return std::get<Animation::Ref>(*value).fallback;
}

template<typename T>
auto assign_layout(T &field, T value, System &system) -> void
{
	if constexpr (std::is_same_v<T, std::optional<AnimatedScalar>>) {
		if (layout_value_equal(field, value)) {
			return;
		}
	} else if constexpr (requires(T const &a, T const &b) {
		                     { a == b } -> std::convertible_to<bool>;
	                     }) {
		if (field == value) {
			return;
		}
	}
	field = std::move(value);
	system.mark_layout_dirty();
}

template<typename T>
auto assign_visual(T &field, T value, System &system) -> void
{
	if constexpr (requires(T const &a, T const &b) {
		              { a == b } -> std::convertible_to<bool>;
	              }) {
		if (field == value) {
			return;
		}
	}
	field = std::move(value);
	system.mark_visual_dirty();
}

template<typename T>
auto assign_world(T &field, T value, System &system) -> void
{
	if constexpr (requires(T const &a, T const &b) {
		              { a == b } -> std::convertible_to<bool>;
	              }) {
		if (field == value) {
			return;
		}
	}
	field = std::move(value);
	system.invalidate_world();
}
} // namespace

auto FlexOptions::builder() -> Builder
{
	return Builder {};
}

auto FlexOptions::width() const -> float
{
	return animated_value_fallback(m_common.width);
}

auto FlexOptions::height() const -> float
{
	return animated_value_fallback(m_common.height);
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
	m_options.m_common.align_self = value;
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
	m_options.m_common.width = value;
	return *this;
}

auto FlexOptions::Builder::width(Animation::Ref value) -> Builder &
{
	m_options.m_common.width = std::move(value);
	return *this;
}

auto FlexOptions::Builder::height(float const value) -> Builder &
{
	m_options.m_common.height = value;
	return *this;
}

auto FlexOptions::Builder::height(Animation::Ref value) -> Builder &
{
	m_options.m_common.height = std::move(value);
	return *this;
}

auto FlexOptions::Builder::flex(float const grow, float const shrink)
    -> Builder &
{
	m_options.m_common.flex_grow = grow;
	m_options.m_common.flex_shrink = shrink;
	return *this;
}

auto FlexOptions::Builder::flex_grow(float const value) -> Builder &
{
	m_options.m_common.flex_grow = value;
	return *this;
}

auto FlexOptions::Builder::flex_shrink(float const value) -> Builder &
{
	m_options.m_common.flex_shrink = value;
	return *this;
}

auto FlexOptions::Builder::flex_basis_px(float const value) -> Builder &
{
	m_options.m_common.flex_basis = value;
	return *this;
}

auto FlexOptions::Builder::flex_basis_auto() -> Builder &
{
	m_options.m_common.flex_basis.reset();
	return *this;
}

auto FlexOptions::Builder::min_width(float const value) -> Builder &
{
	m_options.m_common.min_width = value;
	return *this;
}

auto FlexOptions::Builder::min_height(float const value) -> Builder &
{
	m_options.m_common.min_height = value;
	return *this;
}

auto FlexOptions::Builder::max_width(float const value) -> Builder &
{
	m_options.m_common.max_width = value;
	return *this;
}

auto FlexOptions::Builder::max_height(float const value) -> Builder &
{
	m_options.m_common.max_height = value;
	return *this;
}

auto FlexOptions::Builder::padding(float const value) -> Builder &
{
	m_options.m_common.padding = value;
	return *this;
}

auto FlexOptions::Builder::padding(std::array<float, 2> const value)
    -> Builder &
{
	m_options.m_common.padding = value;
	return *this;
}

auto FlexOptions::Builder::padding(std::array<float, 4> const value)
    -> Builder &
{
	m_options.m_common.padding = value;
	return *this;
}

auto FlexOptions::Builder::merge(FlexOptions const &options) -> Builder &
{
	if (options.has_width()) {
		m_options.m_common.width = options.width_value();
	}
	if (options.has_height()) {
		m_options.m_common.height = options.height_value();
	}

	if (options.has_gap()) {
		m_options.m_gap = options.gap();
	}
	if (options.has_row_gap()) {
		m_options.m_row_gap = options.row_gap();
	}
	if (options.has_column_gap()) {
		m_options.m_column_gap = options.column_gap();
	}

	if (options.flex_grow() != 0.0f) {
		m_options.m_common.flex_grow = options.flex_grow();
	}
	if (options.flex_shrink() != 1.0f) {
		m_options.m_common.flex_shrink = options.flex_shrink();
	}
	if (options.has_flex_basis()) {
		m_options.m_common.flex_basis = options.flex_basis();
	}

	if (options.has_min_width()) {
		m_options.m_common.min_width = options.min_width();
	}
	if (options.has_min_height()) {
		m_options.m_common.min_height = options.min_height();
	}
	if (options.has_max_width()) {
		m_options.m_common.max_width = options.max_width();
	}
	if (options.has_max_height()) {
		m_options.m_common.max_height = options.max_height();
	}

	if (options.align_self() != AlignSelf::Auto) {
		m_options.m_common.align_self = options.align_self();
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

auto OverlayHostSpec::builder() -> Builder
{
	return Builder {};
}

auto OverlayHostSpec::Builder::clip_to_bounds(bool const value) -> Builder &
{
	m_spec.clip_to_bounds = value;
	return *this;
}

auto OverlayHostSpec::Builder::z_index(int const value) -> Builder &
{
	m_spec.z_index = value;
	return *this;
}

auto OverlayHostSpec::Builder::build() const -> OverlayHostSpec
{
	return m_spec;
}

auto LayerSpec::builder() -> Builder
{
	return Builder {};
}

auto LayerSpec::Builder::focus_mode(LayerFocusMode const value) -> Builder &
{
	m_spec.focus_mode = value;
	return *this;
}

auto LayerSpec::Builder::z_index(int const value) -> Builder &
{
	m_spec.z_index = value;
	return *this;
}

auto LayerSpec::Builder::overlay() -> Builder &
{
	return focus_mode(LayerFocusMode::Overlay);
}

auto LayerSpec::Builder::exclusive() -> Builder &
{
	return focus_mode(LayerFocusMode::Exclusive);
}

auto LayerSpec::Builder::passive() -> Builder &
{
	return focus_mode(LayerFocusMode::Passive);
}

auto LayerSpec::Builder::top(float const value) -> Builder &
{
	m_spec.top = value;
	return *this;
}

auto LayerSpec::Builder::top(Animation::Ref value) -> Builder &
{
	m_spec.top = std::move(value);
	return *this;
}

auto LayerSpec::Builder::right(float const value) -> Builder &
{
	m_spec.right = value;
	return *this;
}

auto LayerSpec::Builder::right(Animation::Ref value) -> Builder &
{
	m_spec.right = std::move(value);
	return *this;
}

auto LayerSpec::Builder::bottom(float const value) -> Builder &
{
	m_spec.bottom = value;
	return *this;
}

auto LayerSpec::Builder::bottom(Animation::Ref value) -> Builder &
{
	m_spec.bottom = std::move(value);
	return *this;
}

auto LayerSpec::Builder::left(float const value) -> Builder &
{
	m_spec.left = value;
	return *this;
}

auto LayerSpec::Builder::left(Animation::Ref value) -> Builder &
{
	m_spec.left = std::move(value);
	return *this;
}

auto LayerSpec::Builder::build() const -> LayerSpec
{
	return m_spec;
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

auto ScrollOptions::Builder::reveal_mode(ScrollRevealMode const value)
    -> Builder &
{
	m_options.m_reveal_mode = value;
	return *this;
}

auto ScrollOptions::Builder::step(float const value) -> Builder &
{
	m_options.m_step = value;
	return *this;
}

auto ScrollOptions::Builder::max_width(float const value) -> Builder &
{
	m_options.m_common.max_width = value;
	return *this;
}

auto ScrollOptions::Builder::max_height(float const value) -> Builder &
{
	m_options.m_common.max_height = value;
	return *this;
}

auto ScrollOptions::Builder::min_width(float const value) -> Builder &
{
	m_options.m_common.min_width = value;
	return *this;
}

auto ScrollOptions::Builder::min_height(float const value) -> Builder &
{
	m_options.m_common.min_height = value;
	return *this;
}

auto ScrollOptions::Builder::width(float const value) -> Builder &
{
	m_options.m_common.width = value;
	return *this;
}

auto ScrollOptions::Builder::height(float const value) -> Builder &
{
	m_options.m_common.height = value;
	return *this;
}

auto ScrollOptions::Builder::flex(float const grow, float const shrink)
    -> Builder &
{
	m_options.m_common.flex_grow = grow;
	m_options.m_common.flex_shrink = shrink;
	return *this;
}

auto ScrollOptions::Builder::flex_grow(float const value) -> Builder &
{
	m_options.m_common.flex_grow = value;
	return *this;
}

auto ScrollOptions::Builder::flex_shrink(float const value) -> Builder &
{
	m_options.m_common.flex_shrink = value;
	return *this;
}

auto ScrollOptions::Builder::flex_basis_px(float const value) -> Builder &
{
	m_options.m_common.flex_basis = value;
	return *this;
}

auto ScrollOptions::Builder::flex_basis_auto() -> Builder &
{
	m_options.m_common.flex_basis.reset();
	return *this;
}

auto ScrollOptions::Builder::align_self(AlignSelf const value) -> Builder &
{
	m_options.m_common.align_self = value;
	return *this;
}

auto ScrollOptions::Builder::padding(float const value) -> Builder &
{
	m_options.m_common.padding = value;
	return *this;
}

auto ScrollOptions::Builder::padding(std::array<float, 2> const value)
    -> Builder &
{
	m_options.m_common.padding = value;
	return *this;
}

auto ScrollOptions::Builder::padding(std::array<float, 4> const value)
    -> Builder &
{
	m_options.m_common.padding = value;
	return *this;
}

auto ScrollOptions::Builder::build() const -> ScrollOptions
{
	return m_options;
}

auto ScrollOptions::as_flex_options() const -> FlexOptions
{
	auto builder { FlexOptions::builder() };
	std::visit(
	    [&](auto const &value) { builder.padding(value); }, m_common.padding);
	if (m_common.width.has_value()) {
		if (auto const *value { std::get_if<float>(&*m_common.width) }) {
			builder.width(*value);
		} else {
			builder.width(std::get<Animation::Ref>(*m_common.width));
		}
	}
	if (m_common.height.has_value()) {
		if (auto const *value { std::get_if<float>(&*m_common.height) }) {
			builder.height(*value);
		} else {
			builder.height(std::get<Animation::Ref>(*m_common.height));
		}
	}
	if (m_common.min_width.has_value()) {
		builder.min_width(*m_common.min_width);
	}
	if (m_common.min_height.has_value()) {
		builder.min_height(*m_common.min_height);
	}
	if (m_common.max_width.has_value()) {
		builder.max_width(*m_common.max_width);
	}
	if (m_common.max_height.has_value()) {
		builder.max_height(*m_common.max_height);
	}
	builder.flex_grow(m_common.flex_grow)
	    .flex_shrink(m_common.flex_shrink)
	    .align_self(m_common.align_self);
	if (m_common.flex_basis.has_value()) {
		builder.flex_basis_px(*m_common.flex_basis);
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

auto LayerStyle::Builder::scrim_opacity(float const value) -> Builder &
{
	m_style.scrim_opacity = value;
	m_style.animated_scrim_opacity.reset();
	return *this;
}

auto LayerStyle::Builder::scrim_opacity(Animation::Ref value) -> Builder &
{
	m_style.scrim_opacity = value.fallback;
	m_style.animated_scrim_opacity = std::move(value);
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

auto LayerStyle::Builder::opacity(float const value) -> Builder &
{
	m_style.opacity = value;
	m_style.animated_opacity.reset();
	return *this;
}

auto LayerStyle::Builder::opacity(Animation::Ref value) -> Builder &
{
	m_style.opacity = value.fallback;
	m_style.animated_opacity = std::move(value);
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
	assign_layout(node->content.label, std::string(label), m_system);
	assign_layout(node->content.text_size, style.size, m_system);
	assign_visual(node->content.text_align_x, style.align_x, m_system);
	assign_visual(node->content.text_align_y, style.align_y, m_system);
	assign_visual(node->interaction.use_pressable_state,
	    style.use_pressable_state,
	    m_system);
	assign_visual(node->visual.text_color, style.color, m_system);
	assign_visual(
	    node->visual.selected_text_color, style.selected_color, m_system);
	pop_node();
}

auto Context::icon(
    Id const key, std::string_view const icon_name, IconStyle style) -> void
{
	auto *node { push_node(Kind::Icon, key, m_scope) };
	assign_visual(node->content.icon_name, std::string(icon_name), m_system);
	assign_layout(node->content.icon_size, style.size, m_system);
	assign_visual(node->interaction.use_pressable_state,
	    style.use_pressable_state,
	    m_system);
	assign_visual(node->visual.icon_tint, style.tint, m_system);
	assign_visual(
	    node->visual.selected_icon_tint, style.selected_tint, m_system);
	pop_node();
}

auto Context::surface(Id const key,
    FlexOptions const &options,
    SurfaceStyle const style,
    ComposeFn const &fn) -> void
{
	auto *node { push_node(Kind::Surface, key, m_scope, options) };
	assign_visual(node->visual.draw_fill, style.draw_fill, m_system);
	assign_visual(node->visual.draw_outline, style.draw_outline, m_system);
	assign_visual(node->interaction.use_pressable_state,
	    style.use_pressable_state,
	    m_system);
	assign_visual(node->visual.corner_radius, style.radius, m_system);
	assign_visual(
	    node->visual.outline_thickness, style.outline_thickness, m_system);
	assign_visual(node->visual.fill_color, style.fill_color, m_system);
	assign_visual(
	    node->visual.focus_fill_color, style.focus_fill_color, m_system);
	assign_visual(
	    node->visual.selected_fill_color, style.selected_fill_color, m_system);
	assign_visual(node->visual.outline_color, style.outline_color, m_system);
	fn(*this);
	pop_node();
}

auto Context::pressable(Id const key,
    FlexOptions const &options,
    std::function<void()> on_activate,
    bool const selectable,
    ComposeFn const &fn) -> void
{
	auto *node { push_node(Kind::Pressable, key, m_scope, options) };
	assign_visual(node->interaction.interactive, true, m_system);
	assign_visual(node->interaction.selectable, selectable, m_system);
	node->interaction.on_activate = std::move(on_activate);
	fn(*this);
	pop_node();
}

auto Context::overlay_host(Id const key,
    FlexOptions const &options,
    OverlayHostSpec const &spec,
    ComposeFn const &fn) -> void
{
	(void)key;
	(void)options;
	(void)spec;
	fn(*this);
}

auto Context::layer(Id const key,
    LayerSpec const &spec,
    FlexOptions const &options,
    LayerStyle const style,
    ComposeFn const &fn) -> void
{
	auto const previous_scope { m_scope };
	switch (spec.focus_mode) {
	case LayerFocusMode::Inherit:
		break;
	case LayerFocusMode::Overlay:
		m_scope = Scope::Sidebar;
		break;
	case LayerFocusMode::Exclusive:
		m_scope = Scope::Dialog;
		break;
	case LayerFocusMode::Passive:
		m_scope = Scope::Hud;
		break;
	}
	auto *node { push_node(Kind::Layer, key, m_scope, options) };
	assign_visual(node->visual.layer_focus_mode, spec.focus_mode, m_system);
	assign_layout(node->visual.top, spec.top, m_system);
	assign_layout(node->visual.right, spec.right, m_system);
	assign_layout(node->visual.bottom, spec.bottom, m_system);
	assign_layout(node->visual.left, spec.left, m_system);
	assign_visual(node->visual.draw_scrim, style.draw_scrim, m_system);
	assign_visual(node->visual.scrim_color, style.scrim_color, m_system);
	if (style.animated_scrim_opacity.has_value()) {
		auto const animated_changed {
			!node->visual.animated_scrim_opacity.has_value()
			    || node->visual.animated_scrim_opacity->key
			        != style.animated_scrim_opacity->key
			    || node->visual.animated_scrim_opacity->generation
			        != style.animated_scrim_opacity->generation,
		};
		if (animated_changed) {
			assign_visual(
			    node->visual.scrim_opacity, style.scrim_opacity, m_system);
		}
		node->visual.animated_scrim_opacity = style.animated_scrim_opacity;
	} else {
		if (node->visual.animated_scrim_opacity.has_value()) {
			node->visual.animated_scrim_opacity.reset();
			m_system.mark_visual_dirty();
		}
		assign_visual(
		    node->visual.scrim_opacity, style.scrim_opacity, m_system);
	}
	assign_visual(node->visual.draw_fill, style.draw_fill, m_system);
	assign_visual(node->visual.corner_radius, style.radius, m_system);
	assign_visual(node->visual.fill_color, style.fill_color, m_system);
	if (style.animated_opacity.has_value()) {
		auto const animated_changed {
			!node->visual.animated_opacity.has_value()
			    || node->visual.animated_opacity->key
			        != style.animated_opacity->key
			    || node->visual.animated_opacity->generation
			        != style.animated_opacity->generation,
		};
		if (animated_changed) {
			assign_visual(node->visual.opacity, style.opacity, m_system);
		}
		node->visual.animated_opacity = style.animated_opacity;
	} else {
		if (node->visual.animated_opacity.has_value()) {
			node->visual.animated_opacity.reset();
			m_system.mark_visual_dirty();
		}
		assign_visual(node->visual.opacity, style.opacity, m_system);
	}
	fn(*this);
	pop_node();
	m_scope = previous_scope;
}

auto Context::spacer(Id const key, float const height) -> void
{
	auto *node { push_node(Kind::Spacer, key, m_scope) };
	assign_layout(node->layout.fixed_height, height, m_system);
	pop_node();
}

auto Context::flex(
    Id const key, FlexOptions const &options, ComposeFn const &fn) -> void
{
	push_node(Kind::Flex, key, m_scope, options);
	fn(*this);
	pop_node();
}

auto Context::scrollable(
    Id const key, ScrollOptions const &options, ComposeFn const &fn) -> void
{
	auto *node { push_node(
		Kind::Scrollable, key, m_scope, options.as_flex_options()) };
	assign_layout(node->scroll.scroll_axis, options.axis(), m_system);
	assign_layout(
	    node->scroll.scroll_reveal_mode, options.reveal_mode(), m_system);
	assign_world(node->scroll.scroll_step, options.step(), m_system);
	assign_layout(node->layout.max_width, options.max_width(), m_system);
	assign_layout(node->layout.max_height, options.max_height(), m_system);
	if (fn) {
		fn(*this);
	}
	pop_node();
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
	if (m_scope == Scope::Hud) {
		return true;
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
		if (scope == Scope::Hud) {
			return false;
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

auto Context::id(std::string_view const key) -> Id
{
	return m_system.id(key);
}

auto Context::new_id(std::string_view const prefix) -> std::string
{
	auto const base {
		(prefix.empty() ? std::string { "id" } : std::string { prefix }),
	};
	auto const scope_key {
		m_current != nullptr ? std::string { m_current->key.label() }
		                     : std::string { "root" },
	};
	auto const counter_key { scope_key + "/@id/" + base };
	auto &counter { m_id_counters[counter_key] };
	auto const value { counter++ };
	return std::format("{}_{}", base, value);
}

auto Context::request_recompose() -> void
{
	m_system.invalidate_compose();
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
