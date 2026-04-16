#pragma once

#include <array>
#include <cstddef>

#include "engine/Application.h"
#include "gui/Gui.h"
#include "gui/IconAtlas.h"

namespace MaterialFM
{

class Application final : public Engine::BaseApplication
{
public:
	Application();
	~Application() override;

private:
	auto on_update(float dt) -> void override;

	Gui::System m_gui {};
	Gui::IconAtlas m_icon_atlas {};
	Engine::TextureHandle m_icon_atlas_handle {};
	bool m_has_icon_atlas {};
	bool m_icon_atlas_bound {};
	uint8_t m_gui_hud_visible { 0 };
	Engine::FontHandle m_gui_measure_font_handle {};
	Engine::FontHandle m_notosansjp_font {};
	static constexpr size_t FRAME_TIME_HISTORY_CAPACITY { 60 };
	std::array<float, FRAME_TIME_HISTORY_CAPACITY> m_frame_ms_history {};
	size_t m_frame_ms_head {};
	size_t m_frame_ms_count {};

	std::vector<std::string> m_partitions;
};

} // namespace MaterialFM
