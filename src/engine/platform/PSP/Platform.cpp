#include "engine/platform/Platform.h"

#include <atomic>
#include <chrono>
#include <cmath>

#include <pspctrl.h>
#include <pspkernel.h>
#include <pspuser.h>

PSP_MODULE_INFO(ENGINE_PROJECT_NAME, 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

namespace Engine::Platform
{
namespace
{
std::atomic<bool> g_exit_requested {};

auto exit_callback(int arg1, int arg2, void *common) -> int
{
	(void)arg1, (void)arg2, (void)common;
	g_exit_requested.store(true, std::memory_order_release);
	return 0;
}

auto callback_thread(SceSize args, void *argp) -> int
{
	(void)args, (void)argp;
	int cbid { sceKernelCreateCallback("Exit Callback", exit_callback, 0) };
	sceKernelRegisterExitCallback(cbid);
	sceKernelSleepThreadCB();
	return 0;
}

auto setup_callbacks() -> int
{
	int thid {
		sceKernelCreateThread(
		    "update_thread", callback_thread, 0x11, 0xFA0, 0, 0),
	};
	if (thid >= 0) {
		sceKernelStartThread(thid, 0, 0);
	}
	return thid;
}

auto monotonic_now_seconds() -> double
{
	using clock = std::chrono::steady_clock;
	static auto const start = clock::now();
	auto const now = clock::now();
	std::chrono::duration<double> elapsed = now - start;
	return elapsed.count();
}

auto apply_deadzone(float const value, float const deadzone) -> float
{
	if (std::abs(value) < deadzone) {
		return 0.0f;
	}
	return (value > 0.0f ? (value - deadzone) : (value + deadzone))
	    / (1.0f - deadzone);
}

auto normalize_u8(uint8_t const value) -> float
{
	return (static_cast<float>(value) - 128.0f) / 127.0f;
}
} // namespace

auto initialize() -> void
{
	setup_callbacks();
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
}

auto shutdown() -> void
{
	if (g_exit_requested.load(std::memory_order_acquire)) {
		sceKernelExitGame();
	}
}

auto now_seconds() -> double
{
	return monotonic_now_seconds();
}

auto poll_input() -> ::Engine::detail::InputState
{
	SceCtrlData pad {};
	sceCtrlReadBufferPositive(&pad, 1);

	uint32_t buttons {};
	if ((pad.Buttons & PSP_CTRL_UP) != 0u) {
		buttons |= detail::button_mask(Button::Up);
	}
	if ((pad.Buttons & PSP_CTRL_DOWN) != 0u) {
		buttons |= detail::button_mask(Button::Down);
	}
	if ((pad.Buttons & PSP_CTRL_LEFT) != 0u) {
		buttons |= detail::button_mask(Button::Left);
	}
	if ((pad.Buttons & PSP_CTRL_RIGHT) != 0u) {
		buttons |= detail::button_mask(Button::Right);
	}
	if ((pad.Buttons & PSP_CTRL_CROSS) != 0u) {
		buttons |= detail::button_mask(Button::Cross);
	}
	if ((pad.Buttons & PSP_CTRL_CIRCLE) != 0u) {
		buttons |= detail::button_mask(Button::Circle);
	}
	if ((pad.Buttons & PSP_CTRL_START) != 0u) {
		buttons |= detail::button_mask(Button::Start);
	}
	if ((pad.Buttons & PSP_CTRL_SELECT) != 0u) {
		buttons |= detail::button_mask(Button::Select);
	}
	if ((pad.Buttons & PSP_CTRL_TRIANGLE) != 0u) {
		buttons |= detail::button_mask(Button::Triangle);
	}
	if ((pad.Buttons & PSP_CTRL_LTRIGGER) != 0u) {
		buttons |= detail::button_mask(Button::LeftTrigger);
	}
	if ((pad.Buttons & PSP_CTRL_RTRIGGER) != 0u) {
		buttons |= detail::button_mask(Button::RightTrigger);
	}

	return ::Engine::detail::InputState {
	    .buttons = buttons,
	    .stick = smath::Vec2 {
	        apply_deadzone(normalize_u8(pad.Lx), 0.1f),
	        apply_deadzone(normalize_u8(pad.Ly), 0.1f),
	    },
	};
}

auto is_exit_requested() -> bool
{
	return g_exit_requested.load(std::memory_order_acquire);
}

} // namespace Engine::Platform
