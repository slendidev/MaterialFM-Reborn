#include "gui/System.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace Gui
{
namespace
{
constexpr float PI { 3.14159265358979323846f };

auto scope_accepts_focus(Scope const node_scope, Scope const active_scope)
    -> bool
{
	return node_scope == Scope::Hud || node_scope == active_scope;
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
	return a + (b - a) * clamped;
}

auto choose_color(std::optional<smath::Vec4> const &candidate,
    smath::Vec4 const fallback) -> smath::Vec4
{
	return candidate.value_or(fallback);
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

	auto quarter_circle_segments { [](float const r_value) -> int {
		if (r_value <= 1.0f) {
			return 1;
		}

		auto constexpr pixels_per_segment { 3.0f };
		auto const arc_length { 0.5f * PI * r_value };
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

auto flush_clipped_passes(std::vector<DrawCommand> &out,
    PassBuckets const &passes,
    Engine::Rect<> const clip_rect) -> void
{
	if (passes.shapes.empty() && passes.text.empty() && passes.icons.empty()
	    && passes.overlay.empty()) {
		return;
	}

	out.push_back(
	    DrawCommand { .payload = DrawCommand::PushClip { clip_rect } });
	passes.flush_into(out);
	out.push_back(DrawCommand { .payload = DrawCommand::PopClip {} });
}
} // namespace

auto System::try_make_render_state(System::RenderNode const &node,
    Engine::Rect<> const clip_rect,
    Id const focused_key,
    float const parent_opacity,
    bool const parent_pressable_focused,
    bool const parent_pressable_selected,
    System::RenderState &out) const -> bool
{
	auto const visible_rect { intersect_rects(node.rect, clip_rect) };
	if (!visible_rect.has_value()) {
		return false;
	}

	auto const scope_is_active {
		scope_accepts_focus(node.scope, active_scope()),
	};
	out.visible_rect = *visible_rect;
	out.opacity = std::clamp(parent_opacity * node.opacity, 0.0f, 1.0f);
	out.focused_here
	    = scope_is_active && focused_key.valid() && focused_key == node.key;
	out.selected_here = scope_is_active && m_selected.contains(node.key);
	out.pressable_focused = node.kind == Kind::Pressable && scope_is_active
	    ? out.focused_here
	    : parent_pressable_focused;
	out.pressable_selected = node.kind == Kind::Pressable && scope_is_active
	    ? out.selected_here
	    : parent_pressable_selected;
	return true;
}

auto System::render_node_clipped(std::vector<DrawCommand> &draw_list,
    uint16_t const node_index,
    Engine::Rect<> const clip_rect,
    Id const focused_key,
    float const parent_opacity,
    bool const parent_pressable_focused,
    bool const parent_pressable_selected) -> void
{
	draw_list.push_back(
	    DrawCommand { .payload = DrawCommand::PushClip { clip_rect } });
	render_node(draw_list,
	    node_index,
	    clip_rect,
	    focused_key,
	    parent_opacity,
	    parent_pressable_focused,
	    parent_pressable_selected);
	draw_list.push_back(DrawCommand { .payload = DrawCommand::PopClip {} });
}

auto System::render_block(std::vector<DrawCommand> &draw_list,
    uint16_t const node_index,
    Engine::Rect<> const clip_rect,
    Id const focused_key,
    float const parent_opacity,
    bool const parent_pressable_focused,
    bool const parent_pressable_selected) -> void
{
	if (node_index == INVALID_NODE_INDEX
	    || node_index >= m_render_nodes.size()) {
		return;
	}

	auto const &node { m_render_nodes[node_index] };
	auto const key { node.key };
	System::RenderState state {};
	if (!try_make_render_state(node,
	        clip_rect,
	        focused_key,
	        parent_opacity,
	        parent_pressable_focused,
	        parent_pressable_selected,
	        state)) {
		m_stats.culled_nodes += 1;
		return;
	}

	PassBuckets passes {};
	bool stop_after_self {};
	emit_node_self_into_passes(passes,
	    node_index,
	    focused_key,
	    parent_opacity,
	    parent_pressable_focused,
	    parent_pressable_selected,
	    stop_after_self);

	if (!stop_after_self) {
		auto child_index { node.first_child };
		while (child_index != INVALID_NODE_INDEX) {
			auto const &child { m_render_nodes[child_index] };
			if (!intersect_rects(child.rect, state.visible_rect).has_value()) {
				m_stats.culled_nodes += 1;
				child_index = child.next_sibling;
				continue;
			}

			if (child.kind == Kind::Layer || child.kind == Kind::Scrollable) {
				passes.flush_into(draw_list);
				passes = PassBuckets {};
				render_node(draw_list,
				    child_index,
				    state.visible_rect,
				    focused_key,
				    state.opacity,
				    state.pressable_focused,
				    state.pressable_selected);
			} else {
				collect_regular_subtree_into_passes(draw_list,
				    passes,
				    child_index,
				    state.visible_rect,
				    false,
				    focused_key,
				    state.opacity,
				    state.pressable_focused,
				    state.pressable_selected);
			}

			child_index = child.next_sibling;
		}
	}

	if (m_debug_bounds && node.kind != Kind::Root) {
		draw_debug_bounds(passes.overlay,
		    node_index,
		    node.rect,
		    node.depth,
		    key,
		    state.visible_rect,
		    node.kind == Kind::Layer);
	}

	passes.flush_into(draw_list);
}

auto System::render_scrollable_block(std::vector<DrawCommand> &draw_list,
    uint16_t const node_index,
    Engine::Rect<> const clip_rect,
    Id const focused_key,
    float const parent_opacity,
    bool const parent_pressable_focused,
    bool const parent_pressable_selected) -> void
{
	if (node_index == INVALID_NODE_INDEX
	    || node_index >= m_render_nodes.size()) {
		return;
	}

	auto const &node { m_render_nodes[node_index] };
	auto const key { node.key };
	System::RenderState state {};
	if (!try_make_render_state(node,
	        clip_rect,
	        focused_key,
	        parent_opacity,
	        parent_pressable_focused,
	        parent_pressable_selected,
	        state)) {
		m_stats.culled_nodes += 1;
		return;
	}

	PassBuckets passes {};
	bool stop_after_self {};
	emit_node_self_into_passes(passes,
	    node_index,
	    focused_key,
	    parent_opacity,
	    parent_pressable_focused,
	    parent_pressable_selected,
	    stop_after_self);

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
		viewport_rect, state.visible_rect) };
	if (scroll_clip.has_value()) {
		auto child_index { node.first_child };
		while (child_index != INVALID_NODE_INDEX) {
			auto const &child { m_render_nodes[child_index] };
			if (!intersect_rects(child.rect, *scroll_clip).has_value()) {
				m_stats.culled_nodes += 1;
				child_index = child.next_sibling;
				continue;
			}

			if (child.kind == Kind::Layer || child.kind == Kind::Scrollable) {
				flush_clipped_passes(draw_list, passes, *scroll_clip);
				passes = PassBuckets {};
				render_node_clipped(draw_list,
				    child_index,
				    *scroll_clip,
				    focused_key,
				    state.opacity,
				    state.pressable_focused,
				    state.pressable_selected);
			} else {
				collect_regular_subtree_into_passes(draw_list,
				    passes,
				    child_index,
				    *scroll_clip,
				    true,
				    focused_key,
				    state.opacity,
				    state.pressable_focused,
				    state.pressable_selected);
			}

			child_index = child.next_sibling;
		}

		flush_clipped_passes(draw_list, passes, *scroll_clip);
	}

	if (m_debug_bounds) {
		draw_debug_bounds(draw_list,
		    node_index,
		    node.rect,
		    node.depth,
		    key,
		    state.visible_rect,
		    false);
	}
}

auto System::collect_regular_subtree_into_passes(
    std::vector<DrawCommand> &draw_list,
    PassBuckets &passes,
    uint16_t const node_index,
    Engine::Rect<> const clip_rect,
    bool const hard_clip,
    Id const focused_key,
    float const parent_opacity,
    bool const parent_pressable_focused,
    bool const parent_pressable_selected) -> void
{
	if (node_index == INVALID_NODE_INDEX
	    || node_index >= m_render_nodes.size()) {
		return;
	}

	auto const &node { m_render_nodes[node_index] };
	auto const key { node.key };
	System::RenderState state {};
	if (!try_make_render_state(node,
	        clip_rect,
	        focused_key,
	        parent_opacity,
	        parent_pressable_focused,
	        parent_pressable_selected,
	        state)) {
		m_stats.culled_nodes += 1;
		return;
	}

	bool stop_after_self {};
	emit_node_self_into_passes(passes,
	    node_index,
	    focused_key,
	    parent_opacity,
	    parent_pressable_focused,
	    parent_pressable_selected,
	    stop_after_self);

	if (!stop_after_self) {
		auto child_index { node.first_child };
		while (child_index != INVALID_NODE_INDEX) {
			auto const &child { m_render_nodes[child_index] };
			if (!intersect_rects(child.rect, state.visible_rect).has_value()) {
				m_stats.culled_nodes += 1;
				child_index = child.next_sibling;
				continue;
			}

			if (child.kind == Kind::Layer || child.kind == Kind::Scrollable) {
				passes.flush_into(draw_list);
				passes = PassBuckets {};
				if (hard_clip) {
					render_node_clipped(draw_list,
					    child_index,
					    state.visible_rect,
					    focused_key,
					    state.opacity,
					    state.pressable_focused,
					    state.pressable_selected);
				} else {
					render_node(draw_list,
					    child_index,
					    state.visible_rect,
					    focused_key,
					    state.opacity,
					    state.pressable_focused,
					    state.pressable_selected);
				}
			} else {
				collect_regular_subtree_into_passes(draw_list,
				    passes,
				    child_index,
				    state.visible_rect,
				    hard_clip,
				    focused_key,
				    state.opacity,
				    state.pressable_focused,
				    state.pressable_selected);
			}

			child_index = child.next_sibling;
		}
	}

	if (m_debug_bounds && node.kind != Kind::Root) {
		draw_debug_bounds(passes.overlay,
		    node_index,
		    node.rect,
		    node.depth,
		    key,
		    state.visible_rect,
		    node.kind == Kind::Layer);
	}
}

auto System::emit_node_self_into_passes(PassBuckets &passes,
    uint16_t const node_index,
    Id const focused_key,
    float const parent_opacity,
    bool const parent_pressable_focused,
    bool const parent_pressable_selected,
    bool &stop_after_self) -> void
{
	if (node_index == INVALID_NODE_INDEX
	    || node_index >= m_render_nodes.size()) {
		return;
	}

	auto const &node { m_render_nodes[node_index] };
	auto const &label { *node.label };
	auto const &icon_name { *node.icon_name };
	auto const opacity {
		std::clamp(parent_opacity * node.opacity, 0.0f, 1.0f),
	};

	m_stats.rendered_nodes += 1;

	auto const scope_is_active {
		scope_accepts_focus(node.scope, active_scope()),
	};
	auto const focused_here {
		scope_is_active && focused_key.valid() && focused_key == node.key,
	};
	auto const selected_here {
		scope_is_active && m_selected.contains(node.key),
	};
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
		text_color = color_with_alpha(text_color, text_color.w() * opacity);
		passes.text.push_back(
		    DrawCommand { .payload = DrawCommand::Text {
		                      .value = std::string_view(label),
		                      .box = node.rect,
		                      .size = node.text_size,
		                      .color = text_color,
		                      .align_x = node.text_align_x,
		                      .align_y = node.text_align_y,
		                  } });
		stop_after_self = true;
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
			fill = color_with_alpha(fill, fill.w() * opacity);
			draw_rounded_fill(
			    passes.shapes, node.rect, fill, node.corner_radius);
		}
		if (node.draw_outline) {
			auto const outline {
				choose_color(node.outline_color, m_theme.outline),
			};
			auto const faded_outline {
				color_with_alpha(outline, outline.w() * opacity),
			};
			passes.shapes.push_back(DrawCommand {
			    .payload = DrawCommand::Line {
			        .start = node.rect.position,
			        .end = node.rect.position
			            + smath::Vec2 { node.rect.size.x(), 0.0f },
			        .thickness = node.outline_thickness,
			        .color = faded_outline,
			    } });
			passes.shapes.push_back(DrawCommand {
			    .payload = DrawCommand::Line {
			        .start = node.rect.position
			            + smath::Vec2 { node.rect.size.x(), 0.0f },
			        .end = node.rect.position + node.rect.size,
			        .thickness = node.outline_thickness,
			        .color = faded_outline,
			    } });
			passes.shapes.push_back(DrawCommand {
			    .payload = DrawCommand::Line {
			        .start = node.rect.position + node.rect.size,
			        .end = node.rect.position
			            + smath::Vec2 { 0.0f, node.rect.size.y() },
			        .thickness = node.outline_thickness,
			        .color = faded_outline,
			    } });
			passes.shapes.push_back(DrawCommand {
			    .payload = DrawCommand::Line {
			        .start = node.rect.position
			            + smath::Vec2 { 0.0f, node.rect.size.y() },
			        .end = node.rect.position,
			        .thickness = node.outline_thickness,
			        .color = faded_outline,
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
		icon_color = color_with_alpha(icon_color, icon_color.w() * opacity);
		auto icon_drawn { false };
		if (m_icon_image_id != 0u) {
			auto const icon_it { m_icon_rects.find(icon_name) };
			if (icon_it != m_icon_rects.end()) {
				passes.icons.push_back(DrawCommand { .payload = DrawCommand::Image {
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
			passes.shapes.push_back(DrawCommand { .payload = DrawCommand::Rect {
			                               .rect = Engine::Rect<> {
			                                   .position = node.rect.position,
			                                   .size = smath::Vec2 {
			                                       node.icon_size,
			                                       node.icon_size },
			                               },
			                               .color = color_with_alpha(
			                                   m_theme.outline,
			                                   m_theme.outline.w() * opacity),
			                           } });
		}
		stop_after_self = true;
		return;
	}

	if (node.kind == Kind::Layer) {
		if (node.draw_scrim) {
			auto scrim {
				choose_color(node.scrim_color, m_theme.scrim),
			};
			scrim = color_with_alpha(scrim, scrim.w() * node.scrim_opacity);
			passes.shapes.push_back(DrawCommand { .payload = DrawCommand::Rect {
			                                          .rect = m_window_rect,
			                                          .color = scrim,
			                                      } });
		}
		if (node.draw_fill) {
			auto const fill { color_with_alpha(
				choose_color(node.fill_color, m_theme.surface),
				choose_color(node.fill_color, m_theme.surface).w() * opacity) };
			draw_rounded_fill(
			    passes.shapes, node.rect, fill, node.corner_radius);
		}
	}

	if (node.kind == Kind::Scrollable) {
		stop_after_self = false;
		return;
	}

	if (node.kind == Kind::Pressable) {
		if (node.draw_fill) {
			auto fill { choose_color(
				node.fill_color, m_theme.surface_variant) };
			if (pressable_selected) {
				fill = choose_color(node.selected_fill_color, m_theme.primary);
			} else if (pressable_focused) {
				fill = choose_color(node.focus_fill_color,
				    mix_color(fill, m_theme.primary, 0.20f));
			}
			fill = color_with_alpha(fill, fill.w() * opacity);
			draw_rounded_fill(
			    passes.shapes, node.rect, fill, node.corner_radius);
		}
		if (node.draw_outline) {
			auto const outline {
				choose_color(node.outline_color, m_theme.outline),
			};
			auto const faded_outline {
				color_with_alpha(outline, outline.w() * opacity),
			};
			passes.shapes.push_back(DrawCommand {
			    .payload = DrawCommand::Line {
			        .start = node.rect.position,
			        .end = node.rect.position
			            + smath::Vec2 { node.rect.size.x(), 0.0f },
			        .thickness = node.outline_thickness,
			        .color = faded_outline,
			    } });
			passes.shapes.push_back(DrawCommand {
			    .payload = DrawCommand::Line {
			        .start = node.rect.position
			            + smath::Vec2 { node.rect.size.x(), 0.0f },
			        .end = node.rect.position + node.rect.size,
			        .thickness = node.outline_thickness,
			        .color = faded_outline,
			    } });
			passes.shapes.push_back(DrawCommand {
			    .payload = DrawCommand::Line {
			        .start = node.rect.position + node.rect.size,
			        .end = node.rect.position
			            + smath::Vec2 { 0.0f, node.rect.size.y() },
			        .thickness = node.outline_thickness,
			        .color = faded_outline,
			    } });
			passes.shapes.push_back(DrawCommand {
			    .payload = DrawCommand::Line {
			        .start = node.rect.position
			            + smath::Vec2 { 0.0f, node.rect.size.y() },
			        .end = node.rect.position,
			        .thickness = node.outline_thickness,
			        .color = faded_outline,
			    } });
		}
	}

	if (node.kind == Kind::Flex || node.kind == Kind::Root
	    || node.kind == Kind::Spacer) {
		stop_after_self = false;
		return;
	}

	stop_after_self = false;
}

auto System::render_node(std::vector<DrawCommand> &draw_list,
    uint16_t const node_index,
    Engine::Rect<> const clip_rect,
    Id const focused_key,
    float const parent_opacity,
    bool const parent_pressable_focused,
    bool const parent_pressable_selected) -> void
{
	if (node_index == INVALID_NODE_INDEX
	    || node_index >= m_render_nodes.size()) {
		return;
	}

	auto const &node { m_render_nodes[node_index] };
	if (node.kind == Kind::Scrollable) {
		render_scrollable_block(draw_list,
		    node_index,
		    clip_rect,
		    focused_key,
		    parent_opacity,
		    parent_pressable_focused,
		    parent_pressable_selected);
		return;
	}

	render_block(draw_list,
	    node_index,
	    clip_rect,
	    focused_key,
	    parent_opacity,
	    parent_pressable_focused,
	    parent_pressable_selected);
}

} // namespace Gui
