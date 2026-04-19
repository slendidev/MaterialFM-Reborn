#include "Application.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <variant>

#include "IO.h"
#include "engine/Common.h"
#include "gui/Components.h"
#include "gui/Id.h"
#include "gui/Node.h"

namespace MaterialFM
{

namespace
{

auto icon_for_partition(std::string_view const partition) -> std::string_view
{
	if (partition.starts_with("flash")) {
		return "mem";
	} else if (partition.starts_with("umd") || partition.starts_with("disc")) {
		return "album";
	}
	return "sd";
}

} // namespace

Application::Application()
{
	auto font_result {
		assets().load_font_from_file(
		    "ubuntu", "assets/Fonts/Ubuntu-Regular.ttf"),
	};
	sassert(font_result == Engine::AssetError::Ok, "Failed to load font asset");

	font_result = assets().load_font_from_file(
	    "notosansjp", "assets/Fonts/NotoSansJP-Regular.ttf", m_notosansjp_font),
	sassert(font_result == Engine::AssetError::Ok, "Failed to load font asset");

	sassert(assets().set_active_font(assets().font_handle("ubuntu"))
	        == Engine::AssetError::Ok,
	    "Failed to set active font");

	m_gui_measure_font_handle = assets().active_font_handle();
	m_gui.set_text_measure_fn([this](std::string_view const text,
	                              float const size) {
		return renderer().measure_text(text, size, m_gui_measure_font_handle);
	});
	Gui::components::register_default_scope_roles(m_gui);

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
			if (auto *atlas_texture {
			        assets().texture_mut(m_icon_atlas_handle) }) {
				atlas_texture->can_be_solid_source = true;
			}
			m_has_icon_atlas = true;
		}
	}

	m_partitions = find_available_partitions();
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
		m_gui_hud_visible += 1;
		m_gui_hud_visible %= 3;
	}
	m_gui.set_hud_visible(m_gui_hud_visible == 2);
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
	if (is_pressed(Engine::Button::Start) && !m_dialog_open) {
		m_sidebar_open = !m_sidebar_open;
		m_gui.invalidate_compose();
	}
	if (is_pressed(Engine::Button::Triangle) && !m_dialog_open
	    && !m_sidebar_open) {
		m_dialog_open = true;
		m_gui.invalidate_compose();
	}
	if (is_pressed(Engine::Button::Circle)) {
		if (m_dialog_open) {
			m_dialog_open = false;
			m_gui.invalidate_compose();
		} else if (m_sidebar_open) {
			m_sidebar_open = false;
			m_gui.invalidate_compose();
		}
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
	        .menu_pressed = false,
	        .actions_pressed = false,
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
		std::optional<Gui::components::Toast> toast {};
		auto const window_rect { ui.window_rect() };
		ui.overlay_host(ui.id("notifications_host"),
		    Gui::FlexOptions::builder()
		        .width(window_rect.size.x())
		        .height(window_rect.size.y())
		        .build(),
		    Gui::OverlayHostSpec::builder().build(),
		    [&](Gui::Context &overlay) {
			    toast = Gui::components::Toast::builder(overlay, "status_toast")
			                .build();
		    });
		ui.flex(ui.id("root"),
		    Gui::FlexOptions::builder()
		        .column()
		        .padding(std::array<float, 4> { 10.0f, 10.0f, 0.0f, 10.0f })
		        .gap(8.0f)
		        .flex(1.0f)
		        .build(),
		    [&](Gui::Context &ctx) {
			    ctx.text(ctx.id("title"),
			        "MaterialFM",
			        Gui::TextStyle::builder()
			            .size(20.0f)
			            .color(m_gui.theme().on_surface)
			            .selected_color(m_gui.theme().on_primary)
			            .build());
			    ctx.text(ctx.id("hint"),
			        "Hello there!",
			        Gui::TextStyle::builder()
			            .size(12.0f)
			            .color(m_gui.theme().on_surface_variant)
			            .selected_color(m_gui.theme().on_primary)
			            .build());
			    ctx.scrollable(ctx.id("library"),
			        Gui::ScrollOptions::builder()
			            .reveal_mode(Gui::ScrollRevealMode::IncludePadding)
			            .build(),
			        [&](Gui::Context &scroll) {
				        scroll.flex(scroll.id("library_list"),
				            Gui::FlexOptions::builder()
				                .column()
				                .padding(std::array<float, 4> {
				                    0.0f, 0.0f, 10.0f, 0.0f })
				                .gap(4.0f)
				                .build(),
				            [&](Gui::Context &list) {
					            Gui::components::Button::builder(
					                list, "track_1")
					                .label("Track 1")
					                .on_activate([&]() { })
					                .selectable(true)
					                .build();
					            for (int i = 2; i < 10; i++) {
						            Gui::components::Button::builder(
						                list, std::format("track_{}", i))
						                .label(std::format("Track {}", i))
						                .on_activate([&]() { })
						                .selectable(true)
						                .build();
					            }
				            });
			        });
		    });

		ui.overlay_host(ui.id("overlay_host"),
		    Gui::FlexOptions::builder()
		        .width(window_rect.size.x())
		        .height(window_rect.size.y())
		        .build(),
		    Gui::OverlayHostSpec::builder().build(),
		    [&](Gui::Context &overlay) {
			    Gui::components::Sidebar::builder(overlay, "drawer")
			        .open(m_sidebar_open)
			        .options(Gui::FlexOptions::builder()
			                .column()
			                .padding(10.0f)
			                .gap(8.0f)
			                .build())
			        .content([&](Gui::Context &drawer) {
				        for (auto const &part : m_partitions) {
					        Gui::components::Button::builder(
					            drawer, std::format("btn_{}", part))
					            .label(part)
					            .icon(icon_for_partition(part))
					            .build();
				        }

				        Gui::components::Button::builder(
				            drawer, "drawer_settings")
				            .label("Settings")
				            .icon("settings")
				            .on_activate([toast]() {
					            if (toast.has_value()) {
						            toast->show("Settings unimplemented");
					            }
				            })
				            .build();

				        drawer.flex(drawer.id("counter_controls"),
				            Gui::FlexOptions::builder().row().gap(6.0f).build(),
				            [&](Gui::Context &controls) {
					            auto counter {
						            drawer.mutable_state_of<int>("counter", 0),
					            };

					            drawer.text(drawer.id("counter_value"),
					                std::format("Count: {}", counter.get()),
					                Gui::TextStyle::builder()
					                    .size(14.0f)
					                    .color(m_gui.theme().on_surface)
					                    .selected_color(
					                        m_gui.theme().on_primary)
					                    .align_y(Gui::TextAlignY::Center)
					                    .build());

					            Gui::components::Button::builder(
					                controls, controls.id("counter_decrement"))
					                .label("-")
					                .on_activate([counter]() {
						                counter.update(
						                    [](int &value) { value -= 1; });
					                })
					                .options(Gui::FlexOptions::builder()
					                        .flex(1.0f)
					                        .build())
					                .style(
					                    Gui::components::ButtonStyle::builder()
					                        .text_align_x(
					                            Gui::TextAlignX::Center)
					                        .build())
					                .build();

					            Gui::components::Button::builder(
					                controls, controls.id("counter_increment"))
					                .label("+")
					                .on_activate([counter]() {
						                counter.update(
						                    [](int &value) { value += 1; });
					                })
					                .options(Gui::FlexOptions::builder()
					                        .flex(1.0f)
					                        .build())
					                .style(
					                    Gui::components::ButtonStyle::builder()
					                        .text_align_x(
					                            Gui::TextAlignX::Center)
					                        .build())
					                .build();
				            });
			        })
			        .build();

			    Gui::components::Dialog::builder(overlay, "actions_dialog")
			        .open(m_dialog_open)
			        .options(Gui::FlexOptions::builder()
			                .column()
			                .padding(12.0f)
			                .gap(8.0f)
			                .min_width(200.0f)
			                .build())
			        .content([&](Gui::Context &dialog) {
				        dialog.text(dialog.id("dialog_title"),
				            "Actions",
				            Gui::TextStyle::builder()
				                .size(17.0f)
				                .color(m_gui.theme().on_surface)
				                .selected_color(m_gui.theme().on_primary)
				                .build());
				        Gui::components::Button::builder(dialog, "dialog_close")
				            .label("Close")
				            .on_activate([&]() {
					            m_dialog_open = false;
					            m_gui.invalidate_compose();
				            })
				            .build();
			        })
			        .build();
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
				    renderer().draw_text(payload.value_view(),
				        payload.box,
				        payload.size,
				        payload.color,
				        align_x,
				        align_y,
				        std::nullopt,
				        payload.wrap);
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

	if (m_gui_hud_visible >= 1) {
		std::string fps_label {};
		fps_label.reserve(32);
		std::format_to(
		    std::back_inserter(fps_label), "fps: {:.01f}", fps_smooth);
		renderer().draw_text(fps_label,
		    Engine::Rect<> {
		        .position = smath::Vec2 { 6.0f, 2.0f },
		        .size = smath::Vec2 { 200.0f, 20.0f },
		    },
		    16.0f,
		    Engine::Color::GREEN);

		auto const render_stats { Engine::Platform::renderer_stats() };
		std::string render_label {};
		render_label.reserve(128);
		std::format_to(std::back_inserter(render_label),
		    "b:{} t:{} s:{} bind:{} up:{} ub:{}",
		    render_stats.batch_submits,
		    render_stats.textured_submits,
		    render_stats.solid_submits,
		    render_stats.texture_binds,
		    render_stats.texture_uploads,
		    render_stats.texture_upload_bytes / 1024u);
		renderer().draw_text(render_label,
		    Engine::Rect<> {
		        .position = smath::Vec2 { 6.0f, 20.0f },
		        .size = smath::Vec2 { 320.0f, 20.0f },
		    },
		    16.0f,
		    Engine::Color::GREEN);
	}

	if (m_gui.get_hud_visible()) {
		constexpr float HISTOGRAM_X { 332.0f };
		constexpr float HISTOGRAM_Y { 4.0f };
		constexpr float HISTOGRAM_W { 144.0f };
		constexpr float HISTOGRAM_H { 52.0f };
		constexpr float PLOT_X { HISTOGRAM_X + 2.0f };
		constexpr float PLOT_Y { HISTOGRAM_Y + 2.0f };
		constexpr float PLOT_W { HISTOGRAM_W - 4.0f };
		constexpr float PLOT_H { HISTOGRAM_H - 4.0f };
		constexpr float FRAME_MS_SCALE_MAX { 50.0f };
		constexpr float FRAME_MS_60FPS { 16.7f };
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

			std::vector<Engine::GraphicsVertex> bar_vertices;
			std::vector<uint16_t> bar_indices;
			bar_vertices.reserve(static_cast<size_t>(plot_columns) * 4);
			bar_indices.reserve(static_cast<size_t>(plot_columns) * 6);
			for (int x_pixel = 0; x_pixel < plot_columns; ++x_pixel) {
				auto const sample_pos
				    = (static_cast<size_t>(x_pixel) * sample_den)
				    / static_cast<size_t>(column_den);
				auto const sample_index
				    = (oldest_index + sample_pos) % FRAME_TIME_HISTORY_CAPACITY;
				auto const sample_ms = m_frame_ms_history[sample_index];
				auto const normalized
				    = std::clamp(sample_ms / FRAME_MS_SCALE_MAX, 0.0f, 1.0f);
				auto const bar_h = std::max(1.0f, normalized * (PLOT_H - 1.0f));
				auto const x = PLOT_X + static_cast<float>(x_pixel);
				auto const y = PLOT_Y + PLOT_H - bar_h;

				smath::Vec4 color { 1.0f, 0.15f, 0.15f, 1.0f };
				if (sample_ms <= FRAME_MS_60FPS) {
					color = smath::Vec4 { 0.0f, 1.0f, 0.0f, 1.0f };
				} else if (sample_ms <= FRAME_MS_30FPS) {
					color = smath::Vec4 { 1.0f, 0.85f, 0.1f, 1.0f };
				}
				uint32_t packed_color = smath::pack_unorm4x8(color);
				Engine::GraphicsVertex tl { 0.f, 0.f, packed_color, x, y, 0.f };
				Engine::GraphicsVertex tr {
					1.f, 0.f, packed_color, x + 1.0f, y, 0.f
				};
				Engine::GraphicsVertex bl {
					0.f, 1.f, packed_color, x, y + bar_h, 0.f
				};
				Engine::GraphicsVertex br {
					1.f, 1.f, packed_color, x + 1.0f, y + bar_h, 0.f
				};

				auto base = static_cast<uint16_t>(bar_vertices.size());
				bar_vertices.push_back(tl);
				bar_vertices.push_back(tr);
				bar_vertices.push_back(bl);
				bar_vertices.push_back(br);
				bar_indices.push_back(base);
				bar_indices.push_back(base + 2);
				bar_indices.push_back(base + 1);
				bar_indices.push_back(base + 1);
				bar_indices.push_back(base + 2);
				bar_indices.push_back(base + 3);
			}

			if (!bar_vertices.empty() && !bar_indices.empty()) {
				renderer().draw_polygons(bar_vertices, bar_indices);
			}
		}

		std::string frame_ms_label {};
		frame_ms_label.reserve(96);
		std::format_to(std::back_inserter(frame_ms_label),
		    "dt:{:.01f} avg:{:.01f} max:{:.01f}",
		    current_ms,
		    avg_ms,
		    max_ms);
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

	// renderer().draw_text("日本語が好き",
	//     Engine::Rect<> {
	//         smath::Vec2 { 0.0f, 50.0f },
	//         smath::Vec2 { 200.0f, 200.0f },
	//     },
	//     24.0f,
	//     Engine::Color::RED,
	//     Engine::TextAlignX::Left,
	//     Engine::TextAlignY::Top,
	//     m_notosansjp_font);

	// renderer().draw_text("asdfasdf87asdfas8d7f5sadf5456asd4f6as79d",
	//     Engine::Rect<> {
	//         smath::Vec2 { 0.0f, 100.0f },
	//         smath::Vec2 { 200.0f, 200.0f },
	//     },
	//     16.0f,
	//     Engine::Color::RED);
}

} // namespace MaterialFM
