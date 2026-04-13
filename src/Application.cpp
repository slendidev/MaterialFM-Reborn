#include "Application.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <variant>

#include "engine/Common.h"
#include "gui/Components.h"
#include "gui/Node.h"

Application::Application()
{
	auto const sfx_result {
		assets().load_sound_from_file("ahh", "assets/ahh.ogg"),
	};
	sassert(sfx_result == Engine::AssetError::Ok, "Failed to load SFX asset");

	auto const song_result {
		// assets().load_song_from_file("ticktock", "assets/ticktock.mp3"),
		assets().load_song_from_file("ticktock", "assets/ticktock.ogg"),
	};
	sassert(song_result == Engine::AssetError::Ok, "Failed to load song asset");

	auto const font_result {
		assets().load_font_from_file(
		    "ubuntu", "assets/Fonts/Ubuntu-Regular.ttf"),
	};
	sassert(font_result == Engine::AssetError::Ok, "Failed to load font asset");
	sassert(assets().set_active_font(assets().font_handle("ubuntu"))
	        == Engine::AssetError::Ok,
	    "Failed to set active font");
	m_gui_measure_font_handle = assets().active_font_handle();
	m_gui.set_text_measure_fn([this](std::string_view const text,
	                              float const size) {
		return renderer().measure_text(text, size, m_gui_measure_font_handle);
	});

	auto const atlas_meta {
		Gui::load_icon_atlas("assets/Textures/atlas.txt"),
	};
	if (atlas_meta.has_value()) {
		auto const atlas_tex_result {
			assets().load_texture_from_file(
			    "gui_icons_atlas", atlas_meta->texture_path),
		};
		if (atlas_tex_result == Engine::AssetError::Ok) {
			m_icon_atlas = atlas_meta.value();
			m_icon_atlas_handle = assets().texture_handle("gui_icons_atlas");
			m_has_icon_atlas = true;
		}
	}
}

namespace
{
constexpr Gui::WindowHandle MAIN_WINDOW { 1u };
constexpr uint32_t GUI_ICON_IMAGE_ID { 1u };
constexpr char GUI_TREE_DUMP_PATH[] { "gui_tree_dump.txt" };
constexpr char GUI_COMMAND_DUMP_PATH[] { "gui_command_dump.txt" };
} // namespace

Application::~Application()
{
	assets().shutdown_audio();
}

