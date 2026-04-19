#include "gui/Context.h"

#include <utility>

#include "gui/System.h"

namespace Gui
{

namespace
{
auto tween_spec_equal(
    Animation::TweenSpec const &a, Animation::TweenSpec const &b) -> bool
{
	if (std::abs(a.from - b.from) > 0.0001f || std::abs(a.to - b.to) > 0.0001f
	    || std::abs(a.duration_seconds - b.duration_seconds) > 0.0001f
	    || std::abs(a.delay_seconds - b.delay_seconds) > 0.0001f
	    || a.easing != b.easing || a.repeat != b.repeat) {
		return false;
	}

	if (a.easing != Animation::Easing::Custom) {
		return true;
	}

	if (static_cast<bool>(a.custom_easing)
	    != static_cast<bool>(b.custom_easing)) {
		return false;
	}
	if (!a.custom_easing && !b.custom_easing) {
		return true;
	}
	return a.custom_easing.target_type() == b.custom_easing.target_type();
}

auto animation_ref_equal(Animation::Ref const &a, Animation::Ref const &b)
    -> bool
{
	return a.key == b.key && a.generation() == b.generation()
	    && std::abs(a.fallback - b.fallback) <= 0.0001f
	    && tween_spec_equal(a.spec, b.spec);
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

auto layer_insets_affect_layout(Node const &node) -> bool
{
	auto const is_animated = [](std::optional<AnimatedScalar> const &value) {
		return value.has_value()
		    && std::holds_alternative<Animation::Ref>(*value);
	};
	if (is_animated(node.visual.left) && node.visual.right.has_value()) {
		return true;
	}
	if (is_animated(node.visual.right)
	    && (node.visual.left.has_value() || node.layout.fixed_width <= 0.0f)) {
		return true;
	}
	if (is_animated(node.visual.top) && node.visual.bottom.has_value()) {
		return true;
	}
	if (is_animated(node.visual.bottom)
	    && (node.visual.top.has_value() || node.layout.fixed_height <= 0.0f)) {
		return true;
	}
	return false;
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

auto FlexOptions::width() const -> float
{
	return animated_value_fallback(m_common.width);
}

auto FlexOptions::height() const -> float
{
	return animated_value_fallback(m_common.height);
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

Context::Context(System &system, Node *const root)
    : m_system(system), m_root(root), m_current(root),
      m_scope(system.root_scope())
{ }

auto Context::push_node(Kind const kind,
    Id const key,
    ScopeId const scope,
    FlexOptions const &options) -> Node *
{
	Node *node { m_system.reconcile_node(
		m_current, kind, scope, key, options) };
	m_current = node;
	return node;
}

auto Context::push_node(Kind const kind, Id const key, ScopeId const scope)
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
		m_scope = m_system.scope_for_role(ScopeRole::Overlay);
		break;
	case LayerFocusMode::Exclusive:
		m_scope = m_system.scope_for_role(ScopeRole::Exclusive);
		break;
	case LayerFocusMode::Passive:
		m_scope = m_system.scope_for_role(ScopeRole::Passive);
		break;
	}
	auto const resolved_top { resolve_animated_scalar(spec.top) };
	auto const resolved_right { resolve_animated_scalar(spec.right) };
	auto const resolved_bottom { resolve_animated_scalar(spec.bottom) };
	auto const resolved_left { resolve_animated_scalar(spec.left) };
	auto resolved_style { style };
	if (style.animated_scrim_opacity.has_value()) {
		resolved_style.animated_scrim_opacity
		    = resolve_animation_ref(*style.animated_scrim_opacity);
	}
	if (style.animated_opacity.has_value()) {
		resolved_style.animated_opacity
		    = resolve_animation_ref(*style.animated_opacity);
	}
	auto *node { push_node(Kind::Layer, key, m_scope, options) };
	auto assign_layer_inset { [&](std::optional<AnimatedScalar> &field,
		                          std::optional<AnimatedScalar> value) {
		if (layout_value_equal(field, value)) {
			if (value.has_value()) {
				auto const *ref { std::get_if<Animation::Ref>(&*value) };
				if (ref != nullptr) {
					m_system.sample_animation_ref(*ref, node->key);
				}
			}
			return;
		}
		field = std::move(value);
		if (field.has_value()) {
			auto const *ref { std::get_if<Animation::Ref>(&*field) };
			if (ref != nullptr) {
				m_system.sample_animation_ref(*ref, node->key);
			}
		}
		if (layer_insets_affect_layout(*node)) {
			m_system.mark_layout_dirty();
		} else {
			m_system.invalidate_world();
		}
	} };
	assign_visual(node->visual.layer_focus_mode, spec.focus_mode, m_system);
	assign_layer_inset(node->visual.top, resolved_top);
	assign_layer_inset(node->visual.right, resolved_right);
	assign_layer_inset(node->visual.bottom, resolved_bottom);
	assign_layer_inset(node->visual.left, resolved_left);
	assign_visual(node->visual.draw_scrim, resolved_style.draw_scrim, m_system);
	assign_visual(
	    node->visual.scrim_color, resolved_style.scrim_color, m_system);
	if (resolved_style.animated_scrim_opacity.has_value()) {
		auto const animated_changed {
			!node->visual.animated_scrim_opacity.has_value()
			    || node->visual.animated_scrim_opacity->key
			        != resolved_style.animated_scrim_opacity->key
			    || node->visual.animated_scrim_opacity->generation()
			        != resolved_style.animated_scrim_opacity->generation(),
		};
		if (animated_changed) {
			assign_visual(node->visual.scrim_opacity,
			    resolved_style.scrim_opacity,
			    m_system);
		}
		node->visual.animated_scrim_opacity
		    = resolved_style.animated_scrim_opacity;
	} else {
		if (node->visual.animated_scrim_opacity.has_value()) {
			node->visual.animated_scrim_opacity.reset();
			m_system.mark_visual_dirty();
		}
		assign_visual(
		    node->visual.scrim_opacity, resolved_style.scrim_opacity, m_system);
	}
	assign_visual(node->visual.draw_fill, resolved_style.draw_fill, m_system);
	assign_visual(node->visual.corner_radius, resolved_style.radius, m_system);
	assign_visual(node->visual.fill_color, resolved_style.fill_color, m_system);
	if (resolved_style.animated_opacity.has_value()) {
		auto const animated_changed {
			!node->visual.animated_opacity.has_value()
			    || node->visual.animated_opacity->key
			        != resolved_style.animated_opacity->key
			    || node->visual.animated_opacity->generation()
			        != resolved_style.animated_opacity->generation(),
		};
		if (animated_changed) {
			assign_visual(
			    node->visual.opacity, resolved_style.opacity, m_system);
		}
		node->visual.animated_opacity = resolved_style.animated_opacity;
	} else {
		if (node->visual.animated_opacity.has_value()) {
			node->visual.animated_opacity.reset();
			m_system.mark_visual_dirty();
		}
		assign_visual(node->visual.opacity, resolved_style.opacity, m_system);
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

auto Context::is_visible() const -> bool
{
	if (m_system.scope_focus_pass_through(m_scope)) {
		return true;
	}
	return m_system.scope_present(m_scope);
}

auto Context::visibility_pause_condition() const -> std::function<bool()>
{
	auto const scope { m_scope };
	auto *system { &m_system };
	return [scope, system]() {
		if (system->scope_focus_pass_through(scope)) {
			return false;
		}
		return !system->scope_present(scope);
	};
}

auto Context::resolve_animation_ref(
    Animation::Ref const &ref, bool const restart) -> Animation::Ref
{
	auto resolved { ref };
	if (m_current == nullptr) {
		return resolved;
	}
	if (!resolved.valid()) {
		return resolved;
	}

	auto const state_local_key {
		m_system.id(std::string("@anim/") + resolved.key),
	};
	auto const state_key { m_system.state_id(m_current->key, state_local_key) };
	auto &generation { remember<uint32_t>(state_key, 0u) };
	if (restart) {
		generation += 1;
	}

	resolved.set_generation(generation);
	return resolved;
}

auto Context::resolve_animated_scalar(
    std::optional<AnimatedScalar> const &value) -> std::optional<AnimatedScalar>
{
	if (!value.has_value()) {
		return std::nullopt;
	}
	auto const *ref { std::get_if<Animation::Ref>(&*value) };
	if (ref == nullptr) {
		return value;
	}
	return resolve_animation_ref(*ref);
}

auto Context::restart_animation(Animation::Ref const &ref) -> void
{
	(void)resolve_animation_ref(ref, true);
}

auto Context::sample_animation(Animation::Ref const &ref) -> float
{
	if (m_current == nullptr) {
		return ref.fallback;
	}
	auto const resolved { resolve_animation_ref(ref) };
	return m_system.sample_animation_ref(resolved, m_current->key);
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

} // namespace Gui
