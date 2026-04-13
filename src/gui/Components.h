#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "gui/Context.h"
#include "gui/Id.h"
#include "gui/Node.h"
#include "gui/Theme.h"

namespace Gui::components
{

struct ToastControl;

struct ButtonStyle
{
	float height { 30.0f };
	float corner_radius { 8.0f };
	float text_size { 14.0f };
	float padding_x { 8.0f };
	float padding_y { 4.0f };
	float icon_size { 24.0f };
	float icon_gap { 8.0f };
	std::optional<smath::Vec4> fill {};
	std::optional<smath::Vec4> focused_fill {};
	std::optional<smath::Vec4> selected_fill {};
	std::optional<smath::Vec4> text_color {};
	std::optional<smath::Vec4> icon_tint {};
	std::optional<smath::Vec4> selected_text_color {};
	std::optional<smath::Vec4> selected_icon_tint {};
	bool draw_outline {};
	float outline_thickness { 1.0f };
	std::optional<smath::Vec4> outline_color {};
	TextAlignX text_align_x { TextAlignX::Left };
	TextAlignY text_align_y { TextAlignY::Center };
};

struct MenuItemStyle
{
	float height { 30.0f };
	float corner_radius { 8.0f };
	float text_size { 14.0f };
	float icon_size { 24.0f };
	float icon_gap { 8.0f };
	float padding_x { 8.0f };
	float padding_y { 3.0f };
	std::optional<smath::Vec4> fill {};
	std::optional<smath::Vec4> focused_fill {};
	std::optional<smath::Vec4> selected_fill {};
	std::optional<smath::Vec4> text_color {};
	std::optional<smath::Vec4> selected_text_color {};
	std::optional<smath::Vec4> icon_tint {};
	std::optional<smath::Vec4> selected_icon_tint {};
	bool draw_outline {};
	float outline_thickness { 1.0f };
	std::optional<smath::Vec4> outline_color {};
};

struct DialogStyle
{
	float max_width_ratio { 0.80f };
	float max_height_ratio { 0.85f };
	float corner_radius { 10.0f };
	float tonal_mix { 0.12f };
	std::optional<smath::Vec4> fill {};
	std::optional<smath::Vec4> scrim {};
};

struct SidebarStyle
{
	float width { 186.0f };
	float corner_radius {};
	float tonal_mix { 0.10f };
	std::optional<smath::Vec4> fill {};
	std::optional<smath::Vec4> scrim {};
};

struct ToastStyle
{
	float width { 220.0f };
	float min_height { 40.0f };
	float corner_radius { 10.0f };
	float text_size { 15.0f };
	float padding_x { 8.0f };
	float padding_y { 4.0f };
	float bottom_margin { 18.0f };
	float fade_in_s { 0.16f };
	float hold_s { 1.6f };
	float fade_out_s { 0.22f };
	std::optional<smath::Vec4> fill {};
	std::optional<smath::Vec4> text_color {};
};

class Toast
{
public:
	explicit Toast(std::string key);
	auto show() const -> void;
	auto show(std::string_view message) const -> void;

private:
	std::shared_ptr<ToastControl> m_control {};

	explicit Toast(std::shared_ptr<ToastControl> control);
	friend auto toast(Context &ctx, Id key, ToastStyle const &style) -> Toast;
	friend auto toast(
	    Context &ctx, std::string_view key, ToastStyle const &style) -> Toast;
};

auto button(Context &ctx,
    Id key,
    std::string_view label,
    std::optional<std::string_view> const icon_name,
    std::function<void()> on_activate = {},
    bool selectable = false,
    FlexOptions const &options = FlexOptions::builder().build(),
    ButtonStyle const &style = {}) -> void;

auto button(Context &ctx,
    std::string_view key,
    std::string_view label,
    std::optional<std::string_view> const icon_name,
    std::function<void()> on_activate = {},
    bool selectable = false,
    FlexOptions const &options = FlexOptions::builder().build(),
    ButtonStyle const &style = {}) -> void;

auto sidebar(Context &ctx,
    Id key,
    FlexOptions const &options,
    Context::ComposeFn const &fn,
    SidebarStyle const &style = {}) -> void;

auto sidebar(Context &ctx,
    std::string_view key,
    FlexOptions const &options,
    Context::ComposeFn const &fn,
    SidebarStyle const &style = {}) -> void;

auto dialog(Context &ctx,
    Id key,
    FlexOptions const &options,
    Context::ComposeFn const &fn,
    DialogStyle const &style = {}) -> void;

auto dialog(Context &ctx,
    std::string_view key,
    FlexOptions const &options,
    Context::ComposeFn const &fn,
    DialogStyle const &style = {}) -> void;

auto toast(Context &ctx, Id key, ToastStyle const &style = {}) -> Toast;

auto toast(Context &ctx, std::string_view key, ToastStyle const &style = {})
    -> Toast;

} // namespace Gui::components
