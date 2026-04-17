#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "engine/Math.h"
#include "gui/Animation.h"
#include "gui/Id.h"

namespace Gui
{

enum class Scope
{
	Root,
	Sidebar,
	Dialog,
	Hud,
};

enum class Kind
{
	Root,
	Flex,
	Scrollable,
	Text,
	Pressable,
	Surface,
	Icon,
	Layer,
	Memo,
	Spacer,
};

enum class LayerPresentation
{
	Drawer,
	Modal,
	Hud,
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
	Kind kind { Kind::Root };
	Scope scope { Scope::Root };
	Id key {};
	Id local_key {};
	std::string label {};
	std::string icon_name {};
	float text_size { 14.0f };
	TextAlignX text_align_x { TextAlignX::Left };
	TextAlignY text_align_y { TextAlignY::Top };
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
	ScrollAxis scroll_axis { ScrollAxis::Vertical };
	ScrollRevealMode scroll_reveal_mode { ScrollRevealMode::Minimal };
	float scroll_step { 24.0f };
	float scroll_x {};
	float scroll_y {};
	float scroll_target_x {};
	float scroll_target_y {};
	float content_width {};
	float content_height {};
	bool interactive {};
	bool selectable {};
	bool use_pressable_state {};
	bool draw_fill {};
	bool draw_outline {};
	bool draw_scrim {};
	float corner_radius {};
	float outline_thickness { 1.0f };
	float icon_size { 24.0f };
	float opacity { 1.0f };
	std::optional<Animation::Ref> animated_opacity {};
	LayerPresentation layer_presentation { LayerPresentation::Drawer };
	std::optional<smath::Vec4> fill_color {};
	std::optional<smath::Vec4> focus_fill_color {};
	std::optional<smath::Vec4> selected_fill_color {};
	std::optional<smath::Vec4> outline_color {};
	std::optional<smath::Vec4> text_color {};
	std::optional<smath::Vec4> selected_text_color {};
	std::optional<smath::Vec4> icon_tint {};
	std::optional<smath::Vec4> selected_icon_tint {};
	std::optional<smath::Vec4> scrim_color {};
	Engine::Rect<> local_rect {};
	Engine::Rect<> world_rect {};
	smath::Vec2 translation {};
	uint32_t recompose_count {};
	uint32_t skip_count {};
	std::function<void()> on_activate {};
	Node *parent {};
	std::vector<std::unique_ptr<Node>> children {};
};

} // namespace Gui
