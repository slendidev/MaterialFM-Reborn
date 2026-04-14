#include "gui/System.h"

#include <algorithm>
#include <array>
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
constexpr float PI { 3.14159265358979323846f };
constexpr float DEFAULT_DRAWER_WIDTH { 184.0f };
constexpr float DEFAULT_MODAL_WIDTH { 280.0f };
constexpr float DEFAULT_MODAL_HEIGHT { 170.0f };
constexpr auto STATE_SALT { Gui::id("@state") };
constexpr auto TWEEN_SALT { Gui::id("@tween") };

auto approx_equal(float const a, float const b, float const epsilon = 0.0001f)
    -> bool
{
	return std::abs(a - b) <= epsilon;
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

auto color_with_alpha(smath::Vec4 color, float const alpha) -> smath::Vec4
{
	color.w() = std::clamp(alpha, 0.0f, 1.0f);
	return color;
}

auto mix_color(smath::Vec4 const a, smath::Vec4 const b, float const t)
    -> smath::Vec4
{
	auto const clamped { std::clamp(t, 0.0f, 1.0f) };
	return smath::Vec4 {
		a.x() + (b.x() - a.x()) * clamped,
		a.y() + (b.y() - a.y()) * clamped,
		a.z() + (b.z() - a.z()) * clamped,
		a.w() + (b.w() - a.w()) * clamped,
	};
}

auto choose_color(smath::Vec4 const candidate, smath::Vec4 const fallback)
    -> smath::Vec4
{
	auto const is_unset {
		std::abs(candidate.x()) <= 0.0001f && std::abs(candidate.y()) <= 0.0001f
		    && std::abs(candidate.z()) <= 0.0001f
		    && std::abs(candidate.w()) <= 0.0001f,
	};
	return is_unset ? fallback : candidate;
}

auto intersect_rects(Engine::Rect<> const a, Engine::Rect<> const b)
    -> std::optional<Engine::Rect<>>
{
	auto const a_right { a.position.x() + a.size.x() };
	auto const a_bottom { a.position.y() + a.size.y() };
	auto const b_right { b.position.x() + b.size.x() };
	auto const b_bottom { b.position.y() + b.size.y() };
	auto const x0 { std::max(a.position.x(), b.position.x()) };
	auto const y0 { std::max(a.position.y(), b.position.y()) };
	auto const x1 { std::min(a_right, b_right) };
	auto const y1 { std::min(a_bottom, b_bottom) };

	if (x1 <= x0 || y1 <= y0) {
		return std::nullopt;
	}

	return Engine::Rect<> {
		.position = smath::Vec2 { x0, y0 },
		.size = smath::Vec2 { x1 - x0, y1 - y0 },
	};
}

auto draw_rounded_fill(std::vector<DrawCommand> &draw_list,
    Engine::Rect<> const rect,
    smath::Vec4 const color,
    float const radius) -> void
{
	auto const r {
		std::clamp(radius, 0.0f, std::min(rect.size.x(), rect.size.y()) * 0.5f),
	};
	if (r <= 0.0f) {
		draw_list.push_back(DrawCommand {
		    .payload = DrawCommand::Rect { .rect = rect, .color = color },
		});
		return;
	}

	auto quarter_circle_segments { [](float const r) -> int {
		if (r <= 1.0f) {
			return 1;
		}

		auto constexpr pixels_per_segment { 3.0f };
		auto const arc_length { 0.5f * PI * r };
		auto const segments {
			static_cast<int>(std::ceil(arc_length / pixels_per_segment)),
		};

		return std::clamp(segments, 1, 16);
	} };

	auto const segments { quarter_circle_segments(radius) };

	draw_list.push_back(DrawCommand {
	    .payload = DrawCommand::Rect {
	        .rect = Engine::Rect<> {
	            .position = rect.position + smath::Vec2 { r, 0.0f },
	            .size = smath::Vec2 { rect.size.x() - 2.0f * r, rect.size.y() },
	        },
	        .color = color,
	    },
	});

	draw_list.push_back(DrawCommand {
	    .payload = DrawCommand::Rect {
	        .rect = Engine::Rect<> {
	            .position = rect.position + smath::Vec2 { 0.0f, r },
	            .size = smath::Vec2 { rect.size.x(), rect.size.y() - 2.0f * r },
	        },
	        .color = color,
	    },
	});
	draw_list.push_back(DrawCommand {
	    .payload = DrawCommand::CircleSector {
	        .center = rect.position + smath::Vec2 { r, r },
	        .radius = r,
	        .start_radians = PI,
	        .end_radians = PI * 1.5f,
	        .color = color,
	        .segments = segments,
	    },
	});
	draw_list.push_back(DrawCommand {
	    .payload = DrawCommand::CircleSector {
	        .center = rect.position + smath::Vec2 { rect.size.x() - r, r },
	        .radius = r,
	        .start_radians = PI * 1.5f,
	        .end_radians = PI * 2.0f,
	        .color = color,
	        .segments = segments,
	    },
	});
	draw_list.push_back(DrawCommand {
	    .payload = DrawCommand::CircleSector {
	        .center = rect.position + smath::Vec2 { r, rect.size.y() - r },
	        .radius = r,
	        .start_radians = PI * 0.5f,
	        .end_radians = PI,
	        .color = color,
	        .segments = segments,
	    },
	});
	draw_list.push_back(DrawCommand {
	    .payload = DrawCommand::CircleSector {
	        .center
	        = rect.position + smath::Vec2 { rect.size.x() - r, rect.size.y() - r },
	        .radius = r,
	        .start_radians = 0.0f,
	        .end_radians = PI * 0.5f,
	        .color = color,
	        .segments = segments,
	    },
	});
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

auto saturate(float const value) -> float
{
	return std::clamp(value, 0.0f, 1.0f);
}

auto debug_depth_color(uint16_t const depth) -> smath::Vec4
{
	auto const t { static_cast<float>(depth % 12u) / 12.0f };
	auto const r { saturate(std::abs(std::sin((t + 0.00f) * PI * 2.0f))) };
	auto const g { saturate(std::abs(std::sin((t + 0.33f) * PI * 2.0f))) };
	auto const b { saturate(std::abs(std::sin((t + 0.66f) * PI * 2.0f))) };
	return smath::Vec4 { r, g, b, 0.95f };
}

auto clamp_size(System::MeasuredSize size, Node const &node)
    -> System::MeasuredSize
{
	if (node.min_width > 0.0f) {
		size.width = std::max(size.width, node.min_width);
	}
	if (node.min_height > 0.0f) {
		size.height = std::max(size.height, node.min_height);
	}
	if (node.max_width > 0.0f) {
		size.width = std::min(size.width, node.max_width);
	}
	if (node.max_height > 0.0f) {
		size.height = std::min(size.height, node.max_height);
	}
	return size;
}

auto spans_overlap(
    float const a0, float const a1, float const b0, float const b1) -> bool
{
	return a1 > b0 && b1 > a0;
}
} // namespace

auto System::measure_leaf(Node const &node) const -> MeasuredSize
{
	if (node.fixed_width > 0.0f || node.fixed_height > 0.0f) {
		return clamp_size(
		    {
		        .width = std::max(0.0f, node.fixed_width),
		        .height = std::max(0.0f, node.fixed_height),
		    },
		    node);
	}

	if (node.kind == Kind::Text) {
		float w { 0.0f };
		if (m_text_measure_fn) {
			auto const measured { m_text_measure_fn(
				node.label, node.text_size) };
			w = measured.x() > 0.0f
			    ? std::max(8.0f, measured.x() + 1.5f)
			    : std::max(8.0f,
			          node.text_size * 0.56f
			              * static_cast<float>(node.label.size()));
		} else {
			w = std::max(8.0f,
			    node.text_size * 0.56f * static_cast<float>(node.label.size()));
		}
		return clamp_size(
		    {
		        .width = w,
		        .height = node.text_size + 6.0f,
		    },
		    node);
	}

	if (node.kind == Kind::Icon) {
		auto const s { std::max(1.0f, node.icon_size) };
		return clamp_size({ .width = s, .height = s }, node);
	}

	if (node.kind == Kind::Spacer) {
		return clamp_size(
		    {
		        .width = std::max(0.0f, node.fixed_width),
		        .height = std::max(0.0f, node.fixed_height),
		    },
		    node);
	}

	return clamp_size({}, node);
}

auto System::measure_node(Node const &node, float const available_width) const
    -> MeasuredSize
{
	if (node.fixed_width > 0.0f || node.fixed_height > 0.0f) {
		auto leaf { measure_leaf(node) };
		if (node.fixed_width > 0.0f) {
			leaf.width = node.fixed_width;
		}
		if (node.fixed_height > 0.0f) {
			leaf.height = node.fixed_height;
		}
		return leaf;
	}

	switch (node.kind) {
	case Kind::Text:
	case Kind::Icon:
	case Kind::Spacer:
		return measure_leaf(node);

	case Kind::Scrollable:
	case Kind::Flex:
	case Kind::Surface:
	case Kind::Pressable:
	case Kind::Layer:
	case Kind::Root:
	case Kind::Memo:
		break;
	}

	auto const is_row { is_row_direction(node.flex_direction) };
	auto const gap_main { is_row ? node.column_gap : node.row_gap };

	float main_sum { 0.0f };
	float cross_max { 0.0f };
	bool first_child { true };

	for (auto const &child_ptr : node.children) {
		auto const &child { *child_ptr };
		auto child_size { measure_node(child, available_width) };

		if (child.flex_basis >= 0.0f) {
			if (is_row) {
				child_size.width = std::max(child_size.width, child.flex_basis);
			} else {
				child_size.height
				    = std::max(child_size.height, child.flex_basis);
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
		out.width = node.padding_left + main_sum + node.padding_right;
		out.height = node.padding_top + cross_max + node.padding_bottom;
	} else {
		out.width = node.padding_left + cross_max + node.padding_right;
		out.height = node.padding_top + main_sum + node.padding_bottom;
	}

	return clamp_size(out, node);
}

System::System()
{
	m_root = std::make_unique<Node>();
	m_root->kind = Kind::Root;
	m_root->scope = Scope::Root;
	m_root->key = id("root");
	m_root->local_key = id("root");
	m_sidebar_tween.configure(Animation::TweenSpec {
	    .from = 0.0f,
	    .to = 0.0f,
	    .duration_seconds = 0.0001f,
	    .easing = Animation::Easing::Linear,
	    .repeat = Animation::RepeatMode::Once,
	});
	m_sidebar_tween.stop();
}

auto System::set_sidebar_open(bool const open) -> void
{
	if (m_sidebar_open == open) {
		return;
	}
	auto const target { open ? 1.0f : 0.0f };
	m_sidebar_tween.configure(Animation::TweenSpec {
	    .from = m_sidebar_progress,
	    .to = target,
	    .duration_seconds = 0.22f,
	    .easing = Animation::Easing::EaseOutCubic,
	    .repeat = Animation::RepeatMode::Once,
	});
	m_sidebar_open = open;
	m_structure_dirty = true;
	m_sidebar_dirty = true;
}

auto System::set_dialog_open(bool const open) -> void
{
	if (m_dialog_open == open) {
		return;
	}
	m_dialog_open = open;
	m_structure_dirty = true;
	m_dialog_dirty = true;
}

auto System::set_hud_visible(bool const visible) -> void
{
	if (m_hud_visible == visible) {
		return;
	}
	m_hud_visible = visible;
	m_visual_dirty = true;
}

auto System::selected(Id const key) const -> bool
{
	return m_selected.contains(key);
}

auto System::selected(std::string_view const key) const -> bool
{
	return selected(id(key));
}

auto System::set_icon_atlas(uint32_t const image_id, IconAtlas const &atlas)
    -> void
{
	m_icon_image_id = image_id;
	m_icon_rects = atlas.rects;
	m_visual_dirty = true;
	m_root_dirty = true;
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
	m_stats.recomposed_dialog += 1;
}

auto System::memo_should_recompose(Id const key, uint32_t const deps_hash)
    -> bool
{
	auto const it { m_memo_deps.find(key) };
	if (it == m_memo_deps.end() || it->second != deps_hash) {
		m_memo_deps[key] = deps_hash;
		m_stats.memo_misses += 1;
		return true;
	}
	m_stats.memo_hits += 1;
	return false;
}

auto System::clone_node(Node const &source, Node *const parent) const
    -> std::unique_ptr<Node>
{
	auto out { std::make_unique<Node>() };
	out->kind = source.kind;
	out->scope = source.scope;
	out->key = source.key;
	out->local_key = source.local_key;
	out->label = source.label;
	out->icon_name = source.icon_name;
	out->text_size = source.text_size;
	out->text_align_x = source.text_align_x;
	out->text_align_y = source.text_align_y;
	out->padding_top = source.padding_top;
	out->padding_right = source.padding_right;
	out->padding_bottom = source.padding_bottom;
	out->padding_left = source.padding_left;
	out->gap = source.gap;
	out->row_gap = source.row_gap;
	out->column_gap = source.column_gap;
	out->fixed_width = source.fixed_width;
	out->fixed_height = source.fixed_height;
	out->animated_width = source.animated_width;
	out->animated_height = source.animated_height;
	out->min_width = source.min_width;
	out->min_height = source.min_height;
	out->max_width = source.max_width;
	out->max_height = source.max_height;
	out->flex_grow = source.flex_grow;
	out->flex_shrink = source.flex_shrink;
	out->flex_basis = source.flex_basis;
	out->align_self = source.align_self;
	out->flex_direction = source.flex_direction;
	out->flex_wrap = source.flex_wrap;
	out->justify_content = source.justify_content;
	out->align_items = source.align_items;
	out->align_content = source.align_content;
	out->scroll_axis = source.scroll_axis;
	out->scroll_step = source.scroll_step;
	out->scroll_x = source.scroll_x;
	out->scroll_y = source.scroll_y;
	out->scroll_target_x = source.scroll_target_x;
	out->scroll_target_y = source.scroll_target_y;
	out->content_width = source.content_width;
	out->content_height = source.content_height;
	out->interactive = source.interactive;
	out->selectable = source.selectable;
	out->use_pressable_state = source.use_pressable_state;
	out->draw_fill = source.draw_fill;
	out->draw_outline = source.draw_outline;
	out->draw_scrim = source.draw_scrim;
	out->corner_radius = source.corner_radius;
	out->outline_thickness = source.outline_thickness;
	out->icon_size = source.icon_size;
	out->layer_presentation = source.layer_presentation;
	out->fill_color = source.fill_color;
	out->focus_fill_color = source.focus_fill_color;
	out->selected_fill_color = source.selected_fill_color;
	out->outline_color = source.outline_color;
	out->text_color = source.text_color;
	out->selected_text_color = source.selected_text_color;
	out->icon_tint = source.icon_tint;
	out->selected_icon_tint = source.selected_icon_tint;
	out->scrim_color = source.scrim_color;
	out->local_rect = source.local_rect;
	out->world_rect = source.world_rect;
	out->translation = source.translation;
	out->recompose_count = source.recompose_count;
	out->skip_count = source.skip_count;
	out->on_activate = source.on_activate;
	out->parent = parent;
	for (auto const &child : source.children) {
		out->children.push_back(clone_node(*child, out.get()));
	}
	return out;
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

auto System::restore_memo_child(Node &parent, Node const &source) -> void
{
	auto const local_key { source.local_key };
	auto *node { reconcile_node(&parent,
		source.kind,
		source.scope,
		local_key,
		FlexOptions::builder()
		    .padding(std::array<float, 4> { source.padding_top,
		        source.padding_right,
		        source.padding_bottom,
		        source.padding_left })
		    .gap(source.gap)
		    .row_gap(source.row_gap)
		    .column_gap(source.column_gap)
		    .width(source.fixed_width)
		    .height(source.fixed_height)
		    .flex_grow(source.flex_grow)
		    .flex_shrink(source.flex_shrink)
		    .flex_basis_px(source.flex_basis)
		    .direction(source.flex_direction)
		    .wrap(source.flex_wrap)
		    .justify_content(source.justify_content)
		    .align_items(source.align_items)
		    .align_content(source.align_content)
		    .align_self(source.align_self)
		    .build()) };
	if (source.flex_basis < 0.0f) {
		node->flex_basis = -1.0f;
	}
	node->min_width = source.min_width;
	node->min_height = source.min_height;
	node->max_width = source.max_width;
	node->max_height = source.max_height;
	node->animated_width = source.animated_width;
	node->animated_height = source.animated_height;
	node->scroll_axis = source.scroll_axis;
	node->scroll_step = source.scroll_step;
	node->scroll_x = source.scroll_x;
	node->scroll_y = source.scroll_y;
	node->scroll_target_x = source.scroll_target_x;
	node->scroll_target_y = source.scroll_target_y;
	node->label = source.label;
	node->icon_name = source.icon_name;
	node->text_size = source.text_size;
	node->text_align_x = source.text_align_x;
	node->text_align_y = source.text_align_y;
	node->interactive = source.interactive;
	node->selectable = source.selectable;
	node->use_pressable_state = source.use_pressable_state;
	node->draw_fill = source.draw_fill;
	node->draw_outline = source.draw_outline;
	node->draw_scrim = source.draw_scrim;
	node->corner_radius = source.corner_radius;
	node->outline_thickness = source.outline_thickness;
	node->icon_size = source.icon_size;
	node->layer_presentation = source.layer_presentation;
	node->fill_color = source.fill_color;
	node->focus_fill_color = source.focus_fill_color;
	node->selected_fill_color = source.selected_fill_color;
	node->outline_color = source.outline_color;
	node->text_color = source.text_color;
	node->selected_text_color = source.selected_text_color;
	node->icon_tint = source.icon_tint;
	node->selected_icon_tint = source.selected_icon_tint;
	node->scrim_color = source.scrim_color;
	node->recompose_count = source.recompose_count;
	node->skip_count = source.skip_count;
	node->on_activate = source.on_activate;

	for (auto const &child : source.children) {
		restore_memo_child(*node, *child);
	}
}

auto System::memo_store(Node const &node) -> void
{
	auto &bucket { m_memo_children[node.key] };
	bucket.clear();
	bucket.reserve(node.children.size());
	for (auto const &child : node.children) {
		bucket.push_back(clone_node(*child, nullptr));
	}
}

auto System::memo_restore(Node &node) -> bool
{
	auto const it { m_memo_children.find(node.key) };
	if (it == m_memo_children.end()) {
		return false;
	}
	node.children.clear();
	for (auto const &child : it->second) {
		restore_memo_child(node, *child);
	}
	node.skip_count += 1;
	return true;
}

auto System::build_render_cache_node(Node const &source, uint16_t const depth)
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
	    .rect = source.world_rect,
	    .text_size = source.text_size,
	    .text_align_x = source.text_align_x,
	    .text_align_y = source.text_align_y,
	    .corner_radius = source.corner_radius,
	    .outline_thickness = source.outline_thickness,
	    .icon_size = source.icon_size,
	    .scroll_x = source.scroll_x,
	    .scroll_y = source.scroll_y,
	    .padding_top = source.padding_top,
	    .padding_right = source.padding_right,
	    .padding_bottom = source.padding_bottom,
	    .padding_left = source.padding_left,
	    .interactive = source.interactive,
	    .selectable = source.selectable,
	    .use_pressable_state = source.use_pressable_state,
	    .draw_fill = source.draw_fill,
	    .draw_outline = source.draw_outline,
	    .draw_scrim = source.draw_scrim,
	    .layer_presentation = source.layer_presentation,
	    .fill_color = source.fill_color,
	    .focus_fill_color = source.focus_fill_color,
	    .selected_fill_color = source.selected_fill_color,
	    .outline_color = source.outline_color,
	    .text_color = source.text_color,
	    .selected_text_color = source.selected_text_color,
	    .icon_tint = source.icon_tint,
	    .selected_icon_tint = source.selected_icon_tint,
	    .scrim_color = source.scrim_color,
	    .depth = depth,
	    .label = &source.label,
	    .icon_name = &source.icon_name,
	    .first_child = INVALID_NODE_INDEX,
	    .next_sibling = INVALID_NODE_INDEX,
	});

	auto child_head { INVALID_NODE_INDEX };
	auto prev_child { INVALID_NODE_INDEX };
	for (auto const &child : source.children) {
		auto const child_index { build_render_cache_node(
			*child, static_cast<uint16_t>(depth + 1)) };
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
	m_current_window = handle;
	m_window_rect = rect;
	m_input = input;
	m_dt = dt;
	m_confirm_hold_started = false;
	m_stats = Stats {};

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
	m_state_touched.clear();
	m_stats.recomposed_scopes += 1;
	if (m_root_dirty) {
		mark_scope_recomposed(Scope::Root);
	}
	if (m_sidebar_dirty) {
		mark_scope_recomposed(Scope::Sidebar);
	}
	if (m_dialog_dirty) {
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

	for (auto it { m_reconcile_nodes.begin() };
	    it != m_reconcile_nodes.end();) {
		stash_orphan(std::move(it->second));
		it = m_reconcile_nodes.erase(it);
	}

	m_is_composing = false;
	if (m_recompose_requested_during_compose) {
		m_structure_dirty = true;
	} else {
		m_structure_dirty = false;
		m_root_dirty = false;
		m_sidebar_dirty = false;
		m_dialog_dirty = false;
	}
	m_layout_dirty = true;
	m_visual_dirty = true;
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
	auto const full_key { combine_id(parent->key, key) };

	std::unique_ptr<Node> node {};
	auto const existing_it { m_reconcile_nodes.find(full_key) };
	auto const reused { existing_it != m_reconcile_nodes.end() };
	if (reused) {
		node = std::move(existing_it->second);
		m_reconcile_nodes.erase(existing_it);
	} else if (!m_node_pool.empty()) {
		node = std::move(m_node_pool.back());
		m_node_pool.pop_back();
	} else {
		node = std::make_unique<Node>();
	}

	node->kind = kind;
	node->scope = scope;
	node->parent = parent;
	auto const padding { resolve_padding(options.padding()) };
	node->padding_top = padding.top;
	node->padding_right = padding.right;
	node->padding_bottom = padding.bottom;
	node->padding_left = padding.left;
	node->gap = options.gap();
	node->row_gap = options.row_gap();
	node->column_gap = options.column_gap();
	node->animated_width.reset();
	node->animated_height.reset();
	if (auto const *width_ref {
	        std::get_if<Animation::Ref>(&options.width_value()),
	    }) {
		node->animated_width = *width_ref;
		node->fixed_width = resolve_animated_float(*width_ref, full_key);
	} else {
		node->fixed_width = options.width();
	}
	if (auto const *height_ref {
	        std::get_if<Animation::Ref>(&options.height_value()),
	    }) {
		node->animated_height = *height_ref;
		node->fixed_height = resolve_animated_float(*height_ref, full_key);
	} else {
		node->fixed_height = options.height();
	}
	node->min_width = options.min_width();
	node->min_height = options.min_height();
	node->max_width = options.max_width();
	node->max_height = options.max_height();
	node->flex_grow = options.flex_grow();
	node->flex_shrink = options.flex_shrink();
	node->flex_basis = options.flex_basis();
	node->align_self = options.align_self();
	node->flex_direction = options.direction();
	node->flex_wrap = options.wrap();
	node->justify_content = options.justify_content();
	node->align_items = options.align_items();
	node->align_content = options.align_content();
	node->scroll_axis = ScrollAxis::Vertical;
	node->scroll_step = 24.0f;
	node->key = full_key;
	node->local_key = key;
	node->label.clear();
	node->icon_name.clear();
	node->text_size = 14.0f;
	node->text_align_x = TextAlignX::Left;
	node->text_align_y = TextAlignY::Top;
	node->content_width = 0.0f;
	node->content_height = 0.0f;
	node->interactive = false;
	node->selectable = false;
	node->use_pressable_state = false;
	node->draw_fill = false;
	node->draw_outline = false;
	node->draw_scrim = false;
	node->corner_radius = 0.0f;
	node->outline_thickness = 1.0f;
	node->icon_size = 24.0f;
	node->layer_presentation = LayerPresentation::Drawer;
	node->fill_color = smath::Vec4 {};
	node->focus_fill_color = smath::Vec4 {};
	node->selected_fill_color = smath::Vec4 {};
	node->outline_color = smath::Vec4 {};
	node->text_color = smath::Vec4 {};
	node->selected_text_color = smath::Vec4 {};
	node->icon_tint = smath::Vec4 {};
	node->selected_icon_tint = smath::Vec4 {};
	node->scrim_color = smath::Vec4 {};
	node->local_rect = {};
	node->world_rect = {};
	node->translation = smath::Vec2 { 0.0f, 0.0f };
	node->on_activate = {};
	node->children.clear();
	if (!reused) {
		node->recompose_count = 0;
		node->skip_count = 0;
		node->scroll_x = 0.0f;
		node->scroll_y = 0.0f;
		node->scroll_target_x = 0.0f;
		node->scroll_target_y = 0.0f;
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

	std::vector<Node *> stack {};
	stack.push_back(m_root.get());
	while (!stack.empty()) {
		auto *node { stack.back() };
		stack.pop_back();
		if (node != nullptr && node->key == key) {
			return node;
		}
		if (node == nullptr) {
			continue;
		}
		for (auto &child : node->children) {
			stack.push_back(child.get());
		}
	}

	return nullptr;
}

auto System::active_scope() const -> Scope
{
	if (m_dialog_open) {
		return Scope::Dialog;
	}
	if (m_sidebar_open) {
		return Scope::Sidebar;
	}
	return Scope::Root;
}

auto System::gather_focusables(
    Node &node, Scope const scope, std::vector<Node *> &out) -> void
{
	if (node.scope == scope && node.interactive) {
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
	auto had_to_replace_focus = false;

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
			focused->world_rect.position + (focused->world_rect.size * 0.5f),
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
			auto const top {
				parent->world_rect.position.y() + parent->padding_top,
			};
			auto const bottom {
				parent->world_rect.position.y() + parent->world_rect.size.y()
				    - parent->padding_bottom,
			};
			auto const node_top { node.world_rect.position.y() };
			auto const node_bottom { node.world_rect.position.y()
				+ node.world_rect.size.y() };
			auto const left {
				parent->world_rect.position.x() + parent->padding_left,
			};
			auto const right {
				parent->world_rect.position.x() + parent->world_rect.size.x()
				    - parent->padding_right,
			};
			auto const node_left { node.world_rect.position.x() };
			auto const node_right { node.world_rect.position.x()
				+ node.world_rect.size.x() };

			if ((parent->scroll_axis == ScrollAxis::Vertical
			        || parent->scroll_axis == ScrollAxis::Both)
			    && node_top < top) {
				parent->scroll_target_y -= (top - node_top);
			}
			if ((parent->scroll_axis == ScrollAxis::Vertical
			        || parent->scroll_axis == ScrollAxis::Both)
			    && node_bottom > bottom) {
				parent->scroll_target_y += (node_bottom - bottom);
			}
			if ((parent->scroll_axis == ScrollAxis::Horizontal
			        || parent->scroll_axis == ScrollAxis::Both)
			    && node_left < left) {
				parent->scroll_target_x -= (left - node_left);
			}
			if ((parent->scroll_axis == ScrollAxis::Horizontal
			        || parent->scroll_axis == ScrollAxis::Both)
			    && node_right > right) {
				parent->scroll_target_x += (node_right - right);
			}

			auto const max_scroll {
				std::max(0.0f,
				    parent->content_height
				        - (parent->world_rect.size.y() - parent->padding_top
				            - parent->padding_bottom)),
			};
			auto const max_scroll_x {
				std::max(0.0f,
				    parent->content_width
				        - (parent->world_rect.size.x() - parent->padding_left
				            - parent->padding_right)),
			};
			parent->scroll_target_y
			    = std::clamp(parent->scroll_target_y, 0.0f, max_scroll);
			parent->scroll_target_x
			    = std::clamp(parent->scroll_target_x, 0.0f, max_scroll_x);
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

	if (m_input.menu_pressed && !m_dialog_open) {
		set_sidebar_open(!m_sidebar_open);
	}

	if (m_input.actions_pressed && !m_dialog_open && !m_sidebar_open) {
		set_dialog_open(true);
		m_pending_selectable_activation = Id {};
	}

	if (m_input.back_pressed) {
		if (m_selection_mode) {
			m_selection_mode = false;
			m_selected.clear();
			m_visual_dirty = true;
			m_pending_selectable_activation = Id {};
			return;
		}
		if (m_dialog_open) {
			set_dialog_open(false);
			return;
		}
		if (m_sidebar_open) {
			set_sidebar_open(false);
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
				scroll_parent->world_rect.size.x() - scroll_parent->padding_left
				    - scroll_parent->padding_right) };
			auto const viewport_h { std::max(1.0f,
				scroll_parent->world_rect.size.y() - scroll_parent->padding_top
				    - scroll_parent->padding_bottom) };
			auto const max_scroll_x {
				std::max(0.0f, scroll_parent->content_width - viewport_w),
			};
			auto const max_scroll_y {
				std::max(0.0f, scroll_parent->content_height - viewport_h),
			};

			auto const deadzone { 0.18f };
			auto const speed {
				std::max(120.0f, scroll_parent->scroll_step * 7.0f),
			};
			if ((scroll_parent->scroll_axis == ScrollAxis::Horizontal
			        || scroll_parent->scroll_axis == ScrollAxis::Both)
			    && std::abs(m_input.stick_x) > deadzone) {
				auto const next {
					std::clamp(scroll_parent->scroll_target_x
					        + (m_input.stick_x * speed * m_dt),
					    0.0f,
					    max_scroll_x),
				};
				changed = changed
				    || std::abs(next - scroll_parent->scroll_target_x) > 0.01f;
				scroll_parent->scroll_target_x = next;
			}
			if ((scroll_parent->scroll_axis == ScrollAxis::Vertical
			        || scroll_parent->scroll_axis == ScrollAxis::Both)
			    && std::abs(m_input.stick_y) > deadzone) {
				auto const next {
					std::clamp(scroll_parent->scroll_target_y
					        + (m_input.stick_y * speed * m_dt),
					    0.0f,
					    max_scroll_y),
				};
				changed = changed
				    || std::abs(next - scroll_parent->scroll_target_y) > 0.01f;
				scroll_parent->scroll_target_y = next;
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

	auto center_of = [](Node const *const node) {
		return smath::Vec2 {
			node->world_rect.position.x() + node->world_rect.size.x() * 0.5f,
			node->world_rect.position.y() + node->world_rect.size.y() * 0.5f,
		};
	};

	auto navigate_focus = [&](int const dir_x, int const dir_y) -> Node * {
		if (focused == nullptr) {
			return nullptr;
		}

		auto const base_center { center_of(focused) };
		auto const base_left { focused->world_rect.position.x() };
		auto const base_right { focused->world_rect.position.x()
			+ focused->world_rect.size.x() };
		auto const base_top { focused->world_rect.position.y() };
		auto const base_bottom { focused->world_rect.position.y()
			+ focused->world_rect.size.y() };

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

			auto const candidate_left { candidate->world_rect.position.x() };
			auto const candidate_right {
				candidate->world_rect.position.x()
				    + candidate->world_rect.size.x(),
			};
			auto const candidate_top { candidate->world_rect.position.y() };
			auto const candidate_bottom {
				candidate->world_rect.position.y()
				    + candidate->world_rect.size.y(),
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
			auto const candidate_left { candidate->world_rect.position.x() };
			auto const candidate_right {
				candidate->world_rect.position.x()
				    + candidate->world_rect.size.x(),
			};
			auto const candidate_top { candidate->world_rect.position.y() };
			auto const candidate_bottom {
				candidate->world_rect.position.y()
				    + candidate->world_rect.size.y(),
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
	};

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
			focused->world_rect.position + (focused->world_rect.size * 0.5f),
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

	if (m_confirm_hold_started && focused != nullptr && focused->selectable) {
		m_selection_mode = true;
		m_selected.insert(focused->key);
		m_visual_dirty = true;
		m_pending_selectable_activation = Id {};
	}

	if (m_input.confirm_pressed && focused != nullptr) {
		if (m_selection_mode && focused->selectable) {
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

		if (focused->selectable) {
			m_pending_selectable_activation = focused->key;
			return;
		}

		if (focused->on_activate) {
			focused->on_activate();
		}
	}

	if (m_confirm_released && m_pending_selectable_activation.valid()) {
		if (!m_confirm_hold_consumed) {
			auto *node {
				find_node_by_key(m_pending_selectable_activation),
			};
			if (node != nullptr && node->on_activate) {
				node->on_activate();
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
		m_sidebar_dirty = true;
	}
}

auto System::tick_scroll_animation(float const dt) -> void
{
	if (m_root == nullptr) {
		m_scroll_tweens.clear();
		return;
	}

	std::unordered_set<Id, Id::Hash> active_keys {};
	std::function<void(Node &)> animate_scroll = [&](Node &node) {
		if (node.kind == Kind::Scrollable) {
			active_keys.insert(node.key);
			auto &state { m_scroll_tweens[node.key] };

			if (!state.has_target_x
			    || !approx_equal(state.target_x, node.scroll_target_x, 0.01f)) {
				state.target_x = node.scroll_target_x;
				state.has_target_x = true;
				state.x.configure(Animation::TweenSpec {
				    .from = node.scroll_x,
				    .to = node.scroll_target_x,
				    .duration_seconds = 0.14f,
				    .easing = Animation::Easing::EaseOutCubic,
				    .repeat = Animation::RepeatMode::Once,
				});
			}
			if (!state.has_target_y
			    || !approx_equal(state.target_y, node.scroll_target_y, 0.01f)) {
				state.target_y = node.scroll_target_y;
				state.has_target_y = true;
				state.y.configure(Animation::TweenSpec {
				    .from = node.scroll_y,
				    .to = node.scroll_target_y,
				    .duration_seconds = 0.14f,
				    .easing = Animation::Easing::EaseOutCubic,
				    .repeat = Animation::RepeatMode::Once,
				});
			}

			auto const before_x { node.scroll_x };
			auto const before_y { node.scroll_y };
			state.x.tick(dt);
			state.y.tick(dt);
			node.scroll_x = state.x.value();
			node.scroll_y = state.y.value();
			if (std::abs(node.scroll_x - node.scroll_target_x) < 0.01f) {
				node.scroll_x = node.scroll_target_x;
			}
			if (std::abs(node.scroll_y - node.scroll_target_y) < 0.01f) {
				node.scroll_y = node.scroll_target_y;
			}
			if (!approx_equal(node.scroll_x, before_x)
			    || !approx_equal(node.scroll_y, before_y)) {
				m_world_dirty = true;
				m_visual_dirty = true;
			}
		}

		for (auto &child : node.children) {
			animate_scroll(*child);
		}
	};
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
		resolved_key = id(ref.key);
	} else {
		resolved_key
		    = combine_id(combine_id(owner_key, TWEEN_SALT), id(ref.key));
	}

	auto &track { m_tween_tracks[resolved_key] };
	auto const spec_changed {
		!track.initialized || !tween_spec_equal(track.spec, ref.spec),
	};
	auto const generation_changed {
		!track.has_generation || track.generation != ref.generation,
	};
	if (spec_changed || generation_changed) {
		track.spec = ref.spec;
		track.tween.configure(track.spec);
	}
	track.pause_if = ref.pause_if;
	track.generation = ref.generation;
	track.has_generation = true;
	track.initialized = true;
	return track.tween.value();
}

auto System::tick_node_animations(float const dt, bool const advance) -> void
{
	for (auto &pair : m_tween_tracks) {
		auto &track { pair.second };
		if (!track.initialized) {
			continue;
		}
		auto const paused { track.pause_if && track.pause_if() };
		track.tween.set_paused(paused);
		if (advance) {
			if (track.tween.tick(dt)) {
				m_visual_dirty = true;
			}
		}
	}

	if (m_root == nullptr) {
		return;
	}

	std::function<void(Node &)> apply = [&](Node &node) {
		if (node.animated_width.has_value()) {
			auto const next_width {
				resolve_animated_float(*node.animated_width, node.key),
			};
			if (!approx_equal(next_width, node.fixed_width)) {
				node.fixed_width = next_width;
				m_layout_dirty = true;
				m_visual_dirty = true;
			}
		}
		if (node.animated_height.has_value()) {
			auto const next_height {
				resolve_animated_float(*node.animated_height, node.key),
			};
			if (!approx_equal(next_height, node.fixed_height)) {
				node.fixed_height = next_height;
				m_layout_dirty = true;
				m_visual_dirty = true;
			}
		}

		for (auto &child : node.children) {
			apply(*child);
		}
	};
	apply(*m_root);
}

auto System::layout_node(Node &node,
    float const x,
    float const y,
    float const width,
    float const height_constraint) -> float
{
	m_stats.relaid_out_nodes += 1;

	if (node.kind == Kind::Text) {
		auto const measured { measure_leaf(node) };

		auto text_width { measured.width };

		if (node.fixed_width > 0.0f) {
			text_width = node.fixed_width;
		} else if (node.flex_grow > 0.0f || node.flex_shrink > 0.0f) {
			text_width = std::max(measured.width, width);
		} else if (node.parent != nullptr) {
			auto const effective_align {
				resolve_align_items(node.parent->align_items, node.align_self),
			};
			if (effective_align == AlignItems::Stretch) {
				text_width = std::max(measured.width, width);
			}
		}

		node.local_rect.position = smath::Vec2 { x, y };
		node.local_rect.size = smath::Vec2 { text_width, measured.height };
		return node.local_rect.size.y();
	}
	if (node.kind == Kind::Icon) {
		node.local_rect.position = smath::Vec2 { x, y };
		node.local_rect.size = smath::Vec2 { node.icon_size, node.icon_size };
		return node.local_rect.size.y();
	}
	if (node.kind == Kind::Memo) {
		node.local_rect.position = smath::Vec2 { x, y };
		node.local_rect.size = smath::Vec2 { width, 0.0f };
	}
	if (node.kind == Kind::Spacer) {
		node.local_rect.position = smath::Vec2 { x, y };
		node.local_rect.size = smath::Vec2 { width, node.fixed_height };
		return node.local_rect.size.y();
	}

	auto const is_flex_like { node.kind == Kind::Flex
		|| node.kind == Kind::Pressable || node.kind == Kind::Surface
		|| node.kind == Kind::Layer };
	if (!is_flex_like && node.kind != Kind::Scrollable
	    && node.kind != Kind::Memo) {
		node.local_rect.position = smath::Vec2 { x, y };
		node.local_rect.size = smath::Vec2 { width, node.fixed_height };
		return node.local_rect.size.y();
	}

	auto rect_width { width };
	auto rect_height { node.fixed_height > 0.0f ? node.fixed_height
		                                        : height_constraint };

	if (node.kind != Kind::Layer) {
		auto const measured { measure_node(node, width) };

		if (node.fixed_width <= 0.0f && node.parent != nullptr) {
			auto const effective_parent_align {
				resolve_align_items(node.parent->align_items, node.align_self),
			};
			if (effective_parent_align != AlignItems::Stretch) {
				rect_width = measured.width;
			}
		}

		if (rect_height <= 0.0f) {
			rect_height = measured.height;
		}
	}

	if (node.kind == Kind::Layer) {
		if (node.layer_presentation == LayerPresentation::Drawer) {
			rect_width = node.fixed_width > 0.0f ? node.fixed_width
			                                     : DEFAULT_DRAWER_WIDTH;
			rect_height = m_window_rect.size.y();
		} else if (node.layer_presentation == LayerPresentation::Modal) {
			rect_width = node.fixed_width > 0.0f ? node.fixed_width
			                                     : DEFAULT_MODAL_WIDTH;
			rect_height = node.fixed_height > 0.0f ? node.fixed_height
			                                       : DEFAULT_MODAL_HEIGHT;
		} else {
			rect_width = m_window_rect.size.x();
			rect_height = m_window_rect.size.y();
		}
	}

	node.local_rect.position = smath::Vec2 { x, y };
	auto inner_width { std::max(
		0.0f, rect_width - (node.padding_left + node.padding_right)) };
	auto const bounded_height { rect_height > 0.0f };
	auto inner_height {
		bounded_height
		    ? std::max(
		          0.0f, rect_height - (node.padding_top + node.padding_bottom))
		    : 0.0f,
	};
	auto const inner_x { node.padding_left };
	auto const inner_y { node.padding_top };

	auto const layout_scrollable = [&]() -> float {
		auto layout_inner_width { inner_width };
		if (node.max_width > 0.0f) {
			auto const constrained_width { std::max(0.0f,
				node.max_width - (node.padding_left + node.padding_right)) };
			if (constrained_width > 0.0f) {
				layout_inner_width
				    = std::min(layout_inner_width, constrained_width);
			}
		}

		auto const stacks_horizontally {
			node.scroll_axis == ScrollAxis::Horizontal,
		};
		auto const stack_gap {
			stacks_horizontally ? node.column_gap : node.row_gap,
		};
		auto stack_cursor_x { inner_x };
		auto stack_cursor_y { inner_y };
		auto max_right { inner_x };
		auto max_bottom { inner_y };

		for (size_t i {}; i < node.children.size(); ++i) {
			auto &child_ptr { node.children[i] };
			auto &child { *child_ptr };

			auto const child_x { stack_cursor_x };
			auto const child_y { stack_cursor_y };
			auto const child_height_constraint {
				(node.scroll_axis == ScrollAxis::Vertical
				    || node.scroll_axis == ScrollAxis::Both)
				    ? 0.0f
				    : (inner_height > 0.0f ? inner_height : 0.0f),
			};

			layout_node(child,
			    child_x,
			    child_y,
			    layout_inner_width,
			    child_height_constraint);

			auto const content_x { child.local_rect.position.x() };
			auto const content_y { child.local_rect.position.y() };
			max_right
			    = std::max(max_right, content_x + child.local_rect.size.x());
			max_bottom
			    = std::max(max_bottom, content_y + child.local_rect.size.y());

			auto const has_next { i + 1 < node.children.size() };
			if (!has_next) {
				continue;
			}

			if (stacks_horizontally) {
				stack_cursor_x += child.local_rect.size.x() + stack_gap;
			} else {
				stack_cursor_y += child.local_rect.size.y() + stack_gap;
			}
		}
		node.content_width = std::max(0.0f, max_right - inner_x);
		node.content_height = std::max(0.0f, max_bottom - inner_y);

		if (node.fixed_width <= 0.0f && node.max_width > 0.0f) {
			rect_width
			    = node.content_width + node.padding_left + node.padding_right;
			rect_width = std::min(rect_width, node.max_width);
			inner_width = std::max(
			    0.0f, rect_width - (node.padding_left + node.padding_right));
		}
		if (rect_height <= 0.0f) {
			rect_height
			    = node.content_height + node.padding_top + node.padding_bottom;
			if (node.max_height > 0.0f) {
				rect_height = std::min(rect_height, node.max_height);
			}
			inner_height = std::max(
			    0.0f, rect_height - (node.padding_top + node.padding_bottom));
		}

		auto const max_scroll_y {
			std::max(0.0f, node.content_height - inner_height),
		};
		auto const max_scroll_x {
			std::max(0.0f, node.content_width - inner_width),
		};
		node.scroll_target_y
		    = std::clamp(node.scroll_target_y, 0.0f, max_scroll_y);
		node.scroll_target_x
		    = std::clamp(node.scroll_target_x, 0.0f, max_scroll_x);
		node.scroll_y = std::clamp(node.scroll_y, 0.0f, max_scroll_y);
		node.scroll_x = std::clamp(node.scroll_x, 0.0f, max_scroll_x);

		node.local_rect.size = smath::Vec2 { rect_width, rect_height };
		return node.local_rect.size.y();
	};

	if (node.kind == Kind::Scrollable) {
		return layout_scrollable();
	}

	struct ItemLayout
	{
		Node *node {};
		float base_main {};
		float base_cross {};
		float used_main {};
	};
	struct LineLayout
	{
		std::vector<ItemLayout> items {};
		float cross_size {};
		float main_used {};
	};

	auto const is_row { is_row_direction(node.flex_direction) };
	auto const reverse { is_reverse_direction(node.flex_direction) };
	auto const gap_main { is_row ? node.column_gap : node.row_gap };
	auto const gap_cross { is_row ? node.row_gap : node.column_gap };
	auto const available_main { is_row ? inner_width : inner_height };

	std::vector<LineLayout> lines {};
	auto measure_and_collect_lines = [&]() {
		lines.push_back(LineLayout {});
		for (auto &child_ptr : node.children) {
			auto &child { *child_ptr };

			auto const measured { measure_node(child, inner_width) };
			auto base_main { 0.0f };
			if (is_row) {
				base_main = child.fixed_width > 0.0f
				    ? child.fixed_width
				    : (child.flex_basis >= 0.0f ? child.flex_basis
				                                : measured.width);
			} else {
				base_main = child.fixed_height > 0.0f
				    ? child.fixed_height
				    : (child.flex_basis >= 0.0f ? child.flex_basis
				                                : measured.height);
			}

			auto const base_cross { is_row ? measured.height : measured.width };

			auto &line { lines.back() };
			auto const needed {
				line.items.empty() ? base_main
				                   : line.main_used + gap_main + base_main,
			};
			if (node.flex_wrap == FlexWrap::Wrap && available_main > 0.0f
			    && !line.items.empty() && needed > available_main) {
				lines.push_back(LineLayout {});
			}

			auto &target_line { lines.back() };
			target_line.items.push_back(ItemLayout {
			    .node = &child,
			    .base_main = std::max(0.0f, base_main),
			    .base_cross = std::max(0.0f, base_cross),
			    .used_main = std::max(0.0f, base_main),
			});
			target_line.main_used += target_line.items.size() == 1
			    ? std::max(0.0f, base_main)
			    : gap_main + std::max(0.0f, base_main);
			target_line.cross_size
			    = std::max(target_line.cross_size, base_cross);
		}
	};
	measure_and_collect_lines();

	auto resolve_line_sizes = [&]() {
		for (auto &line : lines) {
			auto grow_sum { 0.0f };
			auto shrink_sum { 0.0f };
			for (auto const &item : line.items) {
				grow_sum += std::max(0.0f, item.node->flex_grow);
				shrink_sum
				    += std::max(0.0f, item.node->flex_shrink * item.base_main);
			}
			if (available_main > 0.0f) {
				auto free_main { available_main - line.main_used };
				if (free_main > 0.0f && grow_sum > 0.0f) {
					for (auto &item : line.items) {
						item.used_main = item.base_main
						    + free_main
						        * (std::max(0.0f, item.node->flex_grow)
						            / grow_sum);
					}
				} else if (free_main < 0.0f && shrink_sum > 0.0f) {
					for (auto &item : line.items) {
						auto const weight {
							std::max(
							    0.0f, item.node->flex_shrink * item.base_main)
							    / shrink_sum,
						};
						item.used_main = std::max(
						    0.0f, item.base_main + free_main * weight);
					}
				}
			}
			line.main_used = 0.0f;
			line.cross_size = 0.0f;
			for (size_t i { 0 }; i < line.items.size(); ++i) {
				line.main_used += line.items[i].used_main;
				if (i + 1 < line.items.size()) {
					line.main_used += gap_main;
				}
				line.cross_size
				    = std::max(line.cross_size, line.items[i].base_cross);
			}
			if (is_row && node.flex_wrap == FlexWrap::NoWrap
			    && inner_height > 0.0f) {
				line.cross_size = inner_height;
			} else if (!is_row && node.flex_wrap == FlexWrap::NoWrap
			    && inner_width > 0.0f) {
				line.cross_size = inner_width;
			}
		}
	};
	resolve_line_sizes();

	auto total_cross { 0.0f };
	for (size_t i { 0 }; i < lines.size(); ++i) {
		total_cross += lines[i].cross_size;
		if (i + 1 < lines.size()) {
			total_cross += gap_cross;
		}
	}
	auto const available_cross { is_row ? inner_height : inner_width };
	auto cross_offset { 0.0f };
	auto cross_spacing { gap_cross };
	auto apply_align_content = [&]() {
		if (available_cross <= 0.0f || lines.empty()) {
			return;
		}

		auto const free_cross {
			std::max(0.0f, available_cross - total_cross),
		};
		switch (node.align_content) {
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
	};
	apply_align_content();

	auto content_main_max { 0.0f };
	auto cross_cursor { cross_offset };
	auto arrange_lines = [&]() {
		for (auto line_it = lines.begin(); line_it != lines.end(); ++line_it) {
			auto &line { *line_it };
			auto main_offset { 0.0f };
			auto main_spacing { gap_main };
			if (available_main > 0.0f && !line.items.empty()) {
				auto const free_main {
					std::max(0.0f, available_main - line.main_used),
				};
				switch (node.justify_content) {
				case JustifyContent::End:
					main_offset = free_main;
					break;
				case JustifyContent::Center:
					main_offset = free_main * 0.5f;
					break;
				case JustifyContent::SpaceBetween:
					main_spacing = line.items.size() > 1 ? gap_main
					        + free_main
					            / static_cast<float>(line.items.size() - 1)
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

			auto main_cursor { main_offset };
			if (reverse) {
				main_cursor = available_main > 0.0f
				    ? available_main - main_offset
				    : 0.0f;
			}

			for (auto &item : line.items) {
				auto const effective_align {
					resolve_align_items(
					    node.align_items, item.node->align_self),
				};
				auto cross_size { item.base_cross };
				if (effective_align == AlignItems::Stretch
				    && available_cross > 0.0f) {
					cross_size = line.cross_size;
				}

				auto cross_pos { cross_cursor };
				if (effective_align == AlignItems::End) {
					cross_pos += std::max(0.0f, line.cross_size - cross_size);
				} else if (effective_align == AlignItems::Center) {
					cross_pos += std::max(
					    0.0f, (line.cross_size - cross_size) * 0.5f);
				}

				auto const item_main {
					reverse ? (main_cursor - item.used_main) : main_cursor,
				};
				auto child_x { inner_x + (is_row ? item_main : cross_pos) };
				auto child_y { inner_y + (is_row ? cross_pos : item_main) };
				auto child_w { is_row ? item.used_main : cross_size };
				auto child_h { is_row ? cross_size : item.used_main };
				layout_node(*item.node,
				    child_x,
				    child_y,
				    child_w,
				    child_h > 0.0f ? child_h : 0.0f);
				if (is_row && child_h > 0.0f) {
					item.node->local_rect.size.y() = child_h;
				}

				if (reverse) {
					main_cursor -= item.used_main + main_spacing;
				} else {
					main_cursor += item.used_main + main_spacing;
				}
			}

			content_main_max = std::max(content_main_max, line.main_used);
			cross_cursor += line.cross_size;
			if (line_it + 1 != lines.end()) {
				cross_cursor += cross_spacing;
			}
		}
	};
	arrange_lines();

	node.content_width = is_row
	    ? content_main_max + node.padding_left + node.padding_right
	    : cross_cursor + node.padding_left + node.padding_right;
	node.content_height = is_row
	    ? cross_cursor + node.padding_top + node.padding_bottom
	    : content_main_max + node.padding_top + node.padding_bottom;

	if (node.fixed_width <= 0.0f && node.parent != nullptr) {
		auto const effective_parent_align {
			resolve_align_items(node.parent->align_items, node.align_self),
		};
		if (effective_parent_align != AlignItems::Stretch) {
			rect_width = node.content_width;
		}
	}

	if (rect_height <= 0.0f) {
		rect_height = node.content_height;
	}

	if (node.min_width > 0.0f) {
		rect_width = std::max(rect_width, node.min_width);
	}
	if (node.min_height > 0.0f) {
		rect_height = std::max(rect_height, node.min_height);
	}
	if (node.max_width > 0.0f) {
		rect_width = std::min(rect_width, node.max_width);
	}
	if (node.max_height > 0.0f) {
		rect_height = std::min(rect_height, node.max_height);
	}

	node.local_rect.size = smath::Vec2 { rect_width, rect_height };
	return node.local_rect.size.y();
}

auto System::layout_tree() -> void
{
	m_root->local_rect.position = smath::Vec2 { 0.0f, 0.0f };
	m_root->local_rect.size = m_window_rect.size;

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

auto System::update_world_node(Node &node,
    float const parent_world_x,
    float const parent_world_y,
    float const parent_scroll_x,
    float const parent_scroll_y) -> void
{
	if (node.kind == Kind::Layer
	    && node.layer_presentation == LayerPresentation::Drawer) {
		node.translation.x()
		    = -(1.0f - m_sidebar_progress) * node.local_rect.size.x();
		node.translation.y() = 0.0f;
	}

	node.world_rect.position = smath::Vec2 {
		parent_world_x + node.local_rect.position.x() - parent_scroll_x
		    + node.translation.x(),
		parent_world_y + node.local_rect.position.y() - parent_scroll_y
		    + node.translation.y(),
	};
	node.world_rect.size = node.local_rect.size;

	float child_scroll_x { 0.0f };
	float child_scroll_y { 0.0f };
	if (node.kind == Kind::Scrollable) {
		child_scroll_x = node.scroll_x;
		child_scroll_y = node.scroll_y;
	}

	auto const child_base_x { node.world_rect.position.x() };
	auto const child_base_y { node.world_rect.position.y() };

	for (auto &child : node.children) {
		update_world_node(
		    *child, child_base_x, child_base_y, child_scroll_x, child_scroll_y);
	}
}

auto System::render_node(std::vector<DrawCommand> &draw_list,
    uint16_t const node_index,
    Engine::Rect<> const clip_rect,
    Id const focused_key,
    bool const parent_pressable_focused,
    bool const parent_pressable_selected) -> void
{
	if (node_index == INVALID_NODE_INDEX
	    || node_index >= m_render_nodes.size()) {
		return;
	}

	auto const &node { m_render_nodes[node_index] };
	auto const key { node.key };
	auto const &label { *node.label };
	auto const &icon_name { *node.icon_name };
	auto node_rect { node.rect };

	auto const visible_rect { intersect_rects(node_rect, clip_rect) };
	if (!visible_rect.has_value()) {
		m_stats.culled_nodes += 1;
		return;
	}

	m_stats.rendered_nodes += 1;

	auto const scope_is_active { [&]() {
		auto const node_scope { node.scope };
		if (m_dialog_open) {
			return node_scope == Scope::Dialog;
		}
		if (m_sidebar_open) {
			return node_scope == Scope::Sidebar;
		}
		return node_scope == Scope::Root;
	}() };

	auto const focused_here { scope_is_active && focused_key.valid()
		&& focused_key == key };
	auto const selected_here { scope_is_active && m_selected.contains(key) };
	auto const pressable_focused {
		node.kind == Kind::Pressable && scope_is_active
		    ? focused_here
		    : parent_pressable_focused,
	};
	auto const pressable_selected {
		node.kind == Kind::Pressable && scope_is_active
		    ? selected_here
		    : parent_pressable_selected,
	};

	if (node.kind == Kind::Text) {
		auto text_color {
			choose_color(node.text_color, m_theme.on_surface),
		};
		if (node.use_pressable_state && pressable_selected) {
			text_color
			    = choose_color(node.selected_text_color, m_theme.on_primary);
		}
		draw_list.push_back(DrawCommand { .payload = DrawCommand::Text {
		                                      .value = label,
		                                      .box = node.rect,
		                                      .size = node.text_size,
		                                      .color = text_color,
		                                      .align_x = node.text_align_x,
		                                      .align_y = node.text_align_y,
		                                  } });
		if (m_debug_bounds) {
			draw_debug_bounds(
			    draw_list, node.rect, node.depth, key, *visible_rect, false);
		}
		return;
	}

	if (node.kind == Kind::Surface) {
		auto fill { choose_color(node.fill_color, m_theme.surface_variant) };
		if (node.use_pressable_state) {
			if (pressable_selected) {
				fill = choose_color(node.selected_fill_color, m_theme.primary);
			} else if (pressable_focused) {
				fill = choose_color(node.focus_fill_color,
				    mix_color(fill, m_theme.primary, 0.20f));
			}
		}
		if (node.draw_fill) {
			draw_rounded_fill(draw_list, node.rect, fill, node.corner_radius);
		}
		if (node.draw_outline) {
			auto const outline {
				choose_color(node.outline_color, m_theme.outline),
			};
			draw_list.push_back(DrawCommand {
			    .payload = DrawCommand::Line {
			        .start = node.rect.position,
			        .end = node.rect.position
			            + smath::Vec2 { node.rect.size.x(), 0.0f },
			        .thickness = node.outline_thickness,
			        .color = outline,
			    } });
			draw_list.push_back(DrawCommand {
			    .payload = DrawCommand::Line {
			        .start = node.rect.position
			            + smath::Vec2 { node.rect.size.x(), 0.0f },
			        .end = node.rect.position + node.rect.size,
			        .thickness = node.outline_thickness,
			        .color = outline,
			    } });
			draw_list.push_back(DrawCommand {
			    .payload = DrawCommand::Line {
			        .start = node.rect.position + node.rect.size,
			        .end = node.rect.position
			            + smath::Vec2 { 0.0f, node.rect.size.y() },
			        .thickness = node.outline_thickness,
			        .color = outline,
			    } });
			draw_list.push_back(DrawCommand {
			    .payload = DrawCommand::Line {
			        .start = node.rect.position
			            + smath::Vec2 { 0.0f, node.rect.size.y() },
			        .end = node.rect.position,
			        .thickness = node.outline_thickness,
			        .color = outline,
			    } });
		}
	}

	if (node.kind == Kind::Icon) {
		auto icon_color {
			choose_color(node.icon_tint, m_theme.on_surface),
		};
		if (node.use_pressable_state && pressable_selected) {
			icon_color
			    = choose_color(node.selected_icon_tint, m_theme.on_primary);
		}
		auto icon_drawn { false };
		if (m_icon_image_id != 0u) {
			auto const icon_it { m_icon_rects.find(icon_name) };
			if (icon_it != m_icon_rects.end()) {
				draw_list.push_back(DrawCommand { .payload = DrawCommand::Image {
				                               .image_id = m_icon_image_id,
				                               .src = icon_it->second,
				                               .dst = Engine::Rect<> {
				                                   .position = node.rect.position,
				                                   .size = smath::Vec2 {
				                                       node.icon_size,
				                                       node.icon_size },
				                               },
				                               .color = icon_color,
				                           } });
				icon_drawn = true;
			}
		}
		if (!icon_drawn) {
			draw_list.push_back(DrawCommand { .payload = DrawCommand::Rect {
			                               .rect = Engine::Rect<> {
			                                   .position = node.rect.position,
			                                   .size = smath::Vec2 {
			                                       node.icon_size,
			                                       node.icon_size },
			                               },
			                               .color = m_theme.outline,
		                           } });
		}
		if (m_debug_bounds) {
			draw_debug_bounds(
			    draw_list, node.rect, node.depth, key, *visible_rect, false);
		}
		return;
	}

	if (node.kind == Kind::Layer) {
		if (node.draw_scrim) {
			auto scrim {
				choose_color(node.scrim_color, m_theme.scrim),
			};
			if (node.layer_presentation == LayerPresentation::Drawer) {
				scrim = color_with_alpha(scrim, m_sidebar_progress * scrim.w());
			}
			draw_list.push_back(DrawCommand { .payload = DrawCommand::Rect {
			                                      .rect = m_window_rect,
			                                      .color = scrim,
			                                  } });
		}
		if (node.draw_fill) {
			draw_rounded_fill(draw_list,
			    node.rect,
			    choose_color(node.fill_color, m_theme.surface),
			    node.corner_radius);
		}
	}

	if (node.kind == Kind::Scrollable) {
		auto const viewport_rect { Engine::Rect<> {
			.position = node.rect.position
			    + smath::Vec2 { node.padding_left, node.padding_top },
			.size = smath::Vec2 {
				std::max(0.0f,
				    node.rect.size.x() - node.padding_left - node.padding_right),
				std::max(0.0f,
				    node.rect.size.y() - node.padding_top - node.padding_bottom),
			},
		} };
		auto const scroll_clip { intersect_rects(
			viewport_rect, *visible_rect) };
		if (!scroll_clip.has_value()) {
			return;
		}
		draw_list.push_back(
		    DrawCommand { .payload = DrawCommand::PushClip { *scroll_clip } });
		auto child_index { node.first_child };
		while (child_index != INVALID_NODE_INDEX) {
			auto const &child { m_render_nodes[child_index] };
			if (!intersect_rects(child.rect, *scroll_clip).has_value()) {
				m_stats.culled_nodes += 1;
				child_index = child.next_sibling;
				continue;
			}
			render_node(draw_list,
			    child_index,
			    *scroll_clip,
			    focused_key,
			    pressable_focused,
			    pressable_selected);
			child_index = child.next_sibling;
		}
		draw_list.push_back(DrawCommand { .payload = DrawCommand::PopClip {} });
		if (m_debug_bounds) {
			draw_debug_bounds(
			    draw_list, node.rect, node.depth, key, *visible_rect, false);
		}
		return;
	}

	auto child_index { node.first_child };
	while (child_index != INVALID_NODE_INDEX) {
		auto const &child { m_render_nodes[child_index] };
		if (!intersect_rects(child.rect, *visible_rect).has_value()) {
			m_stats.culled_nodes += 1;
			child_index = child.next_sibling;
			continue;
		}
		render_node(draw_list,
		    child_index,
		    *visible_rect,
		    focused_key,
		    pressable_focused,
		    pressable_selected);
		child_index = child.next_sibling;
	}

	if (m_debug_bounds && node.kind != Kind::Root) {
		draw_debug_bounds(draw_list,
		    node.rect,
		    node.depth,
		    key,
		    *visible_rect,
		    node.kind == Kind::Layer);
	}
}

auto System::draw_debug_bounds(std::vector<DrawCommand> &draw_list,
    Engine::Rect<> const rect,
    uint16_t const depth,
    Id const key,
    Engine::Rect<> const clip_rect,
    bool const overlay) -> void
{
	auto const color { debug_depth_color(depth) };
	auto const x0 { rect.position.x() };
	auto const y0 { rect.position.y() };
	auto const x1 { rect.position.x() + rect.size.x() };
	auto const y1 { rect.position.y() + rect.size.y() };
	draw_list.push_back(DrawCommand { .payload = DrawCommand::Line {
	                                      .start = smath::Vec2 { x0, y0 },
	                                      .end = smath::Vec2 { x1, y0 },
	                                      .thickness = 1.0f,
	                                      .color = color,
	                                  } });
	draw_list.push_back(DrawCommand { .payload = DrawCommand::Line {
	                                      .start = smath::Vec2 { x1, y0 },
	                                      .end = smath::Vec2 { x1, y1 },
	                                      .thickness = 1.0f,
	                                      .color = color,
	                                  } });
	draw_list.push_back(DrawCommand { .payload = DrawCommand::Line {
	                                      .start = smath::Vec2 { x1, y1 },
	                                      .end = smath::Vec2 { x0, y1 },
	                                      .thickness = 1.0f,
	                                      .color = color,
	                                  } });
	draw_list.push_back(DrawCommand { .payload = DrawCommand::Line {
	                                      .start = smath::Vec2 { x0, y1 },
	                                      .end = smath::Vec2 { x0, y0 },
	                                      .thickness = 1.0f,
	                                      .color = color,
	                                  } });

	if (!key.valid() || rect.size.x() < 32.0f || rect.size.y() < 14.0f) {
		return;
	}
	m_debug_label_candidates.push_back(DebugLabelCandidate {
	    .rect = rect,
	    .clip_rect = clip_rect,
	    .color = color,
	    .key = key,
	    .order = m_debug_label_order++,
	    .overlay = overlay,
	});
}

auto System::draw_debug_labels(std::vector<DrawCommand> &draw_list) -> void
{
	if (m_debug_label_candidates.empty()) {
		return;
	}

	auto intersects = [](Engine::Rect<> const a, Engine::Rect<> const b) {
		auto const a_right { a.position.x() + a.size.x() };
		auto const a_bottom { a.position.y() + a.size.y() };
		auto const b_right { b.position.x() + b.size.x() };
		auto const b_bottom { b.position.y() + b.size.y() };
		return a.position.x() < b_right && a_right > b.position.x()
		    && a.position.y() < b_bottom && a_bottom > b.position.y();
	};

	std::sort(m_debug_label_candidates.begin(),
	    m_debug_label_candidates.end(),
	    [](DebugLabelCandidate const &a, DebugLabelCandidate const &b) {
		    return a.order > b.order;
	    });

	m_debug_label_rects.clear();
	struct OverlayOccluder
	{
		Engine::Rect<> rect {};
		Id key {};
	};
	std::vector<OverlayOccluder> overlay_occluders {};
	for (auto const &candidate : m_debug_label_candidates) {
		auto const clipped { intersect_rects(
			candidate.rect, candidate.clip_rect) };
		if (!clipped.has_value()) {
			continue;
		}

		if (candidate.overlay) {
			overlay_occluders.push_back(OverlayOccluder {
			    .rect = *clipped,
			    .key = candidate.key,
			});
		} else {
			auto occluded_by_overlay { false };
			for (auto const &overlay : overlay_occluders) {
				if (candidate.key == overlay.key) {
					continue;
				}
				if (intersects(candidate.rect, overlay.rect)) {
					occluded_by_overlay = true;
					break;
				}
			}
			if (occluded_by_overlay) {
				continue;
			}
		}

		std::string label { candidate.key.short_label() };
		if (label.empty()) {
			continue;
		}

		auto const rect { candidate.rect };
		auto const clip { *clipped };
		auto const strip_height { std::min(12.0f, rect.size.y() - 2.0f) };
		if (strip_height <= 2.0f) {
			continue;
		}

		auto const estimated_width {
			std::clamp(6.0f + static_cast<float>(label.size()) * 6.0f,
			    10.0f,
			    std::max(10.0f, rect.size.x() - 2.0f)),
		};
		auto label_x {
			std::max(rect.position.x() + 3.0f, clip.position.x() + 2.0f),
		};
		auto const label_y {
			std::max(rect.position.y() + 2.0f, clip.position.y() + 2.0f),
		};
		auto const strip_y {
			std::max(rect.position.y() + 1.0f, clip.position.y() + 1.0f),
		};
		auto const label_x_max { std::min(
			rect.position.x() + rect.size.x() - 8.0f,
			clip.position.x() + clip.size.x() - 8.0f) };
		if (label_x_max <= label_x) {
			continue;
		}

		auto strip_color { smath::Vec4 {
			candidate.color.x() * 0.35f + 0.65f,
			candidate.color.y() * 0.35f + 0.65f,
			candidate.color.z() * 0.35f + 0.65f,
			0.50f,
		} };
		auto const shadow_color { smath::Vec4 { 0.0f, 0.0f, 0.0f, 0.78f } };

		Engine::Rect<> label_rect {};
		for (int pass { 0 }; pass < 16; ++pass) {
			label_x
			    = std::clamp(label_x, clip.position.x() + 1.0f, label_x_max);
			auto const strip_x {
				std::max(clip.position.x() + 1.0f, label_x - 2.0f),
			};
			auto const strip_width {
				std::max(8.0f,
				    std::min(estimated_width,
				        clip.position.x() + clip.size.x() - 1.0f - strip_x)),
			};
			label_rect = Engine::Rect<> {
				.position = smath::Vec2 { strip_x, strip_y },
				.size = smath::Vec2 { strip_width, strip_height },
			};

			auto next_x { label_x };
			for (auto const &existing : m_debug_label_rects) {
				if (!intersects(label_rect, existing)) {
					continue;
				}
				next_x = std::max(
				    next_x, existing.position.x() + existing.size.x() + 2.0f);
			}
			if (next_x <= label_x + 0.01f || next_x > label_x_max) {
				break;
			}
			label_x = next_x;
		}

		draw_list.push_back(DrawCommand { .payload = DrawCommand::Rect {
		                                      .rect = label_rect,
		                                      .color = strip_color,
		                                  } });
		draw_list.push_back(DrawCommand {
		    .payload = DrawCommand::Text {
		        .value = label,
		        .box = Engine::Rect<> {
		            .position = smath::Vec2 { label_x + 1.0f, label_y + 1.0f },
		            .size = smath::Vec2 { 256.0f, 16.0f },
		        },
		        .size = 10.0f,
		        .color = shadow_color,
		        .align_x = TextAlignX::Left,
		        .align_y = TextAlignY::Top,
		    } });
		draw_list.push_back(
		    DrawCommand { .payload = DrawCommand::Text {
		                      .value = label,
		                      .box = Engine::Rect<> {
		                          .position = smath::Vec2 { label_x, label_y },
		                          .size = smath::Vec2 { 256.0f, 16.0f },
		                      },
		                      .size = 10.0f,
		                      .color = candidate.color,
		                      .align_x = TextAlignX::Left,
		                      .align_y = TextAlignY::Top,
		                  } });
		m_debug_label_rects.push_back(label_rect);
	}
}

auto System::end_frame(WindowHandle const handle) -> WindowFrameOutput const &
{
	if (handle.id != m_current_window.id) {
		return m_last_output;
	}
	if (m_layout_dirty) {
		layout_tree();
		m_world_dirty = true;
	}
	if (m_world_dirty) {
		update_world_tree();
		sync_focus();
		m_render_nodes.clear();
		m_render_nodes.reserve(NODE_POOL_MAX);
		m_render_root_index = build_render_cache_node(*m_root, 0);
		m_world_dirty = false;
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
		auto draw_children_pass = [&](bool const hud_only) {
			auto child_index {
				m_render_nodes[m_render_root_index].first_child
			};
			while (child_index != INVALID_NODE_INDEX) {
				auto const &child { m_render_nodes[child_index] };
				auto const is_hud_layer {
					child.kind == Kind::Layer
					    && child.layer_presentation == LayerPresentation::Hud,
				};
				if (is_hud_layer == hud_only) {
					render_node(m_last_output.draw_list,
					    child_index,
					    screen_clip,
					    focused_key,
					    false,
					    false);
				}
				child_index = child.next_sibling;
			}
		};

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
		    "rc:{} r:{} s:{} d:{} m:{}/{} ly:{} rn:{} cl:{} pl:{}",
		    m_stats.recomposed_scopes,
		    m_stats.recomposed_root,
		    m_stats.recomposed_sidebar,
		    m_stats.recomposed_dialog,
		    m_stats.memo_hits,
		    m_stats.memo_misses,
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

auto System::dump_tree_line(
    std::string &out, Node const &node, size_t const indent) const -> void
{
	static constexpr std::string_view kind_names[] {
		"Root",
		"Flex",
		"Scrollable",
		"Text",
		"Pressable",
		"Surface",
		"Icon",
		"Layer",
		"Memo",
		"Spacer",
	};
	for (size_t i {}; i < indent; ++i) {
		out += "  ";
	}
	auto const kind_index { static_cast<size_t>(node.kind) };
	auto const kind_name {
		kind_index < std::size(kind_names) ? kind_names[kind_index]
		                                   : std::string_view { "Unknown" },
	};
	std::format_to(std::back_inserter(out),
	    "{} local_rect=({}, {}) rc={} sk={} key={}\n",
	    kind_name,
	    node.local_rect.position,
	    node.local_rect.size,
	    node.recompose_count,
	    node.skip_count,
	    node.key.value);
	for (auto const &child : node.children) {
		dump_tree_line(out, *child, indent + 1);
	}
}

auto System::dump_tree_string(WindowHandle handle) const
    -> std::optional<std::string>
{
	if (handle.id != m_current_window.id || m_root == nullptr) {
		return std::nullopt;
	}
	std::string out {};
	out.reserve(4096);
	dump_tree_line(out, *m_root, 0);
	return out;
}

auto System::dump_tree_stdout(WindowHandle const handle) const -> void
{
	auto const out { dump_tree_string(handle) };
	if (!out)
		return;
	std::fputs(out->c_str(), stdout);
}

auto System::dump_tree_file(
    WindowHandle const handle, std::string_view const path) const -> bool
{
	auto const out { dump_tree_string(handle) };
	if (!out)
		return false;
	std::ofstream file { std::string(path) };
	if (!file.is_open()) {
		return false;
	}
	file << *out;
	return static_cast<bool>(file);
}

auto System::dump_command_list_string(WindowHandle handle) const
    -> std::optional<std::string>
{
	if (handle.id != m_last_output.handle.id) {
		return std::nullopt;
	}

	std::string out {};
	auto inserter { std::back_inserter(out) };
	for (size_t i {}; i < m_last_output.draw_list.size(); ++i) {
		std::format_to(std::back_inserter(out), "[{}]", i);
		std::visit(
		    [&](auto const &payload) {
			    using T = std::decay_t<decltype(payload)>;
			    if constexpr (std::is_same_v<T, DrawCommand::PushClip>) {
				    std::format_to(inserter,
				        "PushClip rect=({}, {})",
				        payload.rect.position,
				        payload.rect.size);
			    } else if constexpr (std::is_same_v<T, DrawCommand::PopClip>) {
				    std::format_to(inserter, "PopClip");
			    } else if constexpr (std::is_same_v<T, DrawCommand::Rect>) {
				    std::format_to(inserter,
				        "Rect rect=({}, {}) color={}",
				        payload.rect.position,
				        payload.rect.size,
				        payload.color);
			    } else if constexpr (std::is_same_v<T, DrawCommand::Line>) {
				    std::format_to(inserter,
				        "Line start={} end={} thickness={} color={}",
				        payload.start,
				        payload.end,
				        payload.thickness,
				        payload.color);
			    } else if constexpr (std::is_same_v<T,
			                             DrawCommand::CircleSector>) {
				    std::format_to(inserter,
				        "CircleSector center={} radius={} start={} end={} "
				        "seg={} color={}",
				        payload.center,
				        payload.radius,
				        payload.start_radians,
				        payload.end_radians,
				        payload.segments,
				        payload.color);
			    } else if constexpr (std::is_same_v<T, DrawCommand::Text>) {
				    std::format_to(inserter,
				        "Text box=({}, {}), size={} color={} value=\"{}\"",
				        payload.box.position,
				        payload.box.size,
				        payload.size,
				        payload.color,
				        payload.value);
			    } else if constexpr (std::is_same_v<T, DrawCommand::Image>) {
				    std::format_to(inserter,
				        "Image id={} src=({},{}) dst=({},{}) color={}",
				        payload.image_id,
				        payload.src.position,
				        payload.src.size,
				        payload.dst.position,
				        payload.dst.size,
				        payload.color);
			    }
		    },
		    m_last_output.draw_list[i].payload);
		std::format_to(std::back_inserter(out), "\n");
	}
	return out;
}

auto System::dump_command_list_stdout(WindowHandle const handle) const -> void
{
	std::print("{}", dump_command_list_string(handle).value_or(""));
}

auto System::dump_command_list_file(
    WindowHandle const handle, std::string_view const path) const -> bool
{
	auto const out { dump_command_list_string(handle) };
	if (!out)
		return false;
	std::ofstream file { std::string(path) };
	if (!file.is_open()) {
		return false;
	}
	file << *out;
	return static_cast<bool>(file);
}

} // namespace Gui
