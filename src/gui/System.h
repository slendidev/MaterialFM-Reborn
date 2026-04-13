#pragma once

#include <any>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "engine/Math.h"
#include "gui/Animation.h"
#include "gui/IconAtlas.h"
#include "gui/Id.h"
#include "gui/Node.h"
#include "gui/Theme.h"

namespace Gui
{

class Context;
class FlexOptions;

struct Input
{
	bool up_pressed {};
	bool down_pressed {};
	bool left_pressed {};
	bool right_pressed {};
	bool confirm_pressed {};
	bool confirm_down {};
	bool back_pressed {};
	bool menu_pressed {};
	bool actions_pressed {};
	bool debug_bounds_toggle_pressed {};
	float stick_x {};
	float stick_y {};
};

struct WindowHandle
{
	uint32_t id {};
};

struct DrawCommand
{
	struct PushClip
	{
		Engine::Rect<> rect {};
	};
	struct PopClip
	{ };
	struct Rect
	{
		Engine::Rect<> rect {};
		smath::Vec4 color {};
	};
	struct Line
	{
		smath::Vec2 start {};
		smath::Vec2 end {};
		float thickness { 1.0f };
		smath::Vec4 color {};
	};
	struct CircleSector
	{
		smath::Vec2 center {};
		float radius {};
		float start_radians {};
		float end_radians {};
		smath::Vec4 color {};
		int segments { 12 };
	};
	struct Text
	{
		std::string value {};
		Engine::Rect<> box {};
		float size { 16.0f };
		smath::Vec4 color {};
		TextAlignX align_x { TextAlignX::Left };
		TextAlignY align_y { TextAlignY::Top };
	};
	struct Image
	{
		uint32_t image_id {};
		Engine::Rect<> src {};
		Engine::Rect<> dst {};
		smath::Vec4 color {};
	};

	using Payload = std::
	    variant<PushClip, PopClip, Rect, Line, CircleSector, Text, Image>;

	Payload payload { PushClip {} };
};

struct WindowFrameOutput
{
	WindowHandle handle {};
	Engine::Rect<> rect {};
	std::vector<DrawCommand> draw_list {};
};

class System
{
public:
	static constexpr uint16_t INVALID_NODE_INDEX { 0xFFFFu };

	struct Stats
	{
		int recomposed_scopes {};
		int recomposed_root {};
		int recomposed_sidebar {};
		int recomposed_dialog {};
		int relaid_out_nodes {};
		int rendered_nodes {};
		int memo_hits {};
		int memo_misses {};
		int culled_nodes {};
	};

	struct MeasuredSize
	{
		float width {};
		float height {};
	};

	System();

	auto begin_frame(
	    WindowHandle handle, Input input, float dt, Engine::Rect<> rect)
	    -> void;
	auto compose(std::function<void(Context &)> const &fn) -> void;
	auto end_frame(WindowHandle handle) -> WindowFrameOutput const &;

	auto request_recompose() -> void
	{
		m_structure_dirty = true;
		m_root_dirty = true;
		if (m_is_composing) {
			m_recompose_requested_during_compose = true;
		}
	}
	auto sidebar_open() const -> bool { return m_sidebar_open; }
	auto dialog_open() const -> bool { return m_dialog_open; }
	auto sidebar_visible() const -> bool
	{
		return m_sidebar_open || m_sidebar_progress > 0.01f;
	}
	auto selection_mode() const -> bool { return m_selection_mode; }
	auto set_sidebar_open(bool open) -> void;
	auto set_dialog_open(bool open) -> void;
	auto set_hud_visible(bool visible) -> void;
	auto get_hud_visible() const -> bool { return m_hud_visible; }
	auto selected(Id key) const -> bool;
	auto selected(std::string_view key) const -> bool;
	auto stats() const -> Stats const & { return m_stats; }
	auto set_icon_atlas(uint32_t image_id, IconAtlas const &atlas) -> void;
	auto set_text_measure_fn(
	    std::function<smath::Vec2(std::string_view, float)> fn) -> void;
	auto sample_animation_ref(Animation::Ref const &ref, Id owner_key) -> float;
	template<typename T> auto remember_state(Id const key, T init) -> T &
	{
		m_state_touched.insert(key);
		auto it { m_state_store.find(key) };
		if (it == m_state_store.end()) {
			it = m_state_store.emplace(key, std::move(init)).first;
		}
		auto *stored { std::any_cast<T>(&it->second) };
		if (stored == nullptr) {
			it->second = std::move(init);
			stored = std::any_cast<T>(&it->second);
		}
		return *stored;
	}
	template<typename T>
	auto set_state_if_changed(Id const key, T value) -> bool
	{
		auto &stored { remember_state<T>(key, T {}) };
		if constexpr (requires(T const &a, T const &b) {
			              { a == b } -> std::convertible_to<bool>;
		              }) {
			if (stored == value) {
				return false;
			}
		}
		stored = std::move(value);
		request_recompose();
		m_visual_dirty = true;
		return true;
	}
	template<typename T, typename Fn>
	auto update_state(Id const key, Fn &&fn) -> bool
	{
		auto before { remember_state<T>(key, T {}) };
		auto next { before };
		fn(next);
		return set_state_if_changed<T>(key, std::move(next));
	}
	auto window_rect() const -> Engine::Rect<> const & { return m_window_rect; }
	auto dump_tree_stdout(WindowHandle handle) const -> void;
	auto dump_tree_file(WindowHandle handle, std::string_view path) const
	    -> bool;
	auto dump_command_list_stdout(WindowHandle handle) const -> void;
	auto dump_command_list_file(
	    WindowHandle handle, std::string_view path) const -> bool;
	auto memo_should_recompose(Id key, uint64_t deps_hash) -> bool;
	auto memo_store(Node const &node) -> void;
	auto memo_restore(Node &node) -> bool;
	auto mark_scope_recomposed(Scope scope) -> void;

