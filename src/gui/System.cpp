#include "gui/System.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <format>
#include <fstream>
#include <limits>
#include <optional>
#include <print>
#include <type_traits>
#include <unordered_set>
#include <utility>

#include "gui/Context.h"

namespace Gui
{
namespace
{
constexpr float HOLD_THRESHOLD_SECONDS { 0.35f };
constexpr float DEFAULT_DRAWER_WIDTH { 184.0f };
constexpr float DEFAULT_MODAL_WIDTH { 280.0f };
constexpr float DEFAULT_MODAL_HEIGHT { 170.0f };

[[gnu::always_inline]] inline auto approx_equal(
    float const a, float const b, float const epsilon = 0.0001f) -> bool
{
	return std::abs(a - b) <= epsilon;
}

[[gnu::always_inline]] inline auto rect_equal(
    Engine::Rect<> const &a, Engine::Rect<> const &b) -> bool
{
	return a.position.approx_equal(b.position) && a.size.approx_equal(b.size);
}

auto tween_spec_equal(
    Animation::TweenSpec const &a, Animation::TweenSpec const &b) -> bool
{
	if (!approx_equal(a.from, b.from) || !approx_equal(a.to, b.to)
	    || !approx_equal(a.duration_seconds, b.duration_seconds)
	    || !approx_equal(a.delay_seconds, b.delay_seconds)
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

auto resolve_animated_scalar(std::optional<AnimatedScalar> const &value,
    System &system,
    Id const owner_key) -> std::optional<float>
{
	if (!value.has_value()) {
		return std::nullopt;
	}
	if (auto const *static_value { std::get_if<float>(&*value) }) {
		return *static_value;
	}
	return system.sample_animation_ref(
	    std::get<Animation::Ref>(*value), owner_key);
}

auto inset_animation_affects_layout(Node const &node) -> bool
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

auto has_animated_inset(Node const &node) -> bool
{
	auto const is_animated = [](std::optional<AnimatedScalar> const &value) {
		return value.has_value()
		    && std::holds_alternative<Animation::Ref>(*value);
	};
	return is_animated(node.visual.top) || is_animated(node.visual.right)
	    || is_animated(node.visual.bottom) || is_animated(node.visual.left);
}

auto refresh_layer_local_rect_from_insets(
    Node &node, System &system, Engine::Rect<> const &window_rect) -> void
{
	if (node.kind != Kind::Layer) {
		return;
	}

	auto const top { resolve_animated_scalar(
		node.visual.top, system, node.key) };
	auto const right {
		resolve_animated_scalar(node.visual.right, system, node.key),
	};
	auto const bottom {
		resolve_animated_scalar(node.visual.bottom, system, node.key),
	};
	auto const left {
		resolve_animated_scalar(node.visual.left, system, node.key),
	};
	auto const container_width { window_rect.size.x() };
	auto const container_height { window_rect.size.y() };

	float rect_width {
		node.layout.fixed_width > 0.0f ? node.layout.fixed_width
		                               : container_width,
	};
	float rect_height { node.layout.fixed_height > 0.0f
		    ? node.layout.fixed_height
		    : container_height };
	float rect_x {};
	float rect_y {};

	if (left.has_value()) {
		rect_x = *left;
	}
	if (top.has_value()) {
		rect_y = *top;
	}
	if (left.has_value() && right.has_value()) {
		rect_width = std::max(0.0f, container_width - *left - *right);
	} else if (right.has_value()) {
		if (rect_width <= 0.0f) {
			rect_width = std::max(0.0f, container_width - *right);
		} else {
			rect_x = std::max(0.0f, container_width - *right - rect_width);
		}
	}
	if (top.has_value() && bottom.has_value()) {
		rect_height = std::max(0.0f, container_height - *top - *bottom);
	} else if (bottom.has_value()) {
		if (rect_height <= 0.0f) {
			rect_height = std::max(0.0f, container_height - *bottom);
		} else {
			rect_y = std::max(0.0f, container_height - *bottom - rect_height);
		}
	}
	if (!left.has_value() && !right.has_value() && rect_width <= 0.0f) {
		rect_width = container_width;
	}
	if (!top.has_value() && !bottom.has_value() && rect_height <= 0.0f) {
		rect_height = container_height;
	}

	node.layout.local_rect.position = smath::Vec2 { rect_x, rect_y };
	node.layout.local_rect.size = smath::Vec2 { rect_width, rect_height };
}

auto resolve_tween_track_key(
    Animation::Ref const &ref, System &system, Id const owner_key) -> Id
{
	if (!ref.valid()) {
		return {};
	}
	if (ref.key.starts_with("root/")) {
		return system.id(ref.key);
	}
	return system.tween_id(owner_key, ref.key);
}

auto inset_tween_changed(Node const &node,
    std::unordered_set<Id, Id::Hash> const &changed_tweens,
    System &system) -> bool
{
	auto const ref_changed = [&](std::optional<AnimatedScalar> const &value) {
		if (!value.has_value()) {
			return false;
		}
		auto const *ref { std::get_if<Animation::Ref>(&*value) };
		if (ref == nullptr) {
			return false;
		}
		auto const key { resolve_tween_track_key(*ref, system, node.key) };
		return key.valid() && changed_tweens.contains(key);
	};

	return ref_changed(node.visual.top) || ref_changed(node.visual.right)
	    || ref_changed(node.visual.bottom) || ref_changed(node.visual.left);
}

auto resolve_align_items(AlignItems const container_align, AlignSelf const self)
    -> AlignItems
{
	if (self == AlignSelf::Auto) {
		return container_align;
	}

	switch (self) {
	case AlignSelf::Stretch:
		return AlignItems::Stretch;
	case AlignSelf::Start:
		return AlignItems::Start;
	case AlignSelf::End:
		return AlignItems::End;
	case AlignSelf::Center:
		return AlignItems::Center;
	case AlignSelf::Auto:
	default:
		return container_align;
	}
}

struct ResolvedPadding
{
	float top {};
	float right {};
	float bottom {};
	float left {};
};

auto resolve_padding(FlexOptions::Padding const &padding) -> ResolvedPadding
{
	return std::visit(
	    [](auto const &value) -> ResolvedPadding {
		    using Value = std::decay_t<decltype(value)>;
		    if constexpr (std::is_same_v<Value, float>) {
			    return ResolvedPadding {
				    .top = value,
				    .right = value,
				    .bottom = value,
				    .left = value,
			    };
		    } else if constexpr (std::is_same_v<Value, std::array<float, 2>>) {
			    return ResolvedPadding {
				    .top = value[0],
				    .right = value[1],
				    .bottom = value[0],
				    .left = value[1],
			    };
		    } else {
			    return ResolvedPadding {
				    .top = value[0],
				    .right = value[1],
				    .bottom = value[2],
				    .left = value[3],
			    };
		    }
	    },
	    padding);
}

auto is_row_direction(FlexDirection const direction) -> bool
{
	return direction == FlexDirection::Row
	    || direction == FlexDirection::RowReverse;
}

auto is_reverse_direction(FlexDirection const direction) -> bool
{
	return direction == FlexDirection::RowReverse
	    || direction == FlexDirection::ColumnReverse;
}

inline auto clamp_size(System::MeasuredSize size, Node const &node)
    -> System::MeasuredSize
{
	if (node.layout.min_width > 0.0f) {
		size.width = std::max(size.width, node.layout.min_width);
	}
	if (node.layout.min_height > 0.0f) {
		size.height = std::max(size.height, node.layout.min_height);
	}
	if (node.layout.max_width > 0.0f) {
		size.width = std::min(size.width, node.layout.max_width);
	}
	if (node.layout.max_height > 0.0f) {
		size.height = std::min(size.height, node.layout.max_height);
	}
	return size;
}

[[gnu::always_inline]] inline auto spans_overlap(
    float const a0, float const a1, float const b0, float const b1) -> bool
{
	return a1 > b0 && b1 > a0;
}

} // namespace

auto System::IdRegistry::intern(std::string_view const value) -> Id
{
	if (value.empty()) {
		return {};
	}

	if (auto const it { lookup.find(value) }; it != lookup.end()) {
		return Id { it->second };
	}

	auto entry { std::make_unique<Id::Entry>() };
	entry->value = std::string(value);
	auto const slash { entry->value.find_last_of('/') };
	entry->short_offset
	    = slash == std::string::npos ? 0u : static_cast<size_t>(slash + 1u);
	auto *stored { entry.get() };
	entries.push_back(std::move(entry));
	lookup.emplace(stored->value, stored);
	return Id { stored };
}

auto System::IdRegistry::compose(Id const parent, Id const child) -> Id
{
	if (!parent.valid()) {
		return child;
	}
	if (!child.valid()) {
		return parent;
	}
	std::string combined {};
	combined.reserve(parent.label().size() + 1 + child.label().size());
	combined += parent.label();
	combined += '/';
	combined += child.label();
	return intern(combined);
}

auto System::IdRegistry::compose(Id const parent, std::string_view const child)
    -> Id
{
	return compose(parent, intern(child));
}

auto System::id(std::string_view const value) const -> Id
{
	return m_ids.intern(value);
}

auto System::compose_id(Id const parent, Id const child) const -> Id
{
	return m_ids.compose(parent, child);
}

auto System::compose_id(Id const parent, std::string_view const child) const
    -> Id
{
	return m_ids.compose(parent, child);
}

auto System::state_id(Id const parent, Id const local_key) const -> Id
{
	return compose_id(compose_id(parent, STATE_SEGMENT), local_key);
}

auto System::tween_id(Id const owner, std::string_view const local_key) const
    -> Id
{
	return compose_id(compose_id(owner, TWEEN_SEGMENT), local_key);
}

auto System::mark_layout_dirty() -> void
{
	if (m_is_composing) {
		m_layout_changed_during_compose = true;
	} else {
		invalidate_layout();
	}
}

auto System::mark_render_cache_dirty() -> void
{
	if (m_is_composing) {
		m_visual_changed_during_compose = true;
	} else {
		invalidate_render_cache();
	}
}

auto System::mark_compose_dirty() -> void
{
	if (m_is_composing) {
		m_structure_changed_during_compose = true;
	} else {
		invalidate_render_cache();
	}
}

auto System::mark_visual_dirty() -> void
{
	if (m_is_composing) {
		m_visual_changed_during_compose = true;
	} else {
		invalidate_visual();
	}
}

auto System::begin_compose_tracking() -> void
{
	m_structure_changed_during_compose = false;
	m_layout_changed_during_compose = false;
	m_visual_changed_during_compose = false;
}

auto System::end_compose_tracking() -> void
{
	if (m_layout_changed_during_compose) {
		m_layout_dirty = true;
		m_world_dirty = true;
		m_render_cache_dirty = true;
		m_visual_dirty = true;
	} else if (m_structure_changed_during_compose) {
		m_render_cache_dirty = true;
		m_visual_dirty = true;
	} else if (m_visual_changed_during_compose) {
		m_visual_dirty = true;
	}
}

auto System::measure_leaf(Node const &node) const -> MeasuredSize
{
	MeasuredSize size {};

	switch (node.kind) {
	case Kind::Text: {
		float measured_width { 0.0f };
		if (m_text_measure_fn) {
			auto const measured { m_text_measure_fn(
				node.content.label, node.content.text_size) };
			measured_width = measured.x() > 0.0f
			    ? std::max(8.0f, measured.x() + 1.5f)
			    : std::max(8.0f,
			          node.content.text_size * 0.56f
			              * static_cast<float>(node.content.label.size()));
		} else {
			measured_width = std::max(8.0f,
			    node.content.text_size * 0.56f
			        * static_cast<float>(node.content.label.size()));
		}

		size.width = measured_width;
		size.height = node.content.text_size + 6.0f;
		break;
	}

	case Kind::Icon: {
		auto const s { std::max(1.0f, node.content.icon_size) };
		size.width = s;
		size.height = s;
		break;
	}

	case Kind::Spacer: {
		size.width = std::max(0.0f, node.layout.fixed_width);
		size.height = std::max(0.0f, node.layout.fixed_height);
		break;
	}

	default:
		break;
	}

	if (node.layout.fixed_width > 0.0f) {
		size.width = node.layout.fixed_width;
	}
	if (node.layout.fixed_height > 0.0f) {
		size.height = node.layout.fixed_height;
	}

	return clamp_size(size, node);
}

auto System::measure_node(Node const &node, float const available_width) const
    -> MeasuredSize
{
	auto const available_width_bits {
		std::bit_cast<uint32_t>(available_width),
	};
	if (auto const it { m_measure_cache.find(&node) };
	    it != m_measure_cache.end()
	    && it->second.available_width_bits == available_width_bits) {
		return it->second.size;
	}

	auto const store_measure { [&](MeasuredSize const size) {
		m_measure_cache[&node] = MeasureCacheEntry {
			.available_width_bits = available_width_bits,
			.size = size,
		};
		return size;
	} };

	if (node.layout.fixed_width > 0.0f || node.layout.fixed_height > 0.0f) {
		auto leaf { measure_leaf(node) };
		if (node.layout.fixed_width > 0.0f) {
			leaf.width = node.layout.fixed_width;
		}
		if (node.layout.fixed_height > 0.0f) {
			leaf.height = node.layout.fixed_height;
		}
		return store_measure(leaf);
	}

	switch (node.kind) {
	case Kind::Text:
	case Kind::Icon:
	case Kind::Spacer:
		return store_measure(measure_leaf(node));
	case Kind::OverlayHost:
		return store_measure(MeasuredSize {});

	case Kind::Scrollable:
	case Kind::Flex:
	case Kind::Surface:
	case Kind::Pressable:
	case Kind::Layer:
	case Kind::Root:
		break;
	}

	auto const is_row { is_row_direction(node.layout.flex_direction) };
	auto const gap_main { is_row ? node.layout.column_gap
		                         : node.layout.row_gap };

	float main_sum { 0.0f };
	float cross_max { 0.0f };
	bool first_child { true };

	for (auto const &child_ptr : node.children) {
		auto const &child { *child_ptr };
		auto child_size { measure_node(child, available_width) };

		if (child.layout.flex_basis >= 0.0f) {
			if (is_row) {
				child_size.width
				    = std::max(child_size.width, child.layout.flex_basis);
			} else {
				child_size.height
				    = std::max(child_size.height, child.layout.flex_basis);
			}
		}

		auto const child_main { is_row ? child_size.width : child_size.height };
		auto const child_cross { is_row ? child_size.height
			                            : child_size.width };

		if (!first_child) {
			main_sum += gap_main;
		}
		first_child = false;

		main_sum += child_main;
		cross_max = std::max(cross_max, child_cross);
	}

	MeasuredSize out {};
	if (is_row) {
		out.width
		    = node.layout.padding_left + main_sum + node.layout.padding_right;
		out.height
		    = node.layout.padding_top + cross_max + node.layout.padding_bottom;
	} else {
		out.width
		    = node.layout.padding_left + cross_max + node.layout.padding_right;
		out.height
		    = node.layout.padding_top + main_sum + node.layout.padding_bottom;
	}

	return store_measure(clamp_size(out, node));
}

System::System()
{
	m_root = std::make_unique<Node>();
	m_root->kind = Kind::Root;
	m_root->scope = Scope::Root;
	m_root->key = this->id("root");
	m_root->local_key = this->id("root");
	m_sidebar_tween.configure(Animation::TweenSpec {
	    .from = 0.0f,
	    .to = 0.0f,
	    .duration_seconds = 0.0001f,
	    .easing = Animation::Easing::Linear,
	    .repeat = Animation::RepeatMode::Once,
	});
	m_sidebar_tween.stop();
}

auto System::set_icon_atlas(uint32_t const image_id, IconAtlas const &atlas)
    -> void
{
	m_icon_image_id = image_id;
	m_icon_rects = atlas.rects;
	m_visual_dirty = true;
	mark_scope_dirty(Scope::Root);
	m_structure_dirty = true;
}

auto System::set_text_measure_fn(
    std::function<smath::Vec2(std::string_view, float)> fn) -> void
{
	m_text_measure_fn = std::move(fn);
	m_layout_dirty = true;
}

auto System::sample_animation_ref(Animation::Ref const &ref, Id const owner_key)
    -> float
{
	return resolve_animated_float(ref, owner_key);
}

auto System::mark_scope_recomposed(Scope const scope) -> void
{
	if (scope == Scope::Root) {
		m_stats.recomposed_root += 1;
		return;
	}
	if (scope == Scope::Sidebar) {
		m_stats.recomposed_sidebar += 1;
		return;
	}
	if (scope == Scope::Hud) {
		return;
	}
	m_stats.recomposed_dialog += 1;
}

auto System::mark_scope_dirty(Scope const scope) -> void
{
	switch (scope) {
	case Scope::Root:
		m_root_scope_dirty = true;
		break;
	case Scope::Sidebar:
		m_sidebar_scope_dirty = true;
		break;
	case Scope::Dialog:
		m_dialog_scope_dirty = true;
		break;
	case Scope::Hud:
		break;
	}
}

auto System::clear_scope_dirty_flags() -> void
{
	m_root_scope_dirty = false;
	m_sidebar_scope_dirty = false;
	m_dialog_scope_dirty = false;
}

auto System::collect_reconcile_nodes(std::unique_ptr<Node> node) -> void
{
	if (!node) {
		return;
	}

	auto children { std::move(node->children) };
	node->children.clear();
	node->parent = nullptr;
	for (auto &child : children) {
		collect_reconcile_nodes(std::move(child));
	}
	m_reconcile_nodes[node->key] = std::move(node);
}

auto System::stash_orphan(std::unique_ptr<Node> node) -> void
{
	if (!node) {
		return;
	}

	node->children.clear();
	node->parent = nullptr;
	if (m_node_pool.size() >= NODE_POOL_MAX) {
		return;
	}
	m_node_pool.push_back(std::move(node));
}

auto System::build_render_cache_node(
    Node const &source, uint16_t const depth, uint16_t const parent_index)
    -> uint16_t
{
	if (m_render_nodes.size() >= static_cast<size_t>(INVALID_NODE_INDEX)) {
		return INVALID_NODE_INDEX;
	}

	auto const index { static_cast<uint16_t>(m_render_nodes.size()) };
	m_render_nodes.push_back(RenderNode {
	    .kind = source.kind,
	    .scope = source.scope,
	    .key = source.key,
	    .local_key = source.local_key,
	    .rect = source.layout.world_rect,
	    .text_size = source.content.text_size,
	    .text_align_x = source.content.text_align_x,
	    .text_align_y = source.content.text_align_y,
	    .corner_radius = source.visual.corner_radius,
	    .outline_thickness = source.visual.outline_thickness,
	    .icon_size = source.content.icon_size,
	    .opacity = source.visual.opacity,
	    .scroll_x = source.scroll.scroll_x,
	    .scroll_y = source.scroll.scroll_y,
	    .padding_top = source.layout.padding_top,
	    .padding_right = source.layout.padding_right,
	    .padding_bottom = source.layout.padding_bottom,
	    .padding_left = source.layout.padding_left,
	    .interactive = source.interaction.interactive,
	    .selectable = source.interaction.selectable,
	    .use_pressable_state = source.interaction.use_pressable_state,
	    .draw_fill = source.visual.draw_fill,
	    .draw_outline = source.visual.draw_outline,
	    .draw_scrim = source.visual.draw_scrim,
	    .scrim_opacity = source.visual.scrim_opacity,
	    .layer_focus_mode = source.visual.layer_focus_mode,
	    .top = source.visual.top,
	    .right = source.visual.right,
	    .bottom = source.visual.bottom,
	    .left = source.visual.left,
	    .fill_color = source.visual.fill_color,
	    .focus_fill_color = source.visual.focus_fill_color,
	    .selected_fill_color = source.visual.selected_fill_color,
	    .outline_color = source.visual.outline_color,
	    .text_color = source.visual.text_color,
	    .selected_text_color = source.visual.selected_text_color,
	    .icon_tint = source.visual.icon_tint,
	    .selected_icon_tint = source.visual.selected_icon_tint,
	    .scrim_color = source.visual.scrim_color,
	    .depth = depth,
	    .label = &source.content.label,
	    .icon_name = &source.content.icon_name,
	    .parent_index = parent_index,
	    .first_child = INVALID_NODE_INDEX,
	    .next_sibling = INVALID_NODE_INDEX,
	});

	auto child_head { INVALID_NODE_INDEX };
	auto prev_child { INVALID_NODE_INDEX };
	for (auto const &child : source.children) {
		auto const child_index { build_render_cache_node(
			*child, static_cast<uint16_t>(depth + 1), index) };
		if (child_index == INVALID_NODE_INDEX) {
			continue;
		}
		if (child_head == INVALID_NODE_INDEX) {
			child_head = child_index;
		} else {
			m_render_nodes[prev_child].next_sibling = child_index;
		}
		prev_child = child_index;
	}
	m_render_nodes[index].first_child = child_head;
	return index;
}

auto System::begin_frame(WindowHandle const handle,
    Input const input,
    float const dt,
    Engine::Rect<> const rect) -> void
{
	auto const window_rect_changed { !rect_equal(m_window_rect, rect) };
	m_current_window = handle;
	m_window_rect = rect;
	m_input = input;
	m_dt = dt;
	m_confirm_hold_started = false;
	m_stats = Stats {};
	if (window_rect_changed) {
		m_layout_dirty = true;
		m_render_cache_dirty = true;
		m_visual_dirty = true;
	}

	m_confirm_released = m_prev_confirm_down && !m_input.confirm_down;
	if (!m_prev_confirm_down && m_input.confirm_down) {
		m_confirm_hold_elapsed = 0.0f;
		m_confirm_hold_fired = false;
		m_confirm_hold_consumed = false;
	}

	if (m_input.confirm_down) {
		m_confirm_hold_elapsed += dt;
		if (!m_confirm_hold_fired
		    && m_confirm_hold_elapsed >= HOLD_THRESHOLD_SECONDS) {
			m_confirm_hold_fired = true;
			m_confirm_hold_started = true;
			m_confirm_hold_consumed = true;
		}
	} else {
		m_confirm_hold_elapsed = 0.0f;
		m_confirm_hold_fired = false;
	}

	handle_input();
	tick_animation(dt);
	m_prev_confirm_down = m_input.confirm_down;
}

auto System::compose(std::function<void(Context &)> const &fn) -> void
{
	if (!m_structure_dirty) {
		return;
	}
	m_is_composing = true;
	m_recompose_requested_during_compose = false;
	begin_compose_tracking();
	m_state_touched.clear();
	m_stats.recomposed_scopes += 1;
	if (m_root_scope_dirty) {
		mark_scope_recomposed(Scope::Root);
	}
	if (m_sidebar_scope_dirty) {
		mark_scope_recomposed(Scope::Sidebar);
	}
	if (m_dialog_scope_dirty) {
		mark_scope_recomposed(Scope::Dialog);
	}

	m_reconcile_nodes.clear();
	for (auto &child : m_root->children) {
		collect_reconcile_nodes(std::move(child));
	}
	m_root->children.clear();
	Context context(*this, m_root.get());
	fn(context);
	prune_state_store();
	tick_node_animations(0.0f, false);
	rebuild_node_index();

	for (auto it { m_reconcile_nodes.begin() };
	    it != m_reconcile_nodes.end();) {
		mark_compose_dirty();
		stash_orphan(std::move(it->second));
		it = m_reconcile_nodes.erase(it);
	}

	m_is_composing = false;
	if (m_recompose_requested_during_compose) {
		m_structure_dirty = true;
	} else {
		m_structure_dirty = false;
		clear_scope_dirty_flags();
	}
	end_compose_tracking();
}

auto System::prune_state_store() -> void
{
	for (auto it { m_state_store.begin() }; it != m_state_store.end();) {
		if (!m_state_touched.contains(it->first)) {
			it = m_state_store.erase(it);
			continue;
		}
		++it;
	}
}

auto System::reconcile_node(Node *const parent,
    Kind const kind,
    Scope const scope,
    Id const key,
    FlexOptions const &options) -> Node *
{
	if (parent == nullptr) {
		return m_root.get();
	}
	auto const full_key { compose_id(parent->key, key) };

	std::unique_ptr<Node> node {};
	auto const existing_it { m_reconcile_nodes.find(full_key) };
	auto const reused { existing_it != m_reconcile_nodes.end() };
	if (reused) {
		node = std::move(existing_it->second);
		m_reconcile_nodes.erase(existing_it);
	} else if (!m_node_pool.empty()) {
		node = std::move(m_node_pool.back());
		m_node_pool.pop_back();
		mark_compose_dirty();
	} else {
		node = std::make_unique<Node>();
		mark_compose_dirty();
	}
	auto const previous_kind { node->kind };
	auto const kind_changed { !reused || previous_kind != kind };
	auto assign_layout { [&](auto &field, auto value) {
		if (field == value) {
			return;
		}
		field = std::move(value);
		mark_layout_dirty();
	} };
	auto assign_visual { [&](auto &field, auto value) {
		if (field == value) {
			return;
		}
		field = std::move(value);
		mark_visual_dirty();
	} };

	assign_layout(node->kind, kind);
	assign_visual(node->scope, scope);
	node->parent = parent;
	auto const padding { resolve_padding(options.padding()) };
	assign_layout(node->layout.padding_top, padding.top);
	assign_layout(node->layout.padding_right, padding.right);
	assign_layout(node->layout.padding_bottom, padding.bottom);
	assign_layout(node->layout.padding_left, padding.left);
	assign_layout(node->layout.gap, options.gap());
	assign_layout(node->layout.row_gap, options.row_gap());
	assign_layout(node->layout.column_gap, options.column_gap());
	if (node->layout.animated_width.has_value()) {
		node->layout.animated_width.reset();
		mark_layout_dirty();
	}
	if (node->layout.animated_height.has_value()) {
		node->layout.animated_height.reset();
		mark_layout_dirty();
	}
	if (options.width_value().has_value()) {
		auto const *width_ref {
			std::get_if<Animation::Ref>(&*options.width_value()),
		};
		if (width_ref != nullptr) {
			if (!node->layout.animated_width.has_value()) {
				node->layout.animated_width = *width_ref;
				mark_layout_dirty();
			} else {
				node->layout.animated_width = *width_ref;
			}
			assign_layout(node->layout.fixed_width,
			    resolve_animated_float(*width_ref, full_key));
		} else {
			assign_layout(node->layout.fixed_width, options.width());
		}
	} else {
		assign_layout(node->layout.fixed_width, options.width());
	}
	if (options.height_value().has_value()) {
		auto const *height_ref {
			std::get_if<Animation::Ref>(&*options.height_value()),
		};
		if (height_ref != nullptr) {
			if (!node->layout.animated_height.has_value()) {
				node->layout.animated_height = *height_ref;
				mark_layout_dirty();
			} else {
				node->layout.animated_height = *height_ref;
			}
			assign_layout(node->layout.fixed_height,
			    resolve_animated_float(*height_ref, full_key));
		} else {
			assign_layout(node->layout.fixed_height, options.height());
		}
	} else {
		assign_layout(node->layout.fixed_height, options.height());
	}
	assign_layout(node->layout.min_width, options.min_width());
	assign_layout(node->layout.min_height, options.min_height());
	assign_layout(node->layout.max_width, options.max_width());
	assign_layout(node->layout.max_height, options.max_height());
	assign_layout(node->layout.flex_grow, options.flex_grow());
	assign_layout(node->layout.flex_shrink, options.flex_shrink());
	assign_layout(node->layout.flex_basis, options.flex_basis());
	assign_layout(node->layout.align_self, options.align_self());
	assign_layout(node->layout.flex_direction, options.direction());
	assign_layout(node->layout.flex_wrap, options.wrap());
	assign_layout(node->layout.justify_content, options.justify_content());
	assign_layout(node->layout.align_items, options.align_items());
	assign_layout(node->layout.align_content, options.align_content());
	if (kind_changed) {
		node->scroll.scroll_axis = ScrollAxis::Vertical;
		node->scroll.scroll_reveal_mode = ScrollRevealMode::Minimal;
		node->scroll.scroll_step = 24.0f;
		node->content.label.clear();
		node->content.icon_name.clear();
		node->content.text_size = 14.0f;
		node->content.text_align_x = TextAlignX::Left;
		node->content.text_align_y = TextAlignY::Top;
		node->interaction.interactive = false;
		node->interaction.selectable = false;
		node->interaction.use_pressable_state = false;
		node->visual.draw_fill = false;
		node->visual.draw_outline = false;
		node->visual.draw_scrim = false;
		node->visual.scrim_opacity = 1.0f;
		node->visual.animated_scrim_opacity.reset();
		node->visual.corner_radius = 0.0f;
		node->visual.outline_thickness = 1.0f;
		node->content.icon_size = 24.0f;
		node->visual.opacity = 1.0f;
		node->visual.animated_opacity.reset();
		node->visual.layer_focus_mode = LayerFocusMode::Overlay;
		node->visual.top.reset();
		node->visual.right.reset();
		node->visual.bottom.reset();
		node->visual.left.reset();
		node->visual.fill_color.reset();
		node->visual.focus_fill_color.reset();
		node->visual.selected_fill_color.reset();
		node->visual.outline_color.reset();
		node->visual.text_color.reset();
		node->visual.selected_text_color.reset();
		node->visual.icon_tint.reset();
		node->visual.selected_icon_tint.reset();
		node->visual.scrim_color.reset();
		node->interaction.on_activate = {};
		mark_layout_dirty();
		mark_visual_dirty();
	}
	node->key = full_key;
	node->local_key = key;
	node->children.clear();
	if (!reused) {
		node->recompose_count = 0;
		node->skip_count = 0;
		node->scroll.content_width = 0.0f;
		node->scroll.content_height = 0.0f;
		node->layout.local_rect = {};
		node->layout.world_rect = {};
		node->layout.translation = smath::Vec2 { 0.0f, 0.0f };
		node->scroll.scroll_x = 0.0f;
		node->scroll.scroll_y = 0.0f;
		node->scroll.scroll_target_x = 0.0f;
		node->scroll.scroll_target_y = 0.0f;
	}
	node->recompose_count += 1;

	parent->children.push_back(std::move(node));
	return parent->children.back().get();
}

auto System::find_node_by_key(Id const key) -> Node *
{
	if (!key.valid()) {
		return nullptr;
	}

	auto it { m_nodes_by_key.find(key) };
	return it != m_nodes_by_key.end() ? it->second : nullptr;
}

auto System::rebuild_node_index() -> void
{
	m_nodes_by_key.clear();

	std::vector<Node *> stack;
	stack.push_back(m_root.get());

	while (!stack.empty()) {
		auto *node { stack.back() };
		stack.pop_back();

		if (!node) {
			continue;
		}

		m_nodes_by_key[node->key] = node;

		for (auto &child : node->children) {
			stack.push_back(child.get());
		}
	}
}

auto System::scope_present(Scope const scope) const -> bool
{
	if (m_root == nullptr) {
		return false;
	}

	std::vector<Node const *> stack;
	stack.push_back(m_root.get());
	while (!stack.empty()) {
		auto const *node { stack.back() };
		stack.pop_back();
		if (node == nullptr) {
			continue;
		}
		if (node->scope == scope) {
			return true;
		}
		for (auto const &child : node->children) {
			stack.push_back(child.get());
		}
	}
	return false;
}

auto System::active_scope() const -> Scope
{
	if (scope_present(Scope::Dialog)) {
		return Scope::Dialog;
	}
	if (scope_present(Scope::Sidebar)) {
		return Scope::Sidebar;
	}
	return Scope::Root;
}

auto System::gather_focusables(
    Node &node, Scope const scope, std::vector<Node *> &out) -> void
{
	if (node.scope == scope && node.interaction.interactive) {
		out.push_back(&node);
	}
	for (auto &child : node.children) {
		gather_focusables(*child, scope, out);
	}
}

auto System::active_scope_focus_key() -> Id &
{
	auto const scope { active_scope() };
	if (scope == Scope::Sidebar) {
		return m_sidebar_focus_key;
	}
	if (scope == Scope::Dialog) {
		return m_dialog_focus_key;
	}
	return m_root_focus_key;
}

auto System::active_scope_focus_key() const -> Id const &
{
	auto const scope { active_scope() };
	if (scope == Scope::Sidebar) {
		return m_sidebar_focus_key;
	}
	if (scope == Scope::Dialog) {
		return m_dialog_focus_key;
	}
	return m_root_focus_key;
}

auto System::sync_focus() -> void
{
	std::vector<Node *> focusables {};
	gather_focusables(*m_root, active_scope(), focusables);
	if (focusables.empty()) {
		return;
	}

	auto &scope_key { active_scope_focus_key() };
	auto had_to_replace_focus { false };

	if (!scope_key.valid()) {
		scope_key = focusables.front()->key;
		had_to_replace_focus = true;
	}

	auto found = std::any_of(focusables.begin(),
	    focusables.end(),
	    [&](Node const *const node) { return node->key == scope_key; });
	if (!found) {
		scope_key = focusables.front()->key;
		had_to_replace_focus = true;
	}

	auto *focused { find_node_by_key(scope_key) };
	if (focused != nullptr) {
		auto const center {
			focused->layout.world_rect.position
			    + (focused->layout.world_rect.size * 0.5f),
		};

		if (had_to_replace_focus || !m_has_vertical_nav_anchor_x) {
			m_vertical_nav_anchor_x = center.x();
			m_has_vertical_nav_anchor_x = true;
		}
		if (had_to_replace_focus || !m_has_horizontal_nav_anchor_y) {
			m_horizontal_nav_anchor_y = center.y();
			m_has_horizontal_nav_anchor_y = true;
		}
	}
}

auto System::focused_node() -> Node *
{
	return find_node_by_key(active_scope_focus_key());
}

auto System::ensure_focus_visible(Node &node) -> void
{
	auto *parent { node.parent };
	while (parent != nullptr) {
		if (parent->kind == Kind::Scrollable) {
			auto const is_focusable_descendant = [&](Node const &candidate) {
				return candidate.scope == active_scope()
				    && candidate.interaction.interactive;
			};
			auto const first_focusable_descendant
			    = [&](Node const &root) -> Node const * {
				std::vector<Node const *> stack;
				for (auto it = root.children.rbegin();
				    it != root.children.rend();
				    ++it) {
					stack.push_back(it->get());
				}
				while (!stack.empty()) {
					auto const *current { stack.back() };
					stack.pop_back();
					if (current == nullptr) {
						continue;
					}
					if (is_focusable_descendant(*current)) {
						return current;
					}
					for (auto it = current->children.rbegin();
					    it != current->children.rend();
					    ++it) {
						stack.push_back(it->get());
					}
				}
				return nullptr;
			};
			auto const last_focusable_descendant
			    = [&](Node const &root) -> Node const * {
				std::vector<Node const *> stack;
				for (auto const &child : root.children) {
					stack.push_back(child.get());
				}
				while (!stack.empty()) {
					auto const *current { stack.back() };
					stack.pop_back();
					if (current == nullptr) {
						continue;
					}
					if (is_focusable_descendant(*current)) {
						return current;
					}
					for (auto const &child : current->children) {
						stack.push_back(child.get());
					}
				}
				return nullptr;
			};
			auto const include_padding {
				parent->scroll.scroll_reveal_mode
				    == ScrollRevealMode::IncludePadding,
			};
			auto const *first_focusable {
				include_padding ? first_focusable_descendant(*parent) : nullptr,
			};
			auto const *last_focusable {
				include_padding ? last_focusable_descendant(*parent) : nullptr,
			};
			auto const top {
				parent->layout.world_rect.position.y()
				    + parent->layout.padding_top,
			};
			auto const bottom {
				parent->layout.world_rect.position.y()
				    + parent->layout.world_rect.size.y()
				    - parent->layout.padding_bottom,
			};
			auto const node_top { node.layout.world_rect.position.y() };
			auto const node_bottom { node.layout.world_rect.position.y()
				+ node.layout.world_rect.size.y() };
			auto const left {
				parent->layout.world_rect.position.x()
				    + parent->layout.padding_left,
			};
			auto const right {
				parent->layout.world_rect.position.x()
				    + parent->layout.world_rect.size.x()
				    - parent->layout.padding_right,
			};
			auto const node_left { node.layout.world_rect.position.x() };
			auto const node_right { node.layout.world_rect.position.x()
				+ node.layout.world_rect.size.x() };

			if ((parent->scroll.scroll_axis == ScrollAxis::Vertical
			        || parent->scroll.scroll_axis == ScrollAxis::Both)
			    && node_top < top) {
				parent->scroll.scroll_target_y -= (top - node_top);
			}
			if ((parent->scroll.scroll_axis == ScrollAxis::Vertical
			        || parent->scroll.scroll_axis == ScrollAxis::Both)
			    && include_padding && first_focusable == &node) {
				parent->scroll.scroll_target_y = 0.0f;
			}
			if ((parent->scroll.scroll_axis == ScrollAxis::Vertical
			        || parent->scroll.scroll_axis == ScrollAxis::Both)
			    && node_bottom > bottom) {
				parent->scroll.scroll_target_y += (node_bottom - bottom);
			}
			if ((parent->scroll.scroll_axis == ScrollAxis::Vertical
			        || parent->scroll.scroll_axis == ScrollAxis::Both)
			    && include_padding && last_focusable == &node) {
				parent->scroll.scroll_target_y
				    = std::numeric_limits<float>::infinity();
			}
			if ((parent->scroll.scroll_axis == ScrollAxis::Horizontal
			        || parent->scroll.scroll_axis == ScrollAxis::Both)
			    && node_left < left) {
				parent->scroll.scroll_target_x -= (left - node_left);
			}
			if ((parent->scroll.scroll_axis == ScrollAxis::Horizontal
			        || parent->scroll.scroll_axis == ScrollAxis::Both)
			    && include_padding && first_focusable == &node) {
				parent->scroll.scroll_target_x = 0.0f;
			}
			if ((parent->scroll.scroll_axis == ScrollAxis::Horizontal
			        || parent->scroll.scroll_axis == ScrollAxis::Both)
			    && node_right > right) {
				parent->scroll.scroll_target_x += (node_right - right);
			}
			if ((parent->scroll.scroll_axis == ScrollAxis::Horizontal
			        || parent->scroll.scroll_axis == ScrollAxis::Both)
			    && include_padding && last_focusable == &node) {
				parent->scroll.scroll_target_x
				    = std::numeric_limits<float>::infinity();
			}

			auto const max_scroll {
				std::max(0.0f,
				    parent->scroll.content_height
				        - (parent->layout.world_rect.size.y()
				            - parent->layout.padding_top
				            - parent->layout.padding_bottom)),
			};
			auto const max_scroll_x {
				std::max(0.0f,
				    parent->scroll.content_width
				        - (parent->layout.world_rect.size.x()
				            - parent->layout.padding_left
				            - parent->layout.padding_right)),
			};
			parent->scroll.scroll_target_y
			    = std::clamp(parent->scroll.scroll_target_y, 0.0f, max_scroll);
			parent->scroll.scroll_target_x = std::clamp(
			    parent->scroll.scroll_target_x, 0.0f, max_scroll_x);
			m_world_dirty = true;
			m_visual_dirty = true;
		}
		parent = parent->parent;
	}
}

auto System::handle_input() -> void
{
	if (m_input.debug_bounds_toggle_pressed) {
		m_debug_bounds = !m_debug_bounds;
		m_visual_dirty = true;
	}

	if (m_input.back_pressed) {
		if (m_selection_mode) {
			m_selection_mode = false;
			m_selected.clear();
			m_visual_dirty = true;
			m_pending_selectable_activation = Id {};
			return;
		}
	}

	std::vector<Node *> focusables {};
	gather_focusables(*m_root, active_scope(), focusables);
	if (focusables.empty()) {
		return;
	}

	auto *focused { focused_node() };
	if (focused == nullptr) {
		focused = focusables.front();
	}

	if (focused != nullptr) {
		auto *scroll_parent { focused->parent };
		while (scroll_parent != nullptr) {
			if (scroll_parent->kind != Kind::Scrollable) {
				scroll_parent = scroll_parent->parent;
				continue;
			}

			auto changed { false };
			auto const viewport_w { std::max(1.0f,
				scroll_parent->layout.world_rect.size.x()
				    - scroll_parent->layout.padding_left
				    - scroll_parent->layout.padding_right) };
			auto const viewport_h { std::max(1.0f,
				scroll_parent->layout.world_rect.size.y()
				    - scroll_parent->layout.padding_top
				    - scroll_parent->layout.padding_bottom) };
			auto const max_scroll_x {
				std::max(
				    0.0f, scroll_parent->scroll.content_width - viewport_w),
			};
			auto const max_scroll_y {
				std::max(
				    0.0f, scroll_parent->scroll.content_height - viewport_h),
			};

			auto const deadzone { 0.18f };
			auto const speed {
				std::max(120.0f, scroll_parent->scroll.scroll_step * 7.0f),
			};
			if ((scroll_parent->scroll.scroll_axis == ScrollAxis::Horizontal
			        || scroll_parent->scroll.scroll_axis == ScrollAxis::Both)
			    && std::abs(m_input.stick_x) > deadzone) {
				auto const next {
					std::clamp(scroll_parent->scroll.scroll_target_x
					        + (m_input.stick_x * speed * m_dt),
					    0.0f,
					    max_scroll_x),
				};
				changed = changed
				    || std::abs(next - scroll_parent->scroll.scroll_target_x)
				        > 0.01f;
				scroll_parent->scroll.scroll_target_x = next;
			}
			if ((scroll_parent->scroll.scroll_axis == ScrollAxis::Vertical
			        || scroll_parent->scroll.scroll_axis == ScrollAxis::Both)
			    && std::abs(m_input.stick_y) > deadzone) {
				auto const next {
					std::clamp(scroll_parent->scroll.scroll_target_y
					        + (m_input.stick_y * speed * m_dt),
					    0.0f,
					    max_scroll_y),
				};
				changed = changed
				    || std::abs(next - scroll_parent->scroll.scroll_target_y)
				        > 0.01f;
				scroll_parent->scroll.scroll_target_y = next;
			}

			if (changed) {
				m_world_dirty = true;
				m_visual_dirty = true;
			}
			scroll_parent = scroll_parent->parent;
		}
	}

	if (std::find(focusables.begin(), focusables.end(), focused)
	    == focusables.end()) {
		focused = focusables.front();
	}

	auto center_of { [](Node const *const node) {
		return smath::Vec2 {
			node->layout.world_rect.position.x()
			    + node->layout.world_rect.size.x() * 0.5f,
			node->layout.world_rect.position.y()
			    + node->layout.world_rect.size.y() * 0.5f,
		};
	} };

	auto navigate_focus { [&](int const dir_x, int const dir_y) -> Node * {
		if (focused == nullptr) {
			return nullptr;
		}

		auto const base_center { center_of(focused) };
		auto const base_left { focused->layout.world_rect.position.x() };
		auto const base_right { focused->layout.world_rect.position.x()
			+ focused->layout.world_rect.size.x() };
		auto const base_top { focused->layout.world_rect.position.y() };
		auto const base_bottom { focused->layout.world_rect.position.y()
			+ focused->layout.world_rect.size.y() };

		auto const cross_anchor {
			dir_x != 0
			    ? (m_has_horizontal_nav_anchor_y ? m_horizontal_nav_anchor_y
			                                     : base_center.y())
			    : (m_has_vertical_nav_anchor_x ? m_vertical_nav_anchor_x
			                                   : base_center.x()),
		};

		Node *best_in_beam {};
		float best_in_beam_score { std::numeric_limits<float>::max() };
		Node *best_off_beam {};
		float best_off_beam_score { std::numeric_limits<float>::max() };

		for (auto *candidate : focusables) {
			if (candidate == nullptr || candidate == focused) {
				continue;
			}

			auto const candidate_center { center_of(candidate) };
			auto const dx { candidate_center.x() - base_center.x() };
			auto const dy { candidate_center.y() - base_center.y() };

			float primary {};
			if (dir_x < 0) {
				if (dx >= -0.001f) {
					continue;
				}
				primary = -dx;
			} else if (dir_x > 0) {
				if (dx <= 0.001f) {
					continue;
				}
				primary = dx;
			} else if (dir_y < 0) {
				if (dy >= -0.001f) {
					continue;
				}
				primary = -dy;
			} else {
				if (dy <= 0.001f) {
					continue;
				}
				primary = dy;
			}

			auto const candidate_left {
				candidate->layout.world_rect.position.x()
			};
			auto const candidate_right {
				candidate->layout.world_rect.position.x()
				    + candidate->layout.world_rect.size.x(),
			};
			auto const candidate_top {
				candidate->layout.world_rect.position.y()
			};
			auto const candidate_bottom {
				candidate->layout.world_rect.position.y()
				    + candidate->layout.world_rect.size.y(),
			};

			auto const in_beam {
				dir_x != 0 ? spans_overlap(base_top,
				                 base_bottom,
				                 candidate_top,
				                 candidate_bottom)
				           : spans_overlap(base_left,
				                 base_right,
				                 candidate_left,
				                 candidate_right),
			};

			auto const cross {
				dir_x != 0 ? std::abs(candidate_center.y() - cross_anchor)
				           : std::abs(candidate_center.x() - cross_anchor),
			};

			auto const score { primary + cross * 0.25f };

			if (in_beam) {
				if (best_in_beam == nullptr || score < best_in_beam_score) {
					best_in_beam = candidate;
					best_in_beam_score = score;
				}
			} else {
				auto const off_beam_score { primary + cross * 4.0f };
				if (best_off_beam == nullptr
				    || off_beam_score < best_off_beam_score) {
					best_off_beam = candidate;
					best_off_beam_score = off_beam_score;
				}
			}
		}

		if (best_in_beam != nullptr) {
			return best_in_beam;
		}
		if (best_off_beam != nullptr) {
			return best_off_beam;
		}

		Node *wrapped_in_beam {};
		float wrapped_in_beam_axis {};
		float wrapped_in_beam_cross { std::numeric_limits<float>::max() };

		Node *wrapped_off_beam {};
		float wrapped_off_beam_axis {};
		float wrapped_off_beam_cross { std::numeric_limits<float>::max() };

		auto const want_max_axis { dir_x < 0 || dir_y < 0 };

		for (auto *candidate : focusables) {
			if (candidate == nullptr || candidate == focused) {
				continue;
			}

			auto const candidate_center { center_of(candidate) };
			auto const candidate_left {
				candidate->layout.world_rect.position.x()
			};
			auto const candidate_right {
				candidate->layout.world_rect.position.x()
				    + candidate->layout.world_rect.size.x(),
			};
			auto const candidate_top {
				candidate->layout.world_rect.position.y()
			};
			auto const candidate_bottom {
				candidate->layout.world_rect.position.y()
				    + candidate->layout.world_rect.size.y(),
			};

			auto const in_beam {
				dir_x != 0 ? spans_overlap(base_top,
				                 base_bottom,
				                 candidate_top,
				                 candidate_bottom)
				           : spans_overlap(base_left,
				                 base_right,
				                 candidate_left,
				                 candidate_right),
			};

			auto const axis {
				dir_x != 0 ? candidate_center.x() : candidate_center.y(),
			};
			auto const cross {
				dir_x != 0 ? std::abs(candidate_center.y() - cross_anchor)
				           : std::abs(candidate_center.x() - cross_anchor),
			};

			if (in_beam) {
				auto const better_axis {
					wrapped_in_beam == nullptr
					    || (want_max_axis
					            ? axis > wrapped_in_beam_axis + 0.001f
					            : axis < wrapped_in_beam_axis - 0.001f),
				};
				auto const same_axis {
					wrapped_in_beam != nullptr
					    && std::abs(axis - wrapped_in_beam_axis) < 0.001f,
				};

				if (better_axis
				    || (same_axis && cross < wrapped_in_beam_cross)) {
					wrapped_in_beam = candidate;
					wrapped_in_beam_axis = axis;
					wrapped_in_beam_cross = cross;
				}
			} else {
				auto const better_axis {
					wrapped_off_beam == nullptr
					    || (want_max_axis
					            ? axis > wrapped_off_beam_axis + 0.001f
					            : axis < wrapped_off_beam_axis - 0.001f),
				};
				auto const same_axis {
					wrapped_off_beam != nullptr
					    && std::abs(axis - wrapped_off_beam_axis) < 0.001f,
				};

				if (better_axis
				    || (same_axis && cross < wrapped_off_beam_cross)) {
					wrapped_off_beam = candidate;
					wrapped_off_beam_axis = axis;
					wrapped_off_beam_cross = cross;
				}
			}
		}

		if (wrapped_in_beam != nullptr) {
			return wrapped_in_beam;
		}
		return wrapped_off_beam;
	} };

	Node *next_focus {};
	if (m_input.up_pressed) {
		next_focus = navigate_focus(0, -1);
	} else if (m_input.down_pressed) {
		next_focus = navigate_focus(0, 1);
	} else if (m_input.left_pressed) {
		next_focus = navigate_focus(-1, 0);
	} else if (m_input.right_pressed) {
		next_focus = navigate_focus(1, 0);
	}
	if (next_focus != nullptr && next_focus != focused) {
		focused = next_focus;

		auto const center {
			focused->layout.world_rect.position
			    + (focused->layout.world_rect.size * 0.5f),
		};

		if (m_input.left_pressed || m_input.right_pressed) {
			m_vertical_nav_anchor_x = center.x();
			m_has_vertical_nav_anchor_x = true;
		}
		if (m_input.up_pressed || m_input.down_pressed) {
			m_horizontal_nav_anchor_y = center.y();
			m_has_horizontal_nav_anchor_y = true;
		}

		ensure_focus_visible(*focused);
		m_visual_dirty = true;
	}

	if (focused != nullptr) {
		active_scope_focus_key() = focused->key;
	}

	if (m_confirm_hold_started && focused != nullptr
	    && focused->interaction.selectable) {
		m_selection_mode = true;
		m_selected.insert(focused->key);
		m_visual_dirty = true;
		m_pending_selectable_activation = Id {};
	}

	if (m_input.confirm_pressed && focused != nullptr) {
		if (m_selection_mode && focused->interaction.selectable) {
			if (m_selected.contains(focused->key)) {
				m_selected.erase(focused->key);
			} else {
				m_selected.insert(focused->key);
			}
			if (m_selected.empty()) {
				m_selection_mode = false;
			}
			m_visual_dirty = true;
			return;
		}

		if (focused->interaction.selectable) {
			m_pending_selectable_activation = focused->key;
			return;
		}

		if (focused->interaction.on_activate) {
			focused->interaction.on_activate();
		}
	}

	if (m_confirm_released && m_pending_selectable_activation.valid()) {
		if (!m_confirm_hold_consumed) {
			auto *node {
				find_node_by_key(m_pending_selectable_activation),
			};
			if (node != nullptr && node->interaction.on_activate) {
				node->interaction.on_activate();
			}
		}
		m_pending_selectable_activation = Id {};
	}
}

auto System::tick_animation(float const dt) -> void
{
	tick_sidebar_animation(dt);
	tick_scroll_animation(dt);
	tick_node_animations(dt, true);
}

auto System::tick_sidebar_animation(float const dt) -> void
{
	auto const before { m_sidebar_progress };
	m_sidebar_tween.tick(dt);
	auto const new_value { m_sidebar_tween.value() };
	if (new_value < 0.001f) {
		m_sidebar_progress = 0.0f;
	} else if (new_value > 0.999f) {
		m_sidebar_progress = 1.0f;
	} else {
		m_sidebar_progress = new_value;
	}
	if (!approx_equal(m_sidebar_progress, before)) {
		m_world_dirty = true;
		m_visual_dirty = true;
		mark_scope_dirty(Scope::Sidebar);
	}
}

auto System::tick_scroll_animation(float const dt) -> void
{
	if (m_root == nullptr) {
		m_scroll_tweens.clear();
		return;
	}

	std::unordered_set<Id, Id::Hash> active_keys {};
	std::function<void(Node &)> animate_scroll { [&](Node &node) {
		if (node.kind == Kind::Scrollable) {
			active_keys.insert(node.key);
			auto &state { m_scroll_tweens[node.key] };

			if (!state.has_target_x
			    || !approx_equal(
			        state.target_x, node.scroll.scroll_target_x, 0.01f)) {
				state.target_x = node.scroll.scroll_target_x;
				state.has_target_x = true;
				state.x.configure(Animation::TweenSpec {
				    .from = node.scroll.scroll_x,
				    .to = node.scroll.scroll_target_x,
				    .duration_seconds = 0.14f,
				    .easing = Animation::Easing::EaseOutCubic,
				    .repeat = Animation::RepeatMode::Once,
				});
			}
			if (!state.has_target_y
			    || !approx_equal(
			        state.target_y, node.scroll.scroll_target_y, 0.01f)) {
				state.target_y = node.scroll.scroll_target_y;
				state.has_target_y = true;
				state.y.configure(Animation::TweenSpec {
				    .from = node.scroll.scroll_y,
				    .to = node.scroll.scroll_target_y,
				    .duration_seconds = 0.14f,
				    .easing = Animation::Easing::EaseOutCubic,
				    .repeat = Animation::RepeatMode::Once,
				});
			}

			auto const before_x { node.scroll.scroll_x };
			auto const before_y { node.scroll.scroll_y };
			state.x.tick(dt);
			state.y.tick(dt);
			node.scroll.scroll_x = state.x.value();
			node.scroll.scroll_y = state.y.value();
			if (std::abs(node.scroll.scroll_x - node.scroll.scroll_target_x)
			    < 0.01f) {
				node.scroll.scroll_x = node.scroll.scroll_target_x;
			}
			if (std::abs(node.scroll.scroll_y - node.scroll.scroll_target_y)
			    < 0.01f) {
				node.scroll.scroll_y = node.scroll.scroll_target_y;
			}
			if (!approx_equal(node.scroll.scroll_x, before_x)
			    || !approx_equal(node.scroll.scroll_y, before_y)) {
				m_world_dirty = true;
				m_visual_dirty = true;
			}
		}

		for (auto &child : node.children) {
			animate_scroll(*child);
		}
	} };
	animate_scroll(*m_root);

	for (auto it { m_scroll_tweens.begin() }; it != m_scroll_tweens.end();) {
		if (!active_keys.contains(it->first)) {
			it = m_scroll_tweens.erase(it);
			continue;
		}
		++it;
	}
}

auto System::resolve_animated_float(
    Animation::Ref const &ref, Id const owner_key) -> float
{
	if (!ref.valid()) {
		return ref.fallback;
	}

	Id resolved_key {};
	if (ref.key.starts_with("root/")) {
		resolved_key = this->id(ref.key);
	} else {
		resolved_key = tween_id(owner_key, ref.key);
	}

	auto &track { m_tween_tracks[resolved_key] };
	auto const spec_changed {
		!track.initialized || !tween_spec_equal(track.spec, ref.spec),
	};
	auto const generation_changed {
		!track.has_generation || track.generation != ref.generation(),
	};
	if (spec_changed || generation_changed) {
		track.spec = ref.spec;
		track.tween.configure(track.spec);
	}
	track.pause_if = ref.pause_if;
	track.generation = ref.generation();
	track.has_generation = true;
	track.initialized = true;
	return track.tween.value();
}

auto System::tick_node_animations(float const dt, bool const advance) -> void
{
	std::unordered_set<Id, Id::Hash> changed_tweens {};
	for (auto &pair : m_tween_tracks) {
		auto const key { pair.first };
		auto &track { pair.second };
		if (!track.initialized) {
			continue;
		}
		auto const paused { track.pause_if && track.pause_if() };
		track.tween.set_paused(paused);
		if (advance) {
			if (track.tween.tick(dt)) {
				changed_tweens.insert(key);
				m_visual_dirty = true;
			}
		}
	}

	if (m_root == nullptr) {
		return;
	}

	std::function<void(Node &)> apply { [&](Node &node) {
		if (node.layout.animated_width.has_value()) {
			auto const next_width {
				resolve_animated_float(*node.layout.animated_width, node.key),
			};
			if (!approx_equal(next_width, node.layout.fixed_width)) {
				node.layout.fixed_width = next_width;
				m_layout_dirty = true;
				m_render_cache_dirty = true;
				m_visual_dirty = true;
			}
		}
		if (node.layout.animated_height.has_value()) {
			auto const next_height {
				resolve_animated_float(*node.layout.animated_height, node.key),
			};
			if (!approx_equal(next_height, node.layout.fixed_height)) {
				node.layout.fixed_height = next_height;
				m_layout_dirty = true;
				m_render_cache_dirty = true;
				m_visual_dirty = true;
			}
		}
		if (node.visual.animated_opacity.has_value()) {
			auto const next_opacity {
				resolve_animated_float(*node.visual.animated_opacity, node.key),
			};
			if (!approx_equal(next_opacity, node.visual.opacity)) {
				node.visual.opacity = next_opacity;
				m_render_cache_dirty = true;
				m_visual_dirty = true;
			}
		}
		if (node.visual.animated_scrim_opacity.has_value()) {
			auto const next_opacity {
				resolve_animated_float(
				    *node.visual.animated_scrim_opacity, node.key),
			};
			if (!approx_equal(next_opacity, node.visual.scrim_opacity)) {
				node.visual.scrim_opacity = next_opacity;
				m_render_cache_dirty = true;
				m_visual_dirty = true;
			}
		}
		if (has_animated_inset(node)
		    && inset_tween_changed(node, changed_tweens, *this)) {
			m_world_dirty = true;
			m_render_cache_dirty = true;
			m_visual_dirty = true;
			if (inset_animation_affects_layout(node)) {
				m_layout_dirty = true;
				m_world_subtree_dirty.clear();
			} else {
				refresh_layer_local_rect_from_insets(
				    node, *this, m_window_rect);
				m_world_subtree_dirty.insert(node.key);
			}
		}

		for (auto &child : node.children) {
			apply(*child);
		}
	} };
	apply(*m_root);
}

auto System::layout_node(Node &node,
    float const x,
    float const y,
    float const width,
    float const height_constraint) -> float
{
	m_stats.relaid_out_nodes += 1;

	auto clamp_rect_size { [&](float &w, float &h) {
		if (node.layout.min_width > 0.0f) {
			w = std::max(w, node.layout.min_width);
		}
		if (node.layout.min_height > 0.0f) {
			h = std::max(h, node.layout.min_height);
		}
		if (node.layout.max_width > 0.0f) {
			w = std::min(w, node.layout.max_width);
		}
		if (node.layout.max_height > 0.0f) {
			h = std::min(h, node.layout.max_height);
		}
	} };

	auto layout_text { [&]() -> float {
		auto const measured { measure_leaf(node) };

		float text_width { measured.width };
		if (node.layout.fixed_width > 0.0f) {
			text_width = node.layout.fixed_width;
		} else if (node.layout.flex_grow > 0.0f
		    || node.layout.flex_shrink > 0.0f) {
			text_width = std::max(measured.width, width);
		} else if (node.parent != nullptr) {
			auto const effective_align {
				resolve_align_items(
				    node.parent->layout.align_items, node.layout.align_self),
			};
			if (effective_align == AlignItems::Stretch) {
				text_width = std::max(measured.width, width);
			}
		}

		float text_height { measured.height };
		clamp_rect_size(text_width, text_height);

		node.layout.local_rect.position = smath::Vec2 { x, y };
		node.layout.local_rect.size = smath::Vec2 { text_width, text_height };
		return text_height;
	} };

	auto layout_icon { [&]() -> float {
		float w { node.content.icon_size };
		float h { node.content.icon_size };
		clamp_rect_size(w, h);

		node.layout.local_rect.position = smath::Vec2 { x, y };
		node.layout.local_rect.size = smath::Vec2 { w, h };
		return h;
	} };

	auto layout_spacer { [&]() -> float {
		float w { std::max(0.0f, width) };
		float h { std::max(0.0f, node.layout.fixed_height) };
		clamp_rect_size(w, h);

		node.layout.local_rect.position = smath::Vec2 { x, y };
		node.layout.local_rect.size = smath::Vec2 { w, h };
		return h;
	} };

	switch (node.kind) {
	case Kind::Text:
		return layout_text();
	case Kind::Icon:
		return layout_icon();
	case Kind::Spacer:
		return layout_spacer();
	case Kind::OverlayHost:
		break;
	case Kind::Scrollable:
	case Kind::Flex:
	case Kind::Surface:
	case Kind::Pressable:
	case Kind::Layer:
	case Kind::Root:
		break;
	}

	auto const is_flex_like { node.kind == Kind::Flex
		|| node.kind == Kind::Pressable || node.kind == Kind::Surface
		|| node.kind == Kind::Layer };

	if (!is_flex_like && node.kind != Kind::Scrollable) {
		float rect_w { width };
		float rect_h { node.layout.fixed_height };
		clamp_rect_size(rect_w, rect_h);

		node.layout.local_rect.position = smath::Vec2 { x, y };
		node.layout.local_rect.size = smath::Vec2 { rect_w, rect_h };
		return rect_h;
	}

	float rect_width { width };
	float rect_height { node.layout.fixed_height > 0.0f
		    ? node.layout.fixed_height
		    : height_constraint };
	float rect_x { x };
	float rect_y { y };

	if (node.kind == Kind::Layer) {
		auto const top { resolve_animated_scalar(
			node.visual.top, *this, node.key) };
		auto const right { resolve_animated_scalar(
			node.visual.right, *this, node.key) };
		auto const bottom { resolve_animated_scalar(
			node.visual.bottom, *this, node.key) };
		auto const left { resolve_animated_scalar(
			node.visual.left, *this, node.key) };
		auto const container_width { m_window_rect.size.x() };
		auto const container_height { m_window_rect.size.y() };
		rect_width = node.layout.fixed_width > 0.0f ? node.layout.fixed_width
		                                            : rect_width;
		rect_height = node.layout.fixed_height > 0.0f ? node.layout.fixed_height
		                                              : rect_height;
		if (left.has_value()) {
			rect_x = *left;
		}
		if (top.has_value()) {
			rect_y = *top;
		}
		if (left.has_value() && right.has_value()) {
			rect_width = std::max(0.0f, container_width - *left - *right);
		} else if (right.has_value()) {
			if (rect_width <= 0.0f) {
				rect_width = std::max(0.0f, container_width - *right);
			} else {
				rect_x = std::max(0.0f, container_width - *right - rect_width);
			}
		}
		if (top.has_value() && bottom.has_value()) {
			rect_height = std::max(0.0f, container_height - *top - *bottom);
		} else if (bottom.has_value()) {
			if (rect_height <= 0.0f) {
				rect_height = std::max(0.0f, container_height - *bottom);
			} else {
				rect_y
				    = std::max(0.0f, container_height - *bottom - rect_height);
			}
		}
		if (!left.has_value() && !right.has_value() && rect_width <= 0.0f) {
			rect_width = container_width;
		}
		if (!top.has_value() && !bottom.has_value() && rect_height <= 0.0f) {
			rect_height = container_height;
		}
	} else if (node.kind == Kind::OverlayHost) {
		rect_width
		    = node.layout.fixed_width > 0.0f ? node.layout.fixed_width : width;
		rect_height = node.layout.fixed_height > 0.0f
		    ? node.layout.fixed_height
		    : std::max(height_constraint, m_window_rect.size.y());
	} else {
		if (node.layout.fixed_width > 0.0f) {
			rect_width = node.layout.fixed_width;
		}
	}

	node.layout.local_rect.position = smath::Vec2 { rect_x, rect_y };

	float inner_width {
		std::max(0.0f,
		    rect_width
		        - (node.layout.padding_left + node.layout.padding_right)),
	};
	bool const bounded_height { rect_height > 0.0f };
	float inner_height {
		bounded_height
		    ? std::max(0.0f,
		          rect_height
		              - (node.layout.padding_top + node.layout.padding_bottom))
		    : 0.0f,
	};
	float const inner_x { node.layout.padding_left };
	float const inner_y { node.layout.padding_top };

	if (node.kind == Kind::Scrollable) {
		float layout_inner_width { inner_width };
		if (node.layout.max_width > 0.0f) {
			auto const constrained_width {
				std::max(0.0f,
				    node.layout.max_width
				        - (node.layout.padding_left
				            + node.layout.padding_right)),
			};
			if (constrained_width > 0.0f) {
				layout_inner_width
				    = std::min(layout_inner_width, constrained_width);
			}
		}

		bool const stacks_horizontally {
			node.scroll.scroll_axis == ScrollAxis::Horizontal,
		};
		float const stack_gap {
			stacks_horizontally ? node.layout.column_gap : node.layout.row_gap,
		};

		float stack_cursor_x { inner_x };
		float stack_cursor_y { inner_y };
		float max_right { inner_x };
		float max_bottom { inner_y };

		for (size_t i {}; i < node.children.size(); ++i) {
			auto &child { *node.children[i] };

			float const child_height_constraint {
				(node.scroll.scroll_axis == ScrollAxis::Vertical
				    || node.scroll.scroll_axis == ScrollAxis::Both)
				    ? 0.0f
				    : (inner_height > 0.0f ? inner_height : 0.0f),
			};

			layout_node(child,
			    stack_cursor_x,
			    stack_cursor_y,
			    layout_inner_width,
			    child_height_constraint);

			auto const child_right {
				child.layout.local_rect.position.x()
				    + child.layout.local_rect.size.x(),
			};
			auto const child_bottom {
				child.layout.local_rect.position.y()
				    + child.layout.local_rect.size.y(),
			};

			max_right = std::max(max_right, child_right);
			max_bottom = std::max(max_bottom, child_bottom);

			if (i + 1 < node.children.size()) {
				if (stacks_horizontally) {
					stack_cursor_x
					    += child.layout.local_rect.size.x() + stack_gap;
				} else {
					stack_cursor_y
					    += child.layout.local_rect.size.y() + stack_gap;
				}
			}
		}

		node.scroll.content_width = std::max(0.0f, max_right - inner_x);
		node.scroll.content_height = std::max(0.0f, max_bottom - inner_y);

		if (node.layout.fixed_width <= 0.0f && node.layout.max_width > 0.0f) {
			rect_width = node.scroll.content_width + node.layout.padding_left
			    + node.layout.padding_right;
			rect_width = std::min(rect_width, node.layout.max_width);
			if (node.layout.min_width > 0.0f) {
				rect_width = std::max(rect_width, node.layout.min_width);
			}
			inner_width = std::max(0.0f,
			    rect_width
			        - (node.layout.padding_left + node.layout.padding_right));
		}

		if (rect_height <= 0.0f) {
			rect_height = node.scroll.content_height + node.layout.padding_top
			    + node.layout.padding_bottom;
			if (node.layout.max_height > 0.0f) {
				rect_height = std::min(rect_height, node.layout.max_height);
			}
			if (node.layout.min_height > 0.0f) {
				rect_height = std::max(rect_height, node.layout.min_height);
			}
			inner_height = std::max(0.0f,
			    rect_height
			        - (node.layout.padding_top + node.layout.padding_bottom));
		}

		clamp_rect_size(rect_width, rect_height);

		auto const max_scroll_y {
			std::max(0.0f, node.scroll.content_height - inner_height),
		};
		auto const max_scroll_x {
			std::max(0.0f, node.scroll.content_width - inner_width),
		};

		node.scroll.scroll_target_y
		    = std::clamp(node.scroll.scroll_target_y, 0.0f, max_scroll_y);
		node.scroll.scroll_target_x
		    = std::clamp(node.scroll.scroll_target_x, 0.0f, max_scroll_x);
		node.scroll.scroll_y
		    = std::clamp(node.scroll.scroll_y, 0.0f, max_scroll_y);
		node.scroll.scroll_x
		    = std::clamp(node.scroll.scroll_x, 0.0f, max_scroll_x);

		node.layout.local_rect.size = smath::Vec2 { rect_width, rect_height };
		return rect_height;
	}

	struct ItemLayout
	{
		Node *node {};
		float base_main {};
		float base_cross {};
		float used_main {};
		AlignItems resolved_align { AlignItems::Stretch };
	};

	struct LineLayout
	{
		std::vector<ItemLayout> items {};
		float cross_size {};
		float main_used {};
	};

	auto const is_row { is_row_direction(node.layout.flex_direction) };
	auto const reverse { is_reverse_direction(node.layout.flex_direction) };
	float const gap_main { is_row ? node.layout.column_gap
		                          : node.layout.row_gap };
	float const gap_cross { is_row ? node.layout.row_gap
		                           : node.layout.column_gap };
	float const available_main { is_row ? inner_width : inner_height };
	float const available_cross { is_row ? inner_height : inner_width };

	auto resolve_child_basis { [&](Node &child) -> std::pair<float, float> {
		if (child.kind == Kind::Text) {
			auto const measured { measure_leaf(child) };
			float main_size {
				is_row ? (child.layout.fixed_width > 0.0f
				                 ? child.layout.fixed_width
				                 : (child.layout.flex_basis >= 0.0f
				                           ? child.layout.flex_basis
				                           : measured.width))
				       : (child.layout.fixed_height > 0.0f
				                 ? child.layout.fixed_height
				                 : (child.layout.flex_basis >= 0.0f
				                           ? child.layout.flex_basis
				                           : measured.height)),
			};
			float cross_size { is_row ? measured.height : measured.width };
			return { std::max(0.0f, main_size), std::max(0.0f, cross_size) };
		}

		if (child.kind == Kind::Icon) {
			float main_size {
				is_row ? (child.layout.fixed_width > 0.0f
				                 ? child.layout.fixed_width
				                 : (child.layout.flex_basis >= 0.0f
				                           ? child.layout.flex_basis
				                           : child.content.icon_size))
				       : (child.layout.fixed_height > 0.0f
				                 ? child.layout.fixed_height
				                 : (child.layout.flex_basis >= 0.0f
				                           ? child.layout.flex_basis
				                           : child.content.icon_size)),
			};
			float cross_size { child.content.icon_size };
			return { std::max(0.0f, main_size), std::max(0.0f, cross_size) };
		}

		if (child.kind == Kind::Spacer) {
			float main_size {
				is_row ? (child.layout.fixed_width > 0.0f
				                 ? child.layout.fixed_width
				                 : (child.layout.flex_basis >= 0.0f
				                           ? child.layout.flex_basis
				                           : 0.0f))
				       : (child.layout.fixed_height > 0.0f
				                 ? child.layout.fixed_height
				                 : (child.layout.flex_basis >= 0.0f
				                           ? child.layout.flex_basis
				                           : 0.0f)),
			};
			float cross_size {
				is_row ? std::max(0.0f, child.layout.fixed_height)
				       : std::max(0.0f, child.layout.fixed_width),
			};
			return { std::max(0.0f, main_size), std::max(0.0f, cross_size) };
		}

		auto const measured { measure_node(child, inner_width) };
		float main_size {
			is_row ? (child.layout.fixed_width > 0.0f
			                 ? child.layout.fixed_width
			                 : (child.layout.flex_basis >= 0.0f
			                           ? child.layout.flex_basis
			                           : measured.width))
			       : (child.layout.fixed_height > 0.0f
			                 ? child.layout.fixed_height
			                 : (child.layout.flex_basis >= 0.0f
			                           ? child.layout.flex_basis
			                           : measured.height)),
		};
		float cross_size { is_row ? measured.height : measured.width };
		return { std::max(0.0f, main_size), std::max(0.0f, cross_size) };
	} };

	std::vector<LineLayout> lines {};
	lines.reserve(std::max<size_t>(1, node.children.size()));

	lines.push_back(LineLayout {});
	if (node.layout.flex_wrap == FlexWrap::NoWrap) {
		lines.back().items.reserve(node.children.size());
	}

	for (auto &child_ptr : node.children) {
		auto &child { *child_ptr };
		auto const [base_main, base_cross] { resolve_child_basis(child) };

		auto const needed {
			lines.back().items.empty()
			    ? base_main
			    : (lines.back().main_used + gap_main + base_main),
		};

		if (node.layout.flex_wrap == FlexWrap::Wrap && available_main > 0.0f
		    && !lines.back().items.empty() && needed > available_main) {
			lines.push_back(LineLayout {});
		}

		auto &line { lines.back() };
		line.items.push_back(ItemLayout {
		    .node = &child,
		    .base_main = base_main,
		    .base_cross = base_cross,
		    .used_main = base_main,
		    .resolved_align = resolve_align_items(
		        node.layout.align_items, child.layout.align_self),
		});
		line.main_used
		    += line.items.size() == 1 ? base_main : gap_main + base_main;
		line.cross_size = std::max(line.cross_size, base_cross);
	}

	for (auto &line : lines) {
		float grow_sum { 0.0f };
		float shrink_sum { 0.0f };

		for (auto const &item : line.items) {
			grow_sum += std::max(0.0f, item.node->layout.flex_grow);
			shrink_sum += std::max(
			    0.0f, item.node->layout.flex_shrink * item.base_main);
		}

		if (available_main > 0.0f) {
			float const free_main { available_main - line.main_used };

			if (free_main > 0.0f && grow_sum > 0.0f) {
				for (auto &item : line.items) {
					item.used_main = item.base_main
					    + free_main
					        * (std::max(0.0f, item.node->layout.flex_grow)
					            / grow_sum);
				}
			} else if (free_main < 0.0f && shrink_sum > 0.0f) {
				for (auto &item : line.items) {
					auto const weight {
						std::max(0.0f,
						    item.node->layout.flex_shrink * item.base_main)
						    / shrink_sum,
					};
					item.used_main
					    = std::max(0.0f, item.base_main + free_main * weight);
				}
			}
		}

		line.main_used = 0.0f;
		line.cross_size = 0.0f;
		for (size_t i {}; i < line.items.size(); ++i) {
			line.main_used += line.items[i].used_main;
			if (i + 1 < line.items.size()) {
				line.main_used += gap_main;
			}
			line.cross_size
			    = std::max(line.cross_size, line.items[i].base_cross);
		}

		if (is_row && node.layout.flex_wrap == FlexWrap::NoWrap
		    && inner_height > 0.0f) {
			line.cross_size = inner_height;
		} else if (!is_row && node.layout.flex_wrap == FlexWrap::NoWrap
		    && inner_width > 0.0f) {
			line.cross_size = inner_width;
		}
	}

	float total_cross { 0.0f };
	for (size_t i {}; i < lines.size(); ++i) {
		total_cross += lines[i].cross_size;
		if (i + 1 < lines.size()) {
			total_cross += gap_cross;
		}
	}

	float cross_offset { 0.0f };
	float cross_spacing { gap_cross };

	if (available_cross > 0.0f && !lines.empty()) {
		float const free_cross { std::max(
			0.0f, available_cross - total_cross) };

		switch (node.layout.align_content) {
		case AlignContent::End:
			cross_offset = free_cross;
			break;
		case AlignContent::Center:
			cross_offset = free_cross * 0.5f;
			break;
		case AlignContent::SpaceBetween:
			cross_spacing = lines.size() > 1
			    ? gap_cross + free_cross / static_cast<float>(lines.size() - 1)
			    : gap_cross;
			break;
		case AlignContent::SpaceAround:
			cross_spacing
			    = gap_cross + free_cross / static_cast<float>(lines.size());
			cross_offset = cross_spacing * 0.5f;
			break;
		case AlignContent::SpaceEvenly:
			cross_spacing
			    = gap_cross + free_cross / static_cast<float>(lines.size() + 1);
			cross_offset = cross_spacing;
			break;
		case AlignContent::Stretch:
			for (auto &line : lines) {
				line.cross_size
				    += free_cross / static_cast<float>(lines.size());
			}
			break;
		case AlignContent::Start:
		default:
			break;
		}
	}

	float content_main_max { 0.0f };
	float cross_cursor { cross_offset };

	for (size_t line_index {}; line_index < lines.size(); ++line_index) {
		auto &line { lines[line_index] };

		float main_offset { 0.0f };
		float main_spacing { gap_main };

		if (available_main > 0.0f && !line.items.empty()) {
			float const free_main { std::max(
				0.0f, available_main - line.main_used) };

			switch (node.layout.justify_content) {
			case JustifyContent::End:
				main_offset = free_main;
				break;
			case JustifyContent::Center:
				main_offset = free_main * 0.5f;
				break;
			case JustifyContent::SpaceBetween:
				main_spacing = line.items.size() > 1 ? gap_main
				        + free_main / static_cast<float>(line.items.size() - 1)
				                                     : gap_main;
				break;
			case JustifyContent::SpaceAround:
				main_spacing = gap_main
				    + free_main / static_cast<float>(line.items.size());
				main_offset = main_spacing * 0.5f;
				break;
			case JustifyContent::SpaceEvenly:
				main_spacing = gap_main
				    + free_main / static_cast<float>(line.items.size() + 1);
				main_offset = main_spacing;
				break;
			case JustifyContent::Start:
			default:
				break;
			}
		}

		float main_cursor {
			reverse
			    ? (available_main > 0.0f ? available_main - main_offset : 0.0f)
			    : main_offset,
		};

		for (auto &item : line.items) {
			float cross_size { item.base_cross };
			if (item.resolved_align == AlignItems::Stretch
			    && available_cross > 0.0f) {
				cross_size = line.cross_size;
			}

			float cross_pos { cross_cursor };
			if (item.resolved_align == AlignItems::End) {
				cross_pos += std::max(0.0f, line.cross_size - cross_size);
			} else if (item.resolved_align == AlignItems::Center) {
				cross_pos
				    += std::max(0.0f, (line.cross_size - cross_size) * 0.5f);
			}

			float const item_main {
				reverse ? (main_cursor - item.used_main) : main_cursor,
			};

			float const child_x { inner_x + (is_row ? item_main : cross_pos) };
			float const child_y { inner_y + (is_row ? cross_pos : item_main) };
			float const child_w { is_row ? item.used_main : cross_size };
			float const child_h { is_row ? cross_size : item.used_main };

			layout_node(*item.node,
			    child_x,
			    child_y,
			    child_w,
			    child_h > 0.0f ? child_h : 0.0f);

			if (is_row && child_h > 0.0f) {
				item.node->layout.local_rect.size.y() = child_h;
			} else if (!is_row && child_w > 0.0f) {
				item.node->layout.local_rect.size.x() = child_w;
			}

			if (reverse) {
				main_cursor -= item.used_main + main_spacing;
			} else {
				main_cursor += item.used_main + main_spacing;
			}
		}

		content_main_max = std::max(content_main_max, line.main_used);
		cross_cursor += line.cross_size;
		if (line_index + 1 < lines.size()) {
			cross_cursor += cross_spacing;
		}
	}

	node.scroll.content_width = {
		is_row ? content_main_max + node.layout.padding_left
		        + node.layout.padding_right
		       : cross_cursor + node.layout.padding_left
		        + node.layout.padding_right,
	};
	node.scroll.content_height = {
		is_row ? cross_cursor + node.layout.padding_top
		        + node.layout.padding_bottom
		       : content_main_max + node.layout.padding_top
		        + node.layout.padding_bottom,
	};

	if (node.kind != Kind::Layer && node.layout.fixed_width <= 0.0f
	    && node.parent != nullptr) {
		auto const effective_parent_align {
			resolve_align_items(
			    node.parent->layout.align_items, node.layout.align_self),
		};
		if (effective_parent_align != AlignItems::Stretch) {
			rect_width = node.scroll.content_width;
		}
	}

	if (rect_height <= 0.0f) {
		rect_height = node.scroll.content_height;
	}

	clamp_rect_size(rect_width, rect_height);

	node.layout.local_rect.size = smath::Vec2 { rect_width, rect_height };
	return rect_height;
}

auto System::layout_tree() -> void
{
	m_measure_cache.clear();
	m_root->layout.local_rect.position = smath::Vec2 { 0.0f, 0.0f };
	m_root->layout.local_rect.size = m_window_rect.size;

	auto current_y { 0.0f };

	for (size_t i { 0 }; i < m_root->children.size(); ++i) {
		auto &child { *m_root->children[i] };

		if (child.kind == Kind::Layer) {
			layout_node(child,
			    0.0f,
			    0.0f,
			    m_window_rect.size.x(),
			    m_window_rect.size.y());
			continue;
		}

		auto const height {
			layout_node(child,
			    0.0f,
			    current_y,
			    m_window_rect.size.x(),
			    m_window_rect.size.y() - current_y),
		};

		current_y += height;
	}

	m_layout_dirty = false;
	m_world_dirty = true;
}

auto System::update_world_tree() -> void
{
	if (m_root == nullptr) {
		return;
	}

	update_world_node(*m_root,
	    m_window_rect.position.x(),
	    m_window_rect.position.y(),
	    0.0f,
	    0.0f);
}

auto System::update_world_subtree(Node &node) -> void
{
	if (node.parent == nullptr) {
		update_world_node(node,
		    m_window_rect.position.x(),
		    m_window_rect.position.y(),
		    0.0f,
		    0.0f);
		return;
	}
	auto *parent { node.parent };
	float parent_scroll_x { 0.0f };
	float parent_scroll_y { 0.0f };
	if (parent->kind == Kind::Scrollable) {
		parent_scroll_x = parent->scroll.scroll_x;
		parent_scroll_y = parent->scroll.scroll_y;
	}
	update_world_node(node,
	    parent->layout.world_rect.position.x(),
	    parent->layout.world_rect.position.y(),
	    parent_scroll_x,
	    parent_scroll_y);
}

auto System::update_world_node(Node &node,
    float const parent_world_x,
    float const parent_world_y,
    float const parent_scroll_x,
    float const parent_scroll_y) -> void
{
	node.layout.world_rect.position = smath::Vec2 {
		parent_world_x + node.layout.local_rect.position.x() - parent_scroll_x
		    + node.layout.translation.x(),
		parent_world_y + node.layout.local_rect.position.y() - parent_scroll_y
		    + node.layout.translation.y(),
	};
	node.layout.world_rect.size = node.layout.local_rect.size;

	float child_scroll_x { 0.0f };
	float child_scroll_y { 0.0f };
	if (node.kind == Kind::Scrollable) {
		child_scroll_x = node.scroll.scroll_x;
		child_scroll_y = node.scroll.scroll_y;
	}

	auto const child_base_x { node.layout.world_rect.position.x() };
	auto const child_base_y { node.layout.world_rect.position.y() };

	for (auto &child : node.children) {
		update_world_node(
		    *child, child_base_x, child_base_y, child_scroll_x, child_scroll_y);
	}
}

auto System::find_render_node_index(Id const key) const -> uint16_t
{
	for (uint16_t i = 0; i < m_render_nodes.size(); ++i) {
		if (m_render_nodes[i].key == key) {
			return i;
		}
	}
	return INVALID_NODE_INDEX;
}

auto System::update_render_cache_subtree(Node const &node) -> void
{
	auto const index { find_render_node_index(node.key) };
	if (index == INVALID_NODE_INDEX || index >= m_render_nodes.size()) {
		return;
	}
	auto &render_node { m_render_nodes[index] };
	render_node.rect = node.layout.world_rect;
	render_node.opacity = node.visual.opacity;
	render_node.scrim_opacity = node.visual.scrim_opacity;
	for (auto const &child : node.children) {
		update_render_cache_subtree(*child);
	}
}

auto System::end_frame(WindowHandle const handle) -> WindowFrameOutput const &
{
	if (handle.id != m_current_window.id) {
		return m_last_output;
	}
	bool rebuilt_draw_state {};
	if (m_layout_dirty) {
		layout_tree();
		m_world_dirty = true;
		m_render_cache_dirty = true;
		rebuilt_draw_state = true;
	}
	if (m_world_dirty) {
		if (!m_layout_dirty && !m_world_subtree_dirty.empty()) {
			for (auto const &key : m_world_subtree_dirty) {
				if (auto *node { find_node_by_key(key) }) {
					update_world_subtree(*node);
				}
			}
		} else {
			update_world_tree();
		}
		sync_focus();
		m_world_dirty = false;
		m_render_cache_dirty = true;
		rebuilt_draw_state = true;
	}
	if (m_render_cache_dirty) {
		if (!m_layout_dirty && !m_world_subtree_dirty.empty()
		    && !m_render_nodes.empty()) {
			for (auto const &key : m_world_subtree_dirty) {
				if (auto *node { find_node_by_key(key) }) {
					update_render_cache_subtree(*node);
				}
			}
		} else {
			m_render_nodes.clear();
			m_render_nodes.reserve(NODE_POOL_MAX);
			m_render_root_index
			    = build_render_cache_node(*m_root, 0, INVALID_NODE_INDEX);
		}
		m_world_subtree_dirty.clear();
		m_render_cache_dirty = false;
		rebuilt_draw_state = true;
	}

	auto const can_reuse_draw_list {
		!rebuilt_draw_state && !m_visual_dirty && !m_hud_visible
		    && handle.id == m_last_output.handle.id
		    && rect_equal(m_last_output.rect, m_window_rect),
	};
	if (can_reuse_draw_list) {
		return m_last_output;
	}

	m_last_output.handle = handle;
	m_last_output.rect = m_window_rect;
	m_last_output.draw_list.clear();
	m_last_output.draw_list.push_back(DrawCommand {
	    .payload = DrawCommand::Rect {
	        .rect = m_window_rect,
	        .color = m_theme.background,
	    },
	});
	m_debug_label_candidates.clear();
	m_debug_label_rects.clear();
	m_debug_label_order = 0;
	auto const screen_clip { m_window_rect };
	auto const *focused { focused_node() };
	auto const focused_key { focused != nullptr ? focused->key : Id {} };
	if (m_render_root_index != INVALID_NODE_INDEX
	    && m_render_root_index < m_render_nodes.size()) {
		auto draw_children_pass { [&](bool const hud_only) {
			auto child_index {
				m_render_nodes[m_render_root_index].first_child
			};
			while (child_index != INVALID_NODE_INDEX) {
				auto const &child { m_render_nodes[child_index] };
				auto const is_hud_layer {
					child.kind == Kind::Layer
					    && child.layer_focus_mode == LayerFocusMode::Passive,
				};
				if (is_hud_layer == hud_only) {
					render_node(m_last_output.draw_list,
					    child_index,
					    screen_clip,
					    focused_key,
					    1.0f,
					    false,
					    false);
				}
				child_index = child.next_sibling;
			}
		} };

		draw_children_pass(false);
		draw_children_pass(true);
	}
	if (m_debug_bounds) {
		draw_debug_labels(m_last_output.draw_list);
	}

	if (m_hud_visible) {
		std::string hud_line {};
		hud_line.reserve(128);
		std::format_to(std::back_inserter(hud_line),
		    "rc:{} r:{} s:{} d:{} ly:{} rn:{} cl:{} pl:{}",
		    m_stats.recomposed_scopes,
		    m_stats.recomposed_root,
		    m_stats.recomposed_sidebar,
		    m_stats.recomposed_dialog,
		    m_stats.relaid_out_nodes,
		    m_stats.rendered_nodes,
		    m_stats.culled_nodes,
		    m_node_pool.size());
		m_last_output.draw_list.push_back(DrawCommand {
		      .payload = DrawCommand::Text {
		          .value = std::move(hud_line),
		          .box = Engine::Rect<> {
		              .position = smath::Vec2 {
		  	        m_window_rect.position.x() + 4.0f,
		                  m_window_rect.position.y() + m_window_rect.size.y()
		                  - 20.0f,
		  	    },
		              .size = smath::Vec2 {
		                  std::max(0.0f, m_window_rect.size.x() - 8.0f),
		                  18.0f,
		              },
		          },
		          .size = 16.0f,
		          .color = m_theme.on_surface_variant,
		          .align_x = TextAlignX::Left,
		          .align_y = TextAlignY::Top,
			 .wrap = false,
		      },
		  });
	}

	m_visual_dirty = false;
	return m_last_output;
}

} // namespace Gui
