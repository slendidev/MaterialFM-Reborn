#include "gui/System.h"

#include <algorithm>
#include <cstdio>
#include <format>
#include <fstream>
#include <print>
#include <type_traits>

namespace Gui
{
namespace
{
constexpr float PI { 3.14159265358979323846f };

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

auto intersect_debug_rects(Engine::Rect<> const a, Engine::Rect<> const b)
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
} // namespace

auto System::draw_debug_bounds(std::vector<DrawCommand> &draw_list,
    uint16_t const node_index,
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
	    .node_index = node_index,
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

	auto intersects { [](Engine::Rect<> const a, Engine::Rect<> const b) {
		auto const a_right { a.position.x() + a.size.x() };
		auto const a_bottom { a.position.y() + a.size.y() };
		auto const b_right { b.position.x() + b.size.x() };
		auto const b_bottom { b.position.y() + b.size.y() };
		return a.position.x() < b_right && a_right > b.position.x()
		    && a.position.y() < b_bottom && a_bottom > b.position.y();
	} };

	std::sort(m_debug_label_candidates.begin(),
	    m_debug_label_candidates.end(),
	    [](DebugLabelCandidate const &a, DebugLabelCandidate const &b) {
		    return a.order > b.order;
	    });

	m_debug_label_rects.clear();
	struct OverlayOccluder
	{
		uint16_t node_index { INVALID_NODE_INDEX };
		Engine::Rect<> rect {};
		Id key {};
	};
	auto const overlay_contains_node
	    = [&](uint16_t const overlay_index, uint16_t const node_index) {
		      if (overlay_index == INVALID_NODE_INDEX
		          || node_index == INVALID_NODE_INDEX) {
			      return false;
		      }
		      auto current_index { node_index };
		      while (current_index != INVALID_NODE_INDEX
		          && current_index < m_render_nodes.size()) {
			      if (current_index == overlay_index) {
				      return true;
			      }
			      current_index = m_render_nodes[current_index].parent_index;
		      }
		      return false;
	      };
	std::vector<OverlayOccluder> overlay_occluders {};
	for (auto const &candidate : m_debug_label_candidates) {
		auto const clipped {
			intersect_debug_rects(candidate.rect, candidate.clip_rect),
		};
		if (!clipped.has_value()) {
			continue;
		}

		if (candidate.overlay) {
			overlay_occluders.push_back(OverlayOccluder {
			    .node_index = candidate.node_index,
			    .rect = *clipped,
			    .key = candidate.key,
			});
		} else {
			auto occluded_by_overlay { false };
			for (auto const &overlay : overlay_occluders) {
				if (candidate.key == overlay.key) {
					continue;
				}
				if (overlay_contains_node(
				        overlay.node_index, candidate.node_index)) {
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
		auto const label_y_max { std::min(
			rect.position.y() + rect.size.y() - 12.0f,
			clip.position.y() + clip.size.y() - 12.0f) };
		auto const strip_y_max { std::min(
			rect.position.y() + rect.size.y() - strip_height - 1.0f,
			clip.position.y() + clip.size.y() - strip_height - 1.0f) };
		auto const label_x_max { std::min(
			rect.position.x() + rect.size.x() - 8.0f,
			clip.position.x() + clip.size.x() - 8.0f) };
		if (label_x_max <= label_x || label_y_max < label_y
		    || strip_y_max < strip_y) {
			continue;
		}

		auto strip_color { smath::Vec4 {
			candidate.color.x() * 0.35f + 0.65f,
			candidate.color.y() * 0.35f + 0.65f,
			candidate.color.z() * 0.35f + 0.65f,
			0.50f,
		} };
		auto const shadow_color { smath::Vec4 { 0.0f, 0.0f, 0.0f, 0.78f } };

		auto const row_step { strip_height + 2.0f };
		auto make_label_rect = [&](float const candidate_x,
		                           float const candidate_strip_y) {
			auto const clamped_x {
				std::clamp(candidate_x, clip.position.x() + 1.0f, label_x_max),
			};
			auto const strip_x {
				std::max(clip.position.x() + 1.0f, clamped_x - 2.0f),
			};
			auto const strip_width {
				std::max(8.0f,
				    std::min(estimated_width,
				        clip.position.x() + clip.size.x() - 1.0f - strip_x)),
			};
			return Engine::Rect<> {
				.position = smath::Vec2 { strip_x, candidate_strip_y },
				.size = smath::Vec2 { strip_width, strip_height },
			};
		};
		auto row_blocked = [&](Engine::Rect<> const &candidate_rect) {
			for (auto const &existing : m_debug_label_rects) {
				auto const candidate_bottom {
					candidate_rect.position.y() + candidate_rect.size.y(),
				};
				auto const existing_bottom {
					existing.position.y() + existing.size.y(),
				};
				auto const vertical_overlap {
					std::min(candidate_bottom, existing_bottom)
					    - std::max(
					        candidate_rect.position.y(), existing.position.y()),
				};
				if (vertical_overlap <= 0.0f) {
					continue;
				}

				auto const candidate_right {
					candidate_rect.position.x() + candidate_rect.size.x(),
				};
				auto const existing_right {
					existing.position.x() + existing.size.x(),
				};
				auto const horizontal_overlap {
					std::min(candidate_right, existing_right)
					    - std::max(
					        candidate_rect.position.x(), existing.position.x()),
				};
				auto const same_lane {
					horizontal_overlap >= std::min(candidate_rect.size.x(),
					                          existing.size.x())
					            * 0.4f
					    || std::abs(candidate_rect.position.x()
					           - existing.position.x())
					        <= 10.0f,
				};
				if (same_lane) {
					return true;
				}
			}
			return false;
		};

		float placed_label_x { label_x };
		float placed_label_y { label_y };
		Engine::Rect<> label_rect {};
		bool placed {};

		for (int row_pass { 0 }; row_pass < 16; ++row_pass) {
			auto const row_label_y {
				label_y + row_step * static_cast<float>(row_pass),
			};
			auto const row_strip_y {
				strip_y + row_step * static_cast<float>(row_pass),
			};
			if (row_label_y > label_y_max || row_strip_y > strip_y_max) {
				break;
			}
			auto const candidate_rect { make_label_rect(label_x, row_strip_y) };
			if (row_blocked(candidate_rect)) {
				continue;
			}
			placed_label_x = label_x;
			placed_label_y = row_label_y;
			label_rect = candidate_rect;
			placed = true;
			break;
		}

		if (!placed) {
			for (int row_pass { 0 }; row_pass < 16 && !placed; ++row_pass) {
				auto const row_label_y {
					label_y + row_step * static_cast<float>(row_pass),
				};
				auto const row_strip_y {
					strip_y + row_step * static_cast<float>(row_pass),
				};
				if (row_label_y > label_y_max || row_strip_y > strip_y_max) {
					break;
				}

				auto row_label_x { label_x };
				for (int pass { 0 }; pass < 16; ++pass) {
					auto const candidate_rect {
						make_label_rect(row_label_x, row_strip_y),
					};
					auto next_x { row_label_x };
					for (auto const &existing : m_debug_label_rects) {
						if (!intersects(candidate_rect, existing)) {
							continue;
						}
						next_x = std::max(next_x,
						    existing.position.x() + existing.size.x() + 2.0f);
					}
					if (next_x <= row_label_x + 0.01f) {
						placed_label_x = row_label_x;
						placed_label_y = row_label_y;
						label_rect = candidate_rect;
						placed = true;
						break;
					}
					if (next_x > label_x_max) {
						break;
					}
					row_label_x = next_x;
				}
			}
		}

		if (!placed) {
			continue;
		}

		draw_list.push_back(DrawCommand { .payload = DrawCommand::Rect {
		                                      .rect = label_rect,
		                                      .color = strip_color,
		                                  } });
		draw_list.push_back(DrawCommand {
		    .payload = DrawCommand::Text {
		        .value = label,
		        .box = Engine::Rect<> {
		            .position = smath::Vec2 {
		                placed_label_x + 1.0f, placed_label_y + 1.0f },
		            .size = smath::Vec2 { 256.0f, 16.0f },
		        },
		        .size = 10.0f,
		        .color = shadow_color,
		        .align_x = TextAlignX::Left,
		        .align_y = TextAlignY::Top,
		    } });
		draw_list.push_back(
		    DrawCommand { .payload = DrawCommand::Text {
		                      .value = std::move(label),
		                      .box = Engine::Rect<> {
		                          .position = smath::Vec2 {
		                              placed_label_x, placed_label_y },
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
	    node.layout.local_rect.position,
	    node.layout.local_rect.size,
	    node.recompose_count,
	    node.skip_count,
	    node.key.label());
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
				        payload.value_view());
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
