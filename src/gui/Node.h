#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "engine/Math.h"
#include "gui/Animation.h"
#include "gui/Id.h"

namespace Gui
{

using AnimatedScalar = std::variant<float, Animation::Ref>;

using ScopeId = uint16_t;

enum class ScopeRole : uint8_t
{
	Overlay,
	Exclusive,
	Passive,
};

enum class Kind
{
	Root,
	OverlayHost,
	Flex,
	Scrollable,
	Text,
	Pressable,
	Surface,
	Icon,
	Layer,
	Spacer,
};

enum class LayerFocusMode
{
	Inherit,
	Overlay,
	Exclusive,
	Passive,
};

enum class LayerPlacement
{
	Flow,
	WindowLeft,
	WindowCenter,
	WindowFill,
};

enum class FlexDirection
{
	Row,
	RowReverse,
	Column,
	ColumnReverse,
};

enum class FlexWrap
{
	NoWrap,
	Wrap,
};

enum class JustifyContent
{
	Start,
	End,
	Center,
	SpaceBetween,
	SpaceAround,
	SpaceEvenly,
};

enum class AlignItems
{
	Stretch,
	Start,
	End,
	Center,
};

enum class AlignContent
{
	Start,
	End,
	Center,
	Stretch,
	SpaceBetween,
	SpaceAround,
	SpaceEvenly,
};

enum class AlignSelf
{
	Auto,
	Stretch,
	Start,
	End,
	Center,
};

enum class ScrollAxis
{
	Vertical,
	Horizontal,
	Both,
};

enum class ScrollRevealMode
{
	Minimal,
	IncludePadding,
};

enum class TextAlignX
{
	Left,
	Center,
	Right,
};

enum class TextAlignY
{
	Top,
	Center,
	Bottom,
};

struct Node
{
	struct ContentState
	{
		std::string label {};
		std::string icon_name {};
		float text_size { 14.0f };
		TextAlignX text_align_x { TextAlignX::Left };
		TextAlignY text_align_y { TextAlignY::Top };
		float icon_size { 24.0f };
	};

	struct LayoutState
	{
		float padding_top {};
		float padding_right {};
		float padding_bottom {};
		float padding_left {};
		float gap {};
		float row_gap {};
		float column_gap {};
		float fixed_width {};
		float fixed_height {};
		std::optional<Animation::Ref> animated_width {};
		std::optional<Animation::Ref> animated_height {};
		float min_width {};
		float min_height {};
		float max_width {};
		float max_height {};
		float flex_grow {};
		float flex_shrink { 1.0f };
		float flex_basis { -1.0f };
		AlignSelf align_self { AlignSelf::Auto };
		FlexDirection flex_direction { FlexDirection::Column };
		FlexWrap flex_wrap { FlexWrap::NoWrap };
		JustifyContent justify_content { JustifyContent::Start };
		AlignItems align_items { AlignItems::Stretch };
		AlignContent align_content { AlignContent::Start };
		Engine::Rect<> local_rect {};
		Engine::Rect<> world_rect {};
		smath::Vec2 translation {};
	};

	struct ScrollState
	{
		ScrollAxis scroll_axis { ScrollAxis::Vertical };
		ScrollRevealMode scroll_reveal_mode { ScrollRevealMode::Minimal };
		float scroll_step { 24.0f };
		float scroll_x {};
		float scroll_y {};
		float scroll_target_x {};
		float scroll_target_y {};
		float content_width {};
		float content_height {};
	};

	struct InteractionState
	{
		bool interactive {};
		bool selectable {};
		bool use_pressable_state {};
		std::function<void()> on_activate {};
	};

	struct VisualState
	{
		bool draw_fill {};
		bool draw_outline {};
		bool draw_scrim {};
		float corner_radius {};
		float outline_thickness { 1.0f };
		float scrim_opacity { 1.0f };
		std::optional<Animation::Ref> animated_scrim_opacity {};
		float opacity { 1.0f };
		std::optional<Animation::Ref> animated_opacity {};
		LayerFocusMode layer_focus_mode { LayerFocusMode::Overlay };
		std::optional<AnimatedScalar> top {};
		std::optional<AnimatedScalar> right {};
		std::optional<AnimatedScalar> bottom {};
		std::optional<AnimatedScalar> left {};
		std::optional<smath::Vec4> fill_color {};
		std::optional<smath::Vec4> focus_fill_color {};
		std::optional<smath::Vec4> selected_fill_color {};
		std::optional<smath::Vec4> outline_color {};
		std::optional<smath::Vec4> text_color {};
		std::optional<smath::Vec4> selected_text_color {};
		std::optional<smath::Vec4> icon_tint {};
		std::optional<smath::Vec4> selected_icon_tint {};
		std::optional<smath::Vec4> scrim_color {};
	};

	Kind kind { Kind::Root };
	ScopeId scope {};
	Id key {};
	Id local_key {};
	ContentState content {};
	LayoutState layout {};
	ScrollState scroll {};
	InteractionState interaction {};
	VisualState visual {};
	uint32_t recompose_count {};
	uint32_t skip_count {};
	Node *parent {};
	std::vector<std::unique_ptr<Node>> children {};

	Node() = default;

	Node(Node const &) = delete;
	auto operator=(Node const &) -> Node & = delete;
	Node(Node &&) = delete;
	auto operator=(Node &&) -> Node & = delete;
};

} // namespace Gui
