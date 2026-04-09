#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "gui/Context.h"
#include "gui/Node.h"

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
	smath::Vec4 fill { 0.93f, 0.94f, 0.97f, 1.0f };
	smath::Vec4 focused_fill { 0.80f, 0.86f, 0.98f, 1.0f };
	smath::Vec4 selected_fill { 0.12f, 0.31f, 0.85f, 1.0f };
	smath::Vec4 text_color { 0.08f, 0.10f, 0.12f, 1.0f };
	smath::Vec4 icon_tint { 0.08f, 0.10f, 0.12f, 1.0f };
	smath::Vec4 selected_text_color { 1.0f, 1.0f, 1.0f, 1.0f };
	smath::Vec4 selected_icon_tint { 1.0f, 1.0f, 1.0f, 1.0f };
	bool draw_outline {};
	float outline_thickness { 1.0f };
	smath::Vec4 outline_color { 0.62f, 0.66f, 0.72f, 1.0f };
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
	smath::Vec4 fill { 0.94f, 0.95f, 0.98f, 1.0f };
	smath::Vec4 focused_fill { 0.83f, 0.88f, 0.98f, 1.0f };
	smath::Vec4 selected_fill { 0.12f, 0.31f, 0.85f, 1.0f };
	smath::Vec4 text_color { 0.08f, 0.10f, 0.12f, 1.0f };
	smath::Vec4 selected_text_color { 1.0f, 1.0f, 1.0f, 1.0f };
	smath::Vec4 icon_tint { 0.08f, 0.10f, 0.12f, 1.0f };
	smath::Vec4 selected_icon_tint { 1.0f, 1.0f, 1.0f, 1.0f };
	bool draw_outline {};
	float outline_thickness { 1.0f };
	smath::Vec4 outline_color { 0.62f, 0.66f, 0.72f, 1.0f };
};

struct DialogStyle
{
	float max_width_ratio { 0.80f };
	float max_height_ratio { 0.85f };
	float corner_radius { 10.0f };
	float tonal_mix { 0.12f };
	smath::Vec4 scrim { 0.02f, 0.04f, 0.08f, 0.55f };
};

struct SidebarStyle
{
	float width { 186.0f };
	float corner_radius {};
	float tonal_mix { 0.10f };
	smath::Vec4 scrim { 0.02f, 0.04f, 0.08f, 0.55f };
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
	smath::Vec4 fill { 0.08f, 0.10f, 0.14f, 0.92f };
	smath::Vec4 text_color { 1.0f, 1.0f, 1.0f, 1.0f };
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
	friend auto toast(
	    Context &ctx, std::string_view key, ToastStyle const &style) -> Toast;
};

auto button(Context &ctx,
    std::string_view key,
    std::string_view label,
    std::optional<std::string_view> const icon_name,
    std::function<void()> on_activate = {},
    bool selectable = false,
    FlexOptions const &options = FlexOptions::builder().build(),
    ButtonStyle const &style = {}) -> void;

auto sidebar(Context &ctx,
    std::string_view key,
    FlexOptions const &options,
    Context::ComposeFn const &fn,
    SidebarStyle const &style = {}) -> void;

auto dialog(Context &ctx,
    std::string_view key,
    FlexOptions const &options,
    Context::ComposeFn const &fn,
    DialogStyle const &style = {}) -> void;

auto toast(Context &ctx, std::string_view key, ToastStyle const &style = {})
    -> Toast;

} // namespace Gui::components
