#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "gui/Context.h"
#include "gui/Id.h"
#include "gui/Node.h"
#include "gui/Theme.h"

namespace Gui::components
{

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
	class Builder;
	static auto builder() -> Builder;
};

class ButtonStyle::Builder
{
public:
	auto copy(ButtonStyle style) -> Builder &;
	auto height(float value) -> Builder &;
	auto corner_radius(float value) -> Builder &;
	auto text_size(float value) -> Builder &;
	auto padding_x(float value) -> Builder &;
	auto padding_y(float value) -> Builder &;
	auto icon_size(float value) -> Builder &;
	auto icon_gap(float value) -> Builder &;
	auto fill(smath::Vec4 value) -> Builder &;
	auto focused_fill(smath::Vec4 value) -> Builder &;
	auto selected_fill(smath::Vec4 value) -> Builder &;
	auto text_color(smath::Vec4 value) -> Builder &;
	auto icon_tint(smath::Vec4 value) -> Builder &;
	auto selected_text_color(smath::Vec4 value) -> Builder &;
	auto selected_icon_tint(smath::Vec4 value) -> Builder &;
	auto draw_outline(bool value) -> Builder &;
	auto outline_thickness(float value) -> Builder &;
	auto outline_color(smath::Vec4 value) -> Builder &;
	auto text_align_x(TextAlignX value) -> Builder &;
	auto text_align_y(TextAlignY value) -> Builder &;
	auto build() const -> ButtonStyle;

private:
	ButtonStyle m_style {};
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

class Button
{
public:
	class Builder;

	static auto builder(Context &ctx, Id key) -> Builder;
	static auto builder(Context &ctx, std::string_view key) -> Builder;
};

class Button::Builder
{
public:
	Builder(Context &ctx, Id key);

	auto label(std::string_view value) -> Builder &;
	auto icon(std::string_view value) -> Builder &;
	auto on_activate(std::function<void()> fn) -> Builder &;
	auto selectable(bool value = true) -> Builder &;
	auto options(FlexOptions value) -> Builder &;
	auto style(ButtonStyle value) -> Builder &;
	auto build() -> void;

private:
	Context &m_ctx;
	Id m_key {};
	std::string m_label {};
	std::optional<std::string> m_icon_name {};
	std::function<void()> m_on_activate {};
	bool m_selectable {};
	FlexOptions m_options { FlexOptions::builder().build() };
	ButtonStyle m_style {};
};

class Sidebar
{
public:
	class Builder;

	static auto builder(Context &ctx, Id key) -> Builder;
	static auto builder(Context &ctx, std::string_view key) -> Builder;
};

class Sidebar::Builder
{
public:
	Builder(Context &ctx, Id key);

	auto options(FlexOptions value) -> Builder &;
	auto style(SidebarStyle value) -> Builder &;
	auto content(Context::ComposeFn fn) -> Builder &;
	auto build() -> void;

private:
	Context &m_ctx;
	Id m_key {};
	FlexOptions m_options { FlexOptions::builder().build() };
	SidebarStyle m_style {};
	Context::ComposeFn m_content {};
};

class Dialog
{
public:
	class Builder;

	static auto builder(Context &ctx, Id key) -> Builder;
	static auto builder(Context &ctx, std::string_view key) -> Builder;
};

class Dialog::Builder
{
public:
	Builder(Context &ctx, Id key);

	auto options(FlexOptions value) -> Builder &;
	auto style(DialogStyle value) -> Builder &;
	auto content(Context::ComposeFn fn) -> Builder &;
	auto build() -> void;

private:
	Context &m_ctx;
	Id m_key {};
	FlexOptions m_options { FlexOptions::builder().build() };
	DialogStyle m_style {};
	Context::ComposeFn m_content {};
};

class Toast
{
public:
	class Builder;

	Toast(System *system, std::string key);
	static auto builder(Context &ctx, Id key) -> Builder;
	static auto builder(Context &ctx, std::string_view key) -> Builder;
	auto show() const -> void;
	auto show(std::string_view message) const -> void;

private:
	System *m_system {};
	std::string m_key {};
};

class Toast::Builder
{
public:
	Builder(Context &ctx, Id key);

	auto style(ToastStyle value) -> Builder &;
	auto build() -> Toast;

private:
	Context &m_ctx;
	Id m_key {};
	ToastStyle m_style {};
};

} // namespace Gui::components