	auto theme() -> Theme & { return m_theme; }
	auto theme() const -> Theme const & { return m_theme; }

	auto reconcile_node(Node *parent,
	    Kind kind,
	    Scope scope,
	    Id key,
	    FlexOptions const &options) -> Node *;

private:
	static constexpr size_t NODE_POOL_MAX { 256 };

	struct RenderNode
	{
		Kind kind { Kind::Root };
		Engine::Rect<> rect {};
		float text_size { 14.0f };
		TextAlignX text_align_x { TextAlignX::Left };
		TextAlignY text_align_y { TextAlignY::Top };
		float corner_radius {};
		float outline_thickness { 1.0f };
		float icon_size { 24.0f };
		float scroll_x {};
		float scroll_y {};
		float padding_top {};
		float padding_right {};
		float padding_bottom {};
		float padding_left {};
		bool interactive {};
		bool selectable {};
		bool use_pressable_state {};
		bool draw_fill {};
		bool draw_outline {};
		bool draw_scrim {};
		LayerPresentation layer_presentation { LayerPresentation::Drawer };
		smath::Vec4 fill_color {};
		smath::Vec4 focus_fill_color {};
		smath::Vec4 selected_fill_color {};
		smath::Vec4 outline_color {};
		smath::Vec4 text_color {};
		smath::Vec4 selected_text_color {};
		smath::Vec4 icon_tint {};
		smath::Vec4 selected_icon_tint {};
		smath::Vec4 scrim_color {};
		uint16_t depth {};
		Id const *key {};
		std::string const *label {};
		std::string const *icon_name {};
		uint16_t first_child { INVALID_NODE_INDEX };
		uint16_t next_sibling { INVALID_NODE_INDEX };
	};

	auto active_scope() const -> Scope;
	auto collect_reconcile_nodes(std::unique_ptr<Node> node) -> void;
	auto stash_orphan(std::unique_ptr<Node> node) -> void;
	auto restore_memo_child(Node &parent, Node const &source) -> void;
	auto build_render_cache_node(Node const &source, uint16_t depth)
	    -> uint16_t;
	auto find_node_by_key(Id key) -> Node *;
	auto gather_focusables(Node &node, Scope scope, std::vector<Node *> &out)
	    -> void;
	auto active_scope_focus_key() -> Id &;
	auto active_scope_focus_key() const -> Id const &;
	auto sync_focus() -> void;
	auto focused_node() -> Node *;
	auto ensure_focus_visible(Node &node) -> void;
	auto layout_tree() -> void;
	auto layout_node(Node &node,
	    float x,
	    float y,
	    float width,
	    float height_constraint = 0.0f) -> float;
	auto update_world_tree() -> void;
	auto update_world_node(Node &node,
	    float parent_world_x,
	    float parent_world_y,
	    float parent_scroll_x,
	    float parent_scroll_y) -> void;
	auto measure_node(Node const &node, float available_width = 0.0f) const
	    -> MeasuredSize;
	auto measure_leaf(Node const &node) const -> MeasuredSize;
	auto render_node(std::vector<DrawCommand> &draw_list,
	    uint16_t node_index,
	    Engine::Rect<> const clip_rect,
	    Id focused_key,
	    bool parent_pressable_focused,
	    bool parent_pressable_selected) -> void;
	auto draw_debug_bounds(std::vector<DrawCommand> &draw_list,
	    Engine::Rect<> const rect,
	    uint16_t depth,
	    Id key,
	    Engine::Rect<> const clip_rect,
	    bool overlay) -> void;
	auto draw_debug_labels(std::vector<DrawCommand> &draw_list) -> void;
	auto tick_sidebar_animation(float dt) -> void;
	auto tick_scroll_animation(float dt) -> void;
	auto tick_node_animations(float dt, bool advance) -> void;
	auto resolve_animated_float(Animation::Ref const &ref, Id owner_key)
	    -> float;
	auto dump_tree_line(std::string &out, Node const &node, size_t indent) const
	    -> void;
	auto handle_input() -> void;
	auto tick_animation(float dt) -> void;
	auto clone_node(Node const &source, Node *parent) const
	    -> std::unique_ptr<Node>;
	auto prune_state_store() -> void;