auto Application::on_update(float const dt) -> void
{
	static auto const sfx_handle { assets().sound_handle("ahh") };
	static auto const song_handle { assets().song_handle("ticktock") };
	static auto fps_smooth { 60.0f };
	static auto fps_window_value { 60.0f };
	static float fps_window_elapsed {};
	static int fps_window_frames {};

	constexpr float FPS_WINDOW_SECONDS { 0.25f };
	constexpr float FPS_SMOOTHING { 0.25f };

	fps_window_elapsed += dt;
	fps_window_frames += 1;
	if (fps_window_elapsed >= FPS_WINDOW_SECONDS) {
		fps_window_value
		    = static_cast<float>(fps_window_frames) / fps_window_elapsed;
		fps_window_elapsed = 0.0f;
		fps_window_frames = 0;
	}
	fps_smooth += (fps_window_value - fps_smooth) * FPS_SMOOTHING;
	auto const frame_ms { std::max(0.0f, dt * 1000.0f) };
	m_frame_ms_history[m_frame_ms_head] = frame_ms;
	m_frame_ms_head = (m_frame_ms_head + 1) % FRAME_TIME_HISTORY_CAPACITY;
	m_frame_ms_count = std::min(
	    m_frame_ms_count + 1, static_cast<size_t>(FRAME_TIME_HISTORY_CAPACITY));

	assets().update_audio();
	if (is_pressed(Engine::Button::Select)) {
		m_gui_hud_visible = !m_gui_hud_visible;
	}
	m_gui.set_hud_visible(m_gui_hud_visible);
	if (m_has_icon_atlas && !m_icon_atlas_bound) {
		auto const *atlas_tex { assets().texture(m_icon_atlas_handle) };
		if (atlas_tex != nullptr) {
			m_gui.set_icon_atlas(GUI_ICON_IMAGE_ID, m_icon_atlas);
			m_icon_atlas_bound = true;
		}
	}

	if (is_pressed(Engine::Button::Square)) {
		request_exit();
	}

	m_gui.begin_frame(MAIN_WINDOW,
	    Gui::Input {
	        .up_pressed = is_pressed(Engine::Button::Up),
	        .down_pressed = is_pressed(Engine::Button::Down),
	        .left_pressed = is_pressed(Engine::Button::Left),
	        .right_pressed = is_pressed(Engine::Button::Right),
	        .confirm_pressed = is_pressed(Engine::Button::Cross),
	        .confirm_down = is_down(Engine::Button::Cross),
	        .back_pressed = is_pressed(Engine::Button::Circle),
	        .menu_pressed = is_pressed(Engine::Button::Start),
	        .actions_pressed = is_pressed(Engine::Button::Triangle),
	        .debug_bounds_toggle_pressed
	        = is_pressed(Engine::Button::LeftTrigger),
	        .stick_x = stick().x(),
	        .stick_y = stick().y(),
	    },
	    dt,
	    Engine::Rect<> {
	        .position = smath::Vec2 { 0.0f, 0.0f },
	        .size = smath::Vec2 { 480.0f, 272.0f },
	    });

	m_gui.compose([&](Gui::Context &ui) {
		auto toast { Gui::components::toast(ui, Gui::id("status_toast")) };
		ui.flex(Gui::id("root"),
		    Gui::FlexOptions::builder()
		        .column()
		        .padding(std::array<float, 4> { 10.0f, 10.0f, 0.0f, 10.0f })
		        .gap(8.0f)
		        .build(),
		    [&](Gui::Context &ctx) {
			    ctx.text(Gui::id("title"),
			        "MaterialFM",
			        Gui::TextStyle::builder()
			            .size(20.0f)
			            .color(m_gui.theme().on_surface)
			            .selected_color(m_gui.theme().on_primary)
			            .build());
			    ctx.text(Gui::id("hint"),
			        "Hello there!",
			        Gui::TextStyle::builder()
			            .size(12.0f)
			            .color(m_gui.theme().on_surface_variant)
			            .selected_color(m_gui.theme().on_primary)
			            .build());
			    ctx.memo(Gui::id("library_memo"),
			        0xA11CEu,
			        [&](Gui::Context &memo_ctx) {
				        memo_ctx.scrollable(Gui::id("library"),
				            Gui::ScrollOptions::builder()
				                .height(138.0f)
				                .padding(4.0f)
				                .build(),
				            [&](Gui::Context &scroll) {
					            scroll.flex(Gui::id("library_list"),
					                Gui::FlexOptions::builder()
					                    .column()
					                    .gap(4.0f)
					                    .build(),
					                [&](Gui::Context &list) {
						                Gui::components::button(
						                    list,
						                    Gui::id("track_1"),
						                    "Track 1",
						                    std::nullopt,
						                    [&]() {
							                    assets().play_song(song_handle);
						                    },
						                    true);
						                for (int i = 2; i < 10; i++) {
							                Gui::components::button(
							                    list,
							                    std::format("track_{}", i),
							                    std::format("Track {}", i),
							                    std::nullopt,
							                    [&]() {
								                    assets().play_sound(
								                        sfx_handle);
							                    },
							                    true);
						                }
					                });
				            });
			        });
		    });

		Gui::components::sidebar(ui,
		    Gui::id("drawer"),
		    Gui::FlexOptions::builder()
		        .column()
		        .padding(10.0f)
		        .gap(8.0f)
		        .build(),
		    [&](Gui::Context &drawer) {
			    // auto const box_pulse {
			    //         Gui::Animation::Definition::builder("red_box_pulse")
			    //             .from(20.0f)
			    //             .to(40.0f)
			    //             .duration(0.72f)
			    //             .easing(Gui::Animation::Easing::EaseInOutSine)
			    //             .repeat(Gui::Animation::RepeatMode::PingPong)
			    //             .pause_if(drawer.visibility_pause_condition())
			    //             .build(),
			    // };

			    drawer.text(Gui::id("drawer_title"),
			        "Navigation",
			        Gui::TextStyle::builder()
			            .size(17.0f)
			            .color(m_gui.theme().on_surface)
			            .selected_color(m_gui.theme().on_primary)
			            .build());
			    Gui::components::button(
			        drawer, Gui::id("drawer_home"), "Home", "menu", [toast]() {
				        toast.show("Home unimplemented");
			        });
			    Gui::components::button(drawer,
			        Gui::id("drawer_settings"),
			        "Settings",
			        "settings",
			        [toast]() { toast.show("Settings unimplemented"); });
			    Gui::components::button(drawer,
			        Gui::id("drawer_stop"),
			        "Stop Song",
			        "archive",
			        [&, toast]() {
				        assets().stop_song();
				        toast.show("Song stopped");
			        });

			    auto counter { drawer.mutable_state_of<int>("counter", 0) };
			    drawer.text(drawer.new_id(),
			        std::format("Count: {}", counter.get()),
			        Gui::TextStyle::builder()
			            .size(14.0f)
			            .color(m_gui.theme().on_surface)
			            .selected_color(m_gui.theme().on_primary)
			            .build());
			    drawer.flex(Gui::id("counter_controls"),
			        Gui::FlexOptions::builder().row().gap(6.0f).build(),
			        [&](Gui::Context &controls) {
				        Gui::components::button(
				            controls,
				            controls.new_id(),
				            "-",
				            std::nullopt,
				            [counter]() {
					            counter.update([](int &value) { value -= 1; });
				            },
				            false,
				            Gui::FlexOptions::builder().flex(1.0f).build(),
				            Gui::components::ButtonStyle {
				                .text_align_x = Gui::TextAlignX::Center,
				            });
				        Gui::components::button(
				            controls,
				            controls.new_id(),
				            "+",
				            std::nullopt,
				            [counter]() {
					            counter.update([](int &value) { value += 1; });
				            },
				            false,
				            Gui::FlexOptions::builder().flex(1.0f).build(),
				            Gui::components::ButtonStyle {
				                .text_align_x = Gui::TextAlignX::Center,
				            });
			        });

			    // drawer.surface(Gui::id("animated_surface"),
			    //     Gui::FlexOptions::builder()
			    //         .width(box_pulse.get_ref())
			    //         .height(box_pulse.get_ref())
			    //         .align_self(Gui::AlignSelf::Start)
			    //         .build(),
			    //     Gui::SurfaceStyle::builder()
			    //         .fill_color(Engine::Color::RED)
			    //         .build(),
			    //     [&](Gui::Context &) { });
		    });

		Gui::components::dialog(ui,
		    Gui::id("actions_dialog"),
		    Gui::FlexOptions::builder()
		        .column()
		        .padding(12.0f)
		        .gap(8.0f)
		        .min_width(200.0f)
		        .build(),
		    [&](Gui::Context &dialog) {
			    dialog.text(Gui::id("dialog_title"),
			        "Actions",
			        Gui::TextStyle::builder()
			            .size(17.0f)
			            .color(m_gui.theme().on_surface)
			            .selected_color(m_gui.theme().on_primary)
			            .build());
			    Gui::components::button(dialog,
			        Gui::id("dialog_play"),
			        "Play Song",
			        std::nullopt,
			        [&]() { assets().play_song(song_handle); });
			    Gui::components::button(dialog,
			        Gui::id("dialog_pause"),
			        "Pause Song",
			        std::nullopt,
			        [&]() { assets().pause_song(); });
			    Gui::components::button(dialog,
			        Gui::id("dialog_close"),
			        "Close",
			        std::nullopt,
			        [&]() { m_gui.set_dialog_open(false); });
		    });
	});

	renderer().start_frame();
	defer(renderer().end_frame());

	renderer().clear_background();

	renderer().mode_2d();
	auto const &frame { m_gui.end_frame(MAIN_WINDOW) };
	if (is_pressed(Engine::Button::RightTrigger)) {
		m_gui.dump_tree_file(MAIN_WINDOW, GUI_TREE_DUMP_PATH);
		m_gui.dump_command_list_file(MAIN_WINDOW, GUI_COMMAND_DUMP_PATH);
	}
	for (auto const &command : frame.draw_list) {
		std::visit(
		    [&](auto const &payload) {
			    using T = std::decay_t<decltype(payload)>;
			    if constexpr (std::is_same_v<T, Gui::DrawCommand::PushClip>) {
				    renderer().push_clip_rect(payload.rect);
			    } else if constexpr (std::is_same_v<T,
			                             Gui::DrawCommand::PopClip>) {
				    renderer().pop_clip_rect();
			    } else if constexpr (std::is_same_v<T,
			                             Gui::DrawCommand::Rect>) {
				    renderer().draw_rectangle(payload.rect.position,
				        payload.rect.size,
				        payload.color);
			    } else if constexpr (std::is_same_v<T,
			                             Gui::DrawCommand::Line>) {
				    renderer().draw_line(payload.start,
				        payload.end,
				        payload.thickness,
				        payload.color);
			    } else if constexpr (std::is_same_v<T,
			                             Gui::DrawCommand::CircleSector>) {
				    renderer().draw_circle_sector(payload.center,
				        payload.radius,
				        payload.start_radians,
				        payload.end_radians,
				        payload.color,
				        payload.segments);
			    } else if constexpr (std::is_same_v<T,
			                             Gui::DrawCommand::Text>) {
				    auto const align_x { payload.align_x
					            == Gui::TextAlignX::Center
					        ? Engine::TextAlignX::Center
					        : (payload.align_x == Gui::TextAlignX::Right
					                  ? Engine::TextAlignX::Right
					                  : Engine::TextAlignX::Left) };
				    auto const align_y { payload.align_y
					            == Gui::TextAlignY::Center
					        ? Engine::TextAlignY::Center
					        : (payload.align_y == Gui::TextAlignY::Bottom
					                  ? Engine::TextAlignY::Bottom
					                  : Engine::TextAlignY::Top) };
				    renderer().draw_text(payload.value,
				        payload.box,
				        payload.size,
				        payload.color,
				        align_x,
				        align_y);
			    } else if constexpr (std::is_same_v<T,
			                             Gui::DrawCommand::Image>) {
				    if (payload.image_id == GUI_ICON_IMAGE_ID
				        && m_has_icon_atlas) {
					    auto const *tex {
						    assets().texture(m_icon_atlas_handle),
					    };
					    if (tex != nullptr) {
						    renderer().draw_texture_ex(
						        *tex, payload.src, payload.dst, payload.color);
					    }
				    }
			    }
		    },
		    command.payload);
	}

	if (m_gui.get_hud_visible()) {
		char fps_label[32] {};
		std::snprintf(fps_label,
		    sizeof(fps_label),
		    "fps: %.1f",
		    static_cast<double>(fps_smooth));
		renderer().draw_text(fps_label,
		    Engine::Rect<> {
		        .position = smath::Vec2 { 6.0f, 2.0f },
		        .size = smath::Vec2 { 200.0f, 20.0f },
		    },
		    16.0f,
		    Engine::Color::GREEN);

		auto const render_stats { Engine::Platform::renderer_stats() };
		char render_label[128] {};
		std::snprintf(render_label,
		    sizeof(render_label),
		    "b:%lu t:%lu s:%lu bind:%lu up:%lu ub:%luKB",
		    static_cast<unsigned long>(render_stats.batch_submits),
		    static_cast<unsigned long>(render_stats.textured_submits),
		    static_cast<unsigned long>(render_stats.solid_submits),
		    static_cast<unsigned long>(render_stats.texture_binds),
		    static_cast<unsigned long>(render_stats.texture_uploads),
		    static_cast<unsigned long>(
		        render_stats.texture_upload_bytes / 1024u));
		renderer().draw_text(render_label,
		    Engine::Rect<> {
		        .position = smath::Vec2 { 6.0f, 20.0f },
		        .size = smath::Vec2 { 320.0f, 20.0f },
		    },
		    16.0f,
		    Engine::Color::GREEN);

		constexpr float HISTOGRAM_X { 332.0f };
		constexpr float HISTOGRAM_Y { 4.0f };
		constexpr float HISTOGRAM_W { 144.0f };
		constexpr float HISTOGRAM_H { 52.0f };
		constexpr float PLOT_X { HISTOGRAM_X + 2.0f };
		constexpr float PLOT_Y { HISTOGRAM_Y + 2.0f };
		constexpr float PLOT_W { HISTOGRAM_W - 4.0f };
		constexpr float PLOT_H { HISTOGRAM_H - 4.0f };
		constexpr float FRAME_MS_SCALE_MAX { 50.0f };
		constexpr float FRAME_MS_60FPS { 16.67f };
		constexpr float FRAME_MS_30FPS { 33.33f };
		constexpr smath::Vec4 HISTOGRAM_BORDER { 0.0f, 0.0f, 0.0f, 0.85f };

		renderer().draw_rectangle(smath::Vec2 { HISTOGRAM_X, HISTOGRAM_Y },
		    smath::Vec2 { HISTOGRAM_W, HISTOGRAM_H },
		    smath::Vec4 { 0.0f, 0.0f, 0.0f, 0.45f });
		renderer().draw_rectangle(smath::Vec2 { HISTOGRAM_X, HISTOGRAM_Y },
		    smath::Vec2 { HISTOGRAM_W, 1.0f },
		    HISTOGRAM_BORDER);
		renderer().draw_rectangle(
		    smath::Vec2 { HISTOGRAM_X, HISTOGRAM_Y + HISTOGRAM_H - 1.0f },
		    smath::Vec2 { HISTOGRAM_W, 1.0f },
		    HISTOGRAM_BORDER);
		renderer().draw_rectangle(smath::Vec2 { HISTOGRAM_X, HISTOGRAM_Y },
		    smath::Vec2 { 1.0f, HISTOGRAM_H },
		    HISTOGRAM_BORDER);
		renderer().draw_rectangle(
		    smath::Vec2 { HISTOGRAM_X + HISTOGRAM_W - 1.0f, HISTOGRAM_Y },
		    smath::Vec2 { 1.0f, HISTOGRAM_H },
		    HISTOGRAM_BORDER);

		auto const line_y_for_ms {
			[&](float const ms) {
			    auto const normalized {
				    std::clamp(ms / FRAME_MS_SCALE_MAX, 0.0f, 1.0f),
			    };
			    return PLOT_Y + PLOT_H - 1.0f - normalized * (PLOT_H - 1.0f);
			},
		};
		renderer().draw_rectangle(
		    smath::Vec2 { PLOT_X, line_y_for_ms(FRAME_MS_60FPS) },
		    smath::Vec2 { PLOT_W, 1.0f },
		    smath::Vec4 { 0.0f, 1.0f, 0.0f, 0.45f });
		renderer().draw_rectangle(
		    smath::Vec2 { PLOT_X, line_y_for_ms(FRAME_MS_30FPS) },
		    smath::Vec2 { PLOT_W, 1.0f },
		    smath::Vec4 { 1.0f, 0.0f, 0.0f, 0.45f });

		float sum_ms {};
		float max_ms {};
		for (size_t i {}; i < m_frame_ms_count; ++i) {
			auto const sample { m_frame_ms_history[i] };
			sum_ms += sample;
			max_ms = std::max(max_ms, sample);
		}
		auto const avg_ms {
			m_frame_ms_count > 0
			    ? (sum_ms / static_cast<float>(m_frame_ms_count))
			    : 0.0f,
		};
		auto const current_index {
			(m_frame_ms_head + FRAME_TIME_HISTORY_CAPACITY - 1)
			    % FRAME_TIME_HISTORY_CAPACITY,
		};
		auto const current_ms {
			m_frame_ms_count > 0 ? m_frame_ms_history[current_index] : 0.0f,
		};

		if (m_frame_ms_count > 0) {
			auto const oldest_index {
				(m_frame_ms_head + FRAME_TIME_HISTORY_CAPACITY
				    - m_frame_ms_count)
				    % FRAME_TIME_HISTORY_CAPACITY,
			};
			auto const plot_columns {
				std::max(1, static_cast<int>(PLOT_W)),
			};
			auto const column_den {
				std::max(1, plot_columns - 1),
			};
			auto const sample_den {
				std::max<size_t>(1, m_frame_ms_count - 1),
			};

			for (int x_pixel {}; x_pixel < plot_columns; ++x_pixel) {
				auto const sample_pos {
					(static_cast<size_t>(x_pixel) * sample_den)
					    / static_cast<size_t>(column_den),
				};
				auto const sample_index {
					(oldest_index + sample_pos) % FRAME_TIME_HISTORY_CAPACITY,
				};
				auto const sample_ms { m_frame_ms_history[sample_index] };
				auto const normalized {
					std::clamp(sample_ms / FRAME_MS_SCALE_MAX, 0.0f, 1.0f),
				};
				auto const bar_h {
					std::max(1.0f, normalized * (PLOT_H - 1.0f)),
				};
				auto const x {
					PLOT_X + static_cast<float>(x_pixel),
				};
				auto const y { PLOT_Y + PLOT_H - bar_h };

				smath::Vec4 color { 1.0f, 0.15f, 0.15f, 1.0f };
				if (sample_ms <= FRAME_MS_60FPS) {
					color = smath::Vec4 { 0.0f, 1.0f, 0.0f, 1.0f };
				} else if (sample_ms <= FRAME_MS_30FPS) {
					color = smath::Vec4 { 1.0f, 0.85f, 0.1f, 1.0f };
				}

				renderer().draw_rectangle(
				    smath::Vec2 { x, y }, smath::Vec2 { 1.0f, bar_h }, color);
			}
		}

		char frame_ms_label[96] {};
		std::snprintf(frame_ms_label,
		    sizeof(frame_ms_label),
		    "dt:%.1f avg:%.1f max:%.1f",
		    static_cast<double>(current_ms),
		    static_cast<double>(avg_ms),
		    static_cast<double>(max_ms));
		renderer().draw_text(frame_ms_label,
		    Engine::Rect<> {
		        .position = smath::Vec2 {
		            HISTOGRAM_X,
		            HISTOGRAM_Y + HISTOGRAM_H + 2.0f,
		        },
		        .size = smath::Vec2 { 200.0f, 14.0f },
		    },
		    12.0f,
		    Engine::Color::BLACK);
	}
}