	std::unique_ptr<Node> m_root {};
	Theme m_theme {};
	Input m_input {};
	float m_dt {};
	bool m_structure_dirty { true };
	bool m_layout_dirty { true };
	bool m_world_dirty { true };
	bool m_visual_dirty { true };
	bool m_root_dirty { true };
	bool m_sidebar_dirty { true };
	bool m_dialog_dirty { true };
	bool m_sidebar_open {};
	bool m_dialog_open {};
	bool m_selection_mode {};
	bool m_hud_visible { true };
	bool m_debug_bounds {};
	bool m_is_composing {};
	bool m_recompose_requested_during_compose {};
	WindowHandle m_current_window {};
	Engine::Rect<> m_window_rect {
		.position = smath::Vec2 { 0.0f, 0.0f },
		.size = smath::Vec2 { 0.0f, 0.0f },
	};
	float m_sidebar_progress {};
	float m_confirm_hold_elapsed {};
	bool m_confirm_hold_fired {};
	bool m_confirm_hold_started {};
	bool m_confirm_released {};
	bool m_prev_confirm_down {};
	bool m_confirm_hold_consumed {};
	std::unordered_set<Id, IdHash> m_selected {};
	Id m_pending_selectable_activation {};
	Id m_root_focus_key {};
	Id m_sidebar_focus_key {};
	Id m_dialog_focus_key {};
	float m_vertical_nav_anchor_x {};
	float m_horizontal_nav_anchor_y {};
	bool m_has_vertical_nav_anchor_x {};
	bool m_has_horizontal_nav_anchor_y {};
	Animation::Tween m_sidebar_tween {};
	struct ScrollTweenState
	{
		Animation::Tween x {};
		Animation::Tween y {};
		float target_x {};
		float target_y {};
		bool has_target_x {};
		bool has_target_y {};
	};
	std::unordered_map<Id, ScrollTweenState, IdHash> m_scroll_tweens {};
	struct TweenTrack
	{
		Animation::Tween tween {};
		Animation::TweenSpec spec {};
		std::function<bool()> pause_if {};
		uint64_t generation {};
		bool has_generation {};
		bool initialized {};
	};
	std::unordered_map<Id, TweenTrack, IdHash> m_tween_tracks {};
	std::unordered_map<Id, std::any, IdHash> m_state_store {};
	std::unordered_set<Id, IdHash> m_state_touched {};
	std::function<smath::Vec2(std::string_view, float)> m_text_measure_fn {};
	uint32_t m_icon_image_id {};
	std::unordered_map<std::string, Engine::Rect<>> m_icon_rects {};
	std::unordered_map<Id, uint64_t, IdHash> m_memo_deps {};
	std::unordered_map<Id, std::vector<std::unique_ptr<Node>>, IdHash>
	    m_memo_children {};
	std::unordered_map<Id, std::unique_ptr<Node>, IdHash> m_reconcile_nodes {};
	std::vector<std::unique_ptr<Node>> m_node_pool {};
	std::vector<RenderNode> m_render_nodes {};
	struct DebugLabelCandidate
	{
		Engine::Rect<> rect {};
		Engine::Rect<> clip_rect {};
		smath::Vec4 color {};
		Id key {};
		uint32_t order {};
		bool overlay {};
	};
	std::vector<DebugLabelCandidate> m_debug_label_candidates {};
	std::vector<Engine::Rect<>> m_debug_label_rects {};
	uint32_t m_debug_label_order {};
	uint16_t m_render_root_index { INVALID_NODE_INDEX };
	Stats m_stats {};
	WindowFrameOutput m_last_output {};
};

} // namespace Gui
