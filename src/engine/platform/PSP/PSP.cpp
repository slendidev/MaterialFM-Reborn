#include "engine/AssetManager.h"
#include "engine/Common.h"
#include "engine/Renderer.h"
#include "engine/platform/PSP/MediaEngineOgg.h"
#include "engine/platform/Platform.h"

#include <algorithm>
#include <cassert>
#include <vector>

#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspkernel.h>
#include <pspmp3.h>
#include <psptypes.h>
#include <psputility.h>

namespace
{
constexpr unsigned int BUF_WIDTH { 512 };
constexpr unsigned int PIXEL_SIZE { 4 };
constexpr unsigned int FRAME_SIZE { BUF_WIDTH * GU_SCR_HEIGHT * PIXEL_SIZE };

struct BatchedTexturedVertex
{
	float u {};
	float v {};
	uint32_t color {};
	float x {};
	float y {};
	float z {};
};

struct BatchedColorVertex
{
	uint32_t color {};
	float x {};
	float y {};
	float z {};
};

static unsigned int __attribute__((aligned(16))) g_gpu_list[0x40000];
std::vector<Engine::Rect<>> g_clip_stack {};
Engine::Platform::RendererStats g_renderer_stats {};
Engine::Texture const *g_bound_texture {};
int g_scissor_x {};
int g_scissor_y {};
int g_scissor_w {};
int g_scissor_h {};
bool g_scissor_initialized {};
} // namespace

namespace Engine
{

auto Texture::set(bool const upload_data) const -> void
{
	sassert(is_power_of_two(this->width), "Texture width is not a power of 2");
	sassert(
	    is_power_of_two(this->height), "Texture height is not a power of 2");

	sceGuTexMode(GU_PSM_8888, 0, 0, 0);
	if (upload_data) {
		sceKernelDcacheWritebackInvalidateRange(this->data.data(),
		    static_cast<unsigned int>(this->data.size() * sizeof(uint32_t)));
		mark_uploaded();
	}
	sceGuTexImage(0, this->width, this->height, this->width, this->data.data());
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	sceGuTexFilter(GU_NEAREST, GU_NEAREST);
	sceGuTexWrap(GU_CLAMP, GU_CLAMP);
	sceGuTexScale(1.0f / static_cast<float>(this->width),
	    1.0f / static_cast<float>(this->height));
	sceGuTexOffset(0, 0);
	sceGuAmbientColor(0xffffffff);
}

} // namespace Engine

namespace Engine::Platform
{

auto renderer_create() -> void
{
	sceGuInit();
	sceGuStart(GU_DIRECT, g_gpu_list);
	sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_WIDTH);
	sceGuDispBuffer(GU_SCR_WIDTH, GU_SCR_HEIGHT, (void *)FRAME_SIZE, BUF_WIDTH);
	sceGuDepthBuffer((void *)(FRAME_SIZE * 2), BUF_WIDTH);
	sceGuOffset(2048 - (GU_SCR_WIDTH / 2), 2048 - (GU_SCR_HEIGHT / 2));
	sceGuViewport(2048, 2048, GU_SCR_WIDTH, GU_SCR_HEIGHT);
	sceGuDepthRange(0xc350, 0x2710);
	sceGuScissor(0, 0, GU_SCR_WIDTH, GU_SCR_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuDisable(GU_DEPTH_TEST);
	sceGuShadeModel(GU_SMOOTH);
	sceGuEnable(GU_BLEND);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexMode(GU_PSM_8888, 0, 0, 0);
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	sceGuTexEnvColor(0x0);
	sceGuTexOffset(0.0f, 0.0f);
	sceGuTexScale(1.0f / 256.0f, 1.0f / 128.0f);
	sceGuTexWrap(GU_REPEAT, GU_REPEAT);
	sceGuTexFilter(GU_NEAREST, GU_NEAREST);
	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
	sceGuDisplay(GU_TRUE);
}

auto renderer_destroy() -> void
{
	sceGuDisplay(GU_FALSE);
	sceGuTerm();
}

auto renderer_begin_frame() -> void
{
	sceGuStart(GU_DIRECT, g_gpu_list);
	g_clip_stack.clear();
	g_renderer_stats = RendererStats {};
	g_bound_texture = nullptr;

	sceGuDisable(GU_DEPTH_TEST);
	sceGuDisable(GU_CULL_FACE);
	sceGuDisable(GU_ALPHA_TEST);
	sceGuEnable(GU_BLEND);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexMode(GU_PSM_8888, 0, 0, 0);
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	sceGuTexFilter(GU_NEAREST, GU_NEAREST);
	sceGuScissor(0, 0, GU_SCR_WIDTH, GU_SCR_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	g_scissor_x = 0;
	g_scissor_y = 0;
	g_scissor_w = GU_SCR_WIDTH;
	g_scissor_h = GU_SCR_HEIGHT;
	g_scissor_initialized = true;
}

auto renderer_end_frame() -> void
{
	sceGuFinish();
	sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);

	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
}

auto renderer_mode_2d() -> void
{
	sceGumMatrixMode(GU_PROJECTION);
	sceGumLoadIdentity();
	sceGumOrtho(0.0f,
	    static_cast<float>(GU_SCR_WIDTH),
	    0,
	    static_cast<float>(GU_SCR_HEIGHT),
	    0.0f,
	    1.0f);

	sceGumMatrixMode(GU_VIEW);
	sceGumLoadIdentity();

	sceGumMatrixMode(GU_MODEL);
	sceGumLoadIdentity();
}

auto renderer_clear(smath::Vec4 const color) -> void
{
	sceGuClearColor(smath::pack_unorm4x8(color));
	sceGuClear(GU_COLOR_BUFFER_BIT);
}

auto apply_scissor(Engine::Rect<> const clip) -> void
{
	auto const x {
		static_cast<int>(std::ceil(std::max(0.0f, clip.position.x()))),
	};
	auto const y {
		static_cast<int>(std::ceil(std::max(0.0f, clip.position.y()))),
	};
	auto const right {
		static_cast<int>(std::floor(std::min(static_cast<float>(GU_SCR_WIDTH),
		    clip.position.x() + clip.size.x()))),
	};
	auto const bottom {
		static_cast<int>(std::floor(std::min(static_cast<float>(GU_SCR_HEIGHT),
		    clip.position.y() + clip.size.y()))),
	};
	auto const width { std::max(0, right - x) };
	auto const height { std::max(0, bottom - y) };

	if (g_scissor_initialized && g_scissor_x == x && g_scissor_y == y
	    && g_scissor_w == width && g_scissor_h == height) {
		return;
	}

	sceGuScissor(x, y, width, height);
	g_scissor_x = x;
	g_scissor_y = y;
	g_scissor_w = width;
	g_scissor_h = height;
	g_scissor_initialized = true;
}

auto renderer_push_scissor(Rect<> const rect) -> void
{
	g_renderer_stats.clip_pushes += 1;
	auto clip { rect };
	if (!g_clip_stack.empty()) {
		auto const &parent { g_clip_stack.back() };
		auto const parent_right { parent.position.x() + parent.size.x() };
		auto const parent_bottom { parent.position.y() + parent.size.y() };
		auto const clip_right { clip.position.x() + clip.size.x() };
		auto const clip_bottom { clip.position.y() + clip.size.y() };
		auto const x0 { std::max(clip.position.x(), parent.position.x()) };
		auto const y0 { std::max(clip.position.y(), parent.position.y()) };
		auto const x1 { std::min(clip_right, parent_right) };
		auto const y1 { std::min(clip_bottom, parent_bottom) };
		clip.position = smath::Vec2 { x0, y0 };
		clip.size = smath::Vec2 {
			std::max(0.0f, x1 - x0),
			std::max(0.0f, y1 - y0),
		};
	}

	g_clip_stack.push_back(clip);
	apply_scissor(clip);
}

auto renderer_pop_scissor() -> void
{
	g_renderer_stats.clip_pops += 1;
	if (!g_clip_stack.empty()) {
		g_clip_stack.pop_back();
	}

	if (g_clip_stack.empty()) {
		apply_scissor(Engine::Rect<> {
		    .position = smath::Vec2 { 0.0f, 0.0f },
		    .size = smath::Vec2 {
		        static_cast<float>(GU_SCR_WIDTH),
		        static_cast<float>(GU_SCR_HEIGHT),
		    },
		});
		return;
	}

	auto const &clip { g_clip_stack.back() };
	apply_scissor(clip);
}

auto renderer_submit_batch(Texture const *const texture,
    std::span<::Engine::detail::GraphicsVertex const> const vertices,
    std::span<uint16_t const> const indices) -> void
{
	if (vertices.empty() || indices.empty()) {
		return;
	}

	g_renderer_stats.batch_submits += 1;

	auto const index_count { static_cast<int>(indices.size()) };
	auto *gpu_indices { static_cast<uint16_t *>(
		sceGuGetMemory(static_cast<int>(sizeof(uint16_t) * indices.size()))) };
	std::copy(indices.begin(), indices.end(), gpu_indices);

	if (texture != nullptr) {
		g_renderer_stats.textured_submits += 1;
		sceGuEnable(GU_TEXTURE_2D);
		auto const needs_upload { texture->needs_upload() };
		auto const needs_bind { g_bound_texture != texture };
		if (needs_upload || needs_bind) {
			texture->set(needs_upload);
			if (needs_bind) {
				g_renderer_stats.texture_binds += 1;
			}
			if (needs_upload) {
				g_renderer_stats.texture_uploads += 1;
				g_renderer_stats.texture_upload_bytes += static_cast<uint32_t>(
				    texture->data.size() * sizeof(uint32_t));
			}
			g_bound_texture = texture;
		}

		auto *gpu_vertices { static_cast<BatchedTexturedVertex *>(
			sceGuGetMemory(static_cast<int>(
			    sizeof(BatchedTexturedVertex) * vertices.size()))) };
		for (size_t i { 0 }; i < vertices.size(); ++i) {
			gpu_vertices[i] = BatchedTexturedVertex {
				.u = vertices[i].u,
				.v = vertices[i].v,
				.color = vertices[i].color,
				.x = vertices[i].x,
				.y = vertices[i].y,
				.z = vertices[i].z,
			};
		}

		sceGuDrawArray(GU_TRIANGLES,
		    GU_INDEX_16BIT | GU_COLOR_8888 | GU_TEXTURE_32BITF
		        | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
		    index_count,
		    gpu_indices,
		    gpu_vertices);
		return;
	}

	g_renderer_stats.solid_submits += 1;
	sceGuDisable(GU_TEXTURE_2D);
	auto *gpu_vertices { static_cast<BatchedColorVertex *>(sceGuGetMemory(
		static_cast<int>(sizeof(BatchedColorVertex) * vertices.size()))) };
	for (size_t i { 0 }; i < vertices.size(); ++i) {
		gpu_vertices[i] = BatchedColorVertex {
			.color = vertices[i].color,
			.x = vertices[i].x,
			.y = vertices[i].y,
			.z = vertices[i].z,
		};
	}

	sceGuDrawArray(GU_TRIANGLES,
	    GU_INDEX_16BIT | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
	    index_count,
	    gpu_indices,
	    gpu_vertices);
	sceGuEnable(GU_TEXTURE_2D);
}

auto renderer_stats() -> RendererStats
{
	return g_renderer_stats;
}

} // namespace Engine::Platform

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <pspaudiolib.h>
#include <pspthreadman.h>

#include <kb_text_shape.h>
#include <stb_image.h>
#include <stb_truetype.h>

#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
#undef STB_VORBIS_HEADER_ONLY

namespace Engine
{

namespace
{
constexpr int AUDIO_OUTPUT_RATE { 44100 };
constexpr int AUDIO_OUTPUT_CHANNELS { 2 };
constexpr int SONG_DECODE_CHUNK_SAMPLES { 4096 };
constexpr unsigned int MIX_CHANNEL_INDEX { 0 };
constexpr unsigned int SONG_CHANNEL_INDEX { 1 };
constexpr size_t MAX_SOUND_VOICES { 8 };
constexpr size_t AUDIO_COMMAND_QUEUE_CAPACITY { 128 };
constexpr size_t MAX_COMMANDS_PER_CALLBACK { 12 };
constexpr size_t SONG_RING_CAPACITY_FRAMES { 32768 };
constexpr size_t SONG_RING_LOW_WATERMARK { 8192 };
constexpr size_t SONG_RING_HIGH_WATERMARK { 24576 };
constexpr size_t SONG_COMPACT_THRESHOLD_FRAMES { 4096 };
constexpr float SONG_OUTPUT_GAIN { 0.35f };
constexpr unsigned int SONG_THREAD_SLEEP_US { 100 };
constexpr int SONG_THREAD_PRIORITY { 0x40 };
constexpr int SONG_THREAD_STACK { 0x20000 };
constexpr size_t MP3_STREAM_BUF_SIZE { 16 * 1024 };
constexpr size_t MP3_PCM_BUF_SIZE { 16 * (1152 / 2) };

inline auto allocate_aligned(size_t const size) -> uint8_t *
{
	return static_cast<uint8_t *>(aligned_alloc(64, size));
}

inline auto fill_mp3_stream_buffer(SceInt32 const handle,
    std::vector<uint8_t> const &file_data,
    size_t &file_pos) -> bool
{
	SceUChar8 *dst {};
	SceInt32 towrite {};
	SceInt32 srcpos {};

	auto const status { sceMp3GetInfoToAddStreamData(
		handle, &dst, &towrite, &srcpos) };
	if (status < 0 || dst == nullptr || towrite <= 0) {
		return false;
	}

	auto const remaining { file_data.size() - file_pos };
	auto const to_copy { std::min(static_cast<size_t>(towrite), remaining) };
	if (to_copy > 0) {
		std::copy_n(file_data.data() + file_pos, to_copy, dst);
		sceMp3NotifyAddStreamData(handle, static_cast<SceInt32>(to_copy));
		file_pos += to_copy;
	}

	return true;
}

struct Mp3DecoderResources
{
	SceInt32 handle { -1 };
	uint8_t *stream_buf { nullptr };
	uint8_t *pcm_buf { nullptr };
	size_t file_pos {};
};

auto release_mp3_resources(Mp3DecoderResources &resources) -> void
{
	if (resources.handle >= 0) {
		sceMp3ReleaseMp3Handle(resources.handle);
		resources.handle = -1;
	}
	if (resources.stream_buf != nullptr) {
		std::free(resources.stream_buf);
		resources.stream_buf = nullptr;
	}
	if (resources.pcm_buf != nullptr) {
		std::free(resources.pcm_buf);
		resources.pcm_buf = nullptr;
	}
	resources.file_pos = 0;
}

auto reserve_and_init_mp3_decoder(std::vector<uint8_t> const &file_data,
    Mp3DecoderResources &resources) -> bool
{
	resources.stream_buf = allocate_aligned(MP3_STREAM_BUF_SIZE + 1472);
	resources.pcm_buf = allocate_aligned(MP3_PCM_BUF_SIZE);
	if (resources.stream_buf == nullptr || resources.pcm_buf == nullptr) {
		release_mp3_resources(resources);
		return false;
	}

	SceMp3InitArg mp3_arg {};
	mp3_arg.mp3StreamStart = 0;
	mp3_arg.mp3StreamEnd = static_cast<SceOff>(file_data.size());
	mp3_arg.mp3Buf = resources.stream_buf;
	mp3_arg.mp3BufSize = static_cast<SceInt32>(MP3_STREAM_BUF_SIZE + 1472);
	mp3_arg.pcmBuf = resources.pcm_buf;
	mp3_arg.pcmBufSize = static_cast<SceInt32>(MP3_PCM_BUF_SIZE);

	resources.handle = sceMp3ReserveMp3Handle(&mp3_arg);
	if (resources.handle < 0) {
		release_mp3_resources(resources);
		return false;
	}

	if (!fill_mp3_stream_buffer(
	        resources.handle, file_data, resources.file_pos)) {
		release_mp3_resources(resources);
		return false;
	}

	if (sceMp3Init(resources.handle) < 0) {
		release_mp3_resources(resources);
		return false;
	}

	return true;
}

struct StringHash
{
	using is_transparent = void;

	auto operator()(std::string_view const value) const noexcept -> size_t
	{
		return std::hash<std::string_view> {}(value);
	}

	auto operator()(std::string const &value) const noexcept -> size_t
	{
		return (*this)(std::string_view { value });
	}

	auto operator()(char const *value) const noexcept -> size_t
	{
		return (*this)(std::string_view { value });
	}
};

struct StringEqual
{
	using is_transparent = void;

	auto operator()(std::string_view const lhs,
	    std::string_view const rhs) const noexcept -> bool
	{
		return lhs == rhs;
	}
};

auto clamp_volume(float const volume) -> float
{
	if (!std::isfinite(volume)) {
		return 1.0f;
	}
	return std::clamp(volume, 0.0f, 1.0f);
}

auto is_valid_speed(float const speed) -> bool
{
	return std::isfinite(speed) && speed > 0.0f;
}

auto read_file_bytes(std::string_view const path)
    -> std::optional<std::vector<uint8_t>>
{
	std::ifstream file(std::string(path), std::ios::binary);
	if (!file) {
		return std::nullopt;
	}

	file.seekg(0, std::ios::end);
	auto const end_pos { file.tellg() };
	if (end_pos <= 0) {
		return std::nullopt;
	}

	std::vector<uint8_t> bytes(static_cast<size_t>(end_pos));
	file.seekg(0, std::ios::beg);
	file.read(reinterpret_cast<char *>(bytes.data()),
	    static_cast<std::streamsize>(bytes.size()));
	if (!file) {
		return std::nullopt;
	}

	return bytes;
}

auto frame_at(short const *samples,
    int const channels,
    size_t const frame_index,
    float &left,
    float &right) -> void
{
	if (channels == 1) {
		auto const mono { static_cast<float>(samples[frame_index]) };
		left = mono;
		right = mono;
		return;
	}

	auto const base { frame_index * static_cast<size_t>(channels) };
	left = static_cast<float>(samples[base]);
	right = static_cast<float>(samples[base + 1]);
}

auto clamp_i16(float const value) -> short
{
	auto const clamped { std::clamp(value, -32768.0f, 32767.0f) };
	return static_cast<short>(clamped);
}

template<typename T, size_t Capacity> struct SpscRing
{
	static_assert(
	    (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

	std::array<T, Capacity> data {};
	std::atomic<uint32_t> write_index { 0 };
	std::atomic<uint32_t> read_index { 0 };

	auto reset() -> void
	{
		read_index.store(0, std::memory_order_relaxed);
		write_index.store(0, std::memory_order_relaxed);
	}

	auto size() const -> uint32_t
	{
		auto const w { write_index.load(std::memory_order_acquire) };
		auto const r { read_index.load(std::memory_order_acquire) };
		return w - r;
	}

	auto free_space() const -> uint32_t
	{
		return static_cast<uint32_t>(Capacity) - size();
	}

	auto push(T const *src, uint32_t count) -> uint32_t
	{
		auto const w { write_index.load(std::memory_order_relaxed) };
		auto const r { read_index.load(std::memory_order_acquire) };
		auto const free { static_cast<uint32_t>(Capacity) - (w - r) };
		auto const to_write { std::min(count, free) };

		for (uint32_t i { 0 }; i < to_write; ++i) {
			data[(w + i) & (Capacity - 1)] = src[i];
		}

		write_index.store(w + to_write, std::memory_order_release);
		return to_write;
	}

	auto pop(T *dst, uint32_t count) -> uint32_t
	{
		auto const r { read_index.load(std::memory_order_relaxed) };
		auto const w { write_index.load(std::memory_order_acquire) };
		auto const avail { w - r };
		auto const to_read { std::min(count, avail) };

		for (uint32_t i { 0 }; i < to_read; ++i) {
			dst[i] = data[(r + i) & (Capacity - 1)];
		}

		read_index.store(r + to_read, std::memory_order_release);
		return to_read;
	}
};
} // namespace

struct AssetManager::Impl
{
	struct AudioCommand
	{
		enum class Type
		{
			PlaySound,
			UnloadSound,
		};

		Type type { Type::PlaySound };
		uint32_t handle {};
		float volume { 1.0f };
		float speed { 1.0f };
	};

	struct TextureSlot
	{
		Texture *asset {};
		std::string name {};
	};

	struct SoundSlot
	{
		std::atomic<Sound *> asset { nullptr };
		std::atomic<uint32_t> generation { 1 };
		std::string name {};
	};

	struct SongSlot
	{
		Song *asset {};
		std::string name {};
		enum class Format
		{
			Ogg,
			Mp3
		} format { Format::Ogg };
	};

	struct FontSlot
	{
		Font *asset {};
		std::string name {};
	};

	struct SoundVoice
	{
		bool active {};
		uint32_t slot_index { 0xFFFFFFFFu };
		uint32_t generation {};
		SoundPlaybackOptions options {};
		double cursor {};
	};

	struct SongState
	{
		std::string song_name {};
		SongPlaybackOptions options {};
		bool paused {};
		double position_seconds {};
		stb_vorbis *decoder {};
		int source_channels {};
		int source_sample_rate {};
		bool reached_eof {};
		std::vector<short> source_buffer {};
		size_t source_buffer_start_frame {};
		size_t source_buffer_frames {};
		double source_cursor {};
		std::vector<short> decode_scratch {};
		std::vector<short> output_scratch {};
		bool use_media_engine_ogg {};
		std::vector<uint8_t> ogg_file_data {};

		SongSlot::Format format { SongSlot::Format::Ogg };
		SceInt32 mp3_handle { -1 };
		std::vector<uint8_t> mp3_file_data {};
		size_t mp3_file_pos {};
		uint8_t *mp3_stream_buf { nullptr };
		uint8_t *mp3_pcm_buf { nullptr };
	};

	using TextureMap
	    = std::unordered_map<std::string, Texture, StringHash, StringEqual>;
	using SoundMap = std::unordered_map<std::string,
	    std::unique_ptr<Sound>,
	    StringHash,
	    StringEqual>;
	using SongMap
	    = std::unordered_map<std::string, Song, StringHash, StringEqual>;
	using FontMap
	    = std::unordered_map<std::string, Font, StringHash, StringEqual>;
	using NameToHandle
	    = std::unordered_map<std::string, uint32_t, StringHash, StringEqual>;

	TextureMap textures {};
	SoundMap sounds {};
	SongMap songs {};
	FontMap fonts {};
	NameToHandle texture_names {};
	NameToHandle sound_names {};
	NameToHandle song_names {};
	NameToHandle font_names {};
	std::vector<TextureSlot> texture_slots {};
	std::deque<std::unique_ptr<SoundSlot>> sound_slots {};
	std::vector<SongSlot> song_slots {};
	std::vector<FontSlot> font_slots {};
	FontHandle active_font {};
	std::atomic<uint32_t> active_font_id { 0xFFFFFFFFu };
	std::atomic<Font const *> active_font_ptr {};

	std::array<SoundVoice, MAX_SOUND_VOICES> sound_voices {};
	std::optional<SongState> song_state {};
	std::array<AudioCommand, AUDIO_COMMAND_QUEUE_CAPACITY> command_queue {};
	std::atomic<uint32_t> command_read {};
	std::atomic<uint32_t> command_write {};

	std::vector<float> mix_accum {};
	std::mutex texture_mutex {};
	std::mutex font_mutex {};
	std::mutex audio_mutex {};
	std::mutex song_state_mutex {};

	struct RetiredSound
	{
		std::unique_ptr<Sound> sound {};
		uint32_t slot_index { 0xFFFFFFFFu };
		uint32_t generation {};
	};

	std::vector<RetiredSound> retired_sounds {};

	static constexpr size_t SONG_RING_SAMPLES_CAPACITY {
		SONG_RING_CAPACITY_FRAMES * AUDIO_OUTPUT_CHANNELS
	};

	SpscRing<short, SONG_RING_SAMPLES_CAPACITY> song_ring {};
	std::atomic<bool> song_playing {};
	std::atomic<bool> song_paused {};
	std::atomic<float> song_position_seconds {};
	std::atomic<float> song_volume { 1.0f };
	std::atomic<uint32_t> song_underruns {};
	SceUID song_thread_id { -1 };
	std::atomic<bool> song_thread_stop {};
	std::atomic<bool> audio_initialized {};

	static auto audio_callback(void *buf, unsigned int reqn, void *pdata)
	    -> void
	{
		auto *impl { static_cast<Impl *>(pdata) };
		if (impl == nullptr || buf == nullptr || reqn == 0) {
			return;
		}
		impl->mix_audio(static_cast<short *>(buf), reqn);
	}

	static auto song_callback(void *buf, unsigned int reqn, void *pdata) -> void
	{
		auto *impl { static_cast<Impl *>(pdata) };
		if (impl == nullptr || buf == nullptr || reqn == 0) {
			return;
		}
		impl->mix_song_channel(static_cast<short *>(buf), reqn);
	}

	static auto song_thread_entry(SceSize const args, void *argp) -> int
	{
		if (args < sizeof(Impl *) || argp == nullptr) {
			return 0;
		}

		auto *impl { *static_cast<Impl **>(argp) };
		if (impl == nullptr) {
			return 0;
		}
		return impl->song_thread_main();
	}

	auto start_song_thread() -> void
	{
		if (song_thread_id >= 0) {
			return;
		}

		song_thread_stop.store(false, std::memory_order_release);

		song_thread_id = sceKernelCreateThread("asset_song_decode",
		    &Impl::song_thread_entry,
		    SONG_THREAD_PRIORITY,
		    SONG_THREAD_STACK,
		    0,
		    nullptr);
		if (song_thread_id < 0) {
			return;
		}

		auto *self { this };
		auto const start_result {
			sceKernelStartThread(song_thread_id, sizeof(self), &self),
		};
		if (start_result < 0) {
			sceKernelDeleteThread(song_thread_id);
			song_thread_id = -1;
		}
	}

	auto stop_song_thread() -> void
	{
		if (song_thread_id < 0) {
			return;
		}

		song_thread_stop.store(true, std::memory_order_release);
		sceKernelWaitThreadEnd(song_thread_id, nullptr);
		sceKernelDeleteThread(song_thread_id);
		song_thread_id = -1;
	}

	auto ring_push_frames(short const *samples, size_t const frames) -> size_t
	{
		auto const sample_count {
			static_cast<uint32_t>(frames * AUDIO_OUTPUT_CHANNELS),
		};
		auto const written { song_ring.push(samples, sample_count) };
		return static_cast<size_t>(written / AUDIO_OUTPUT_CHANNELS);
	}

	auto ring_pop_song_frames(
	    short *out, size_t const max_frames, float const volume) -> size_t
	{
		auto const max_samples {
			static_cast<uint32_t>(max_frames * AUDIO_OUTPUT_CHANNELS),
		};

		auto const popped_samples { song_ring.pop(out, max_samples) };
		auto const popped_frames {
			static_cast<size_t>(popped_samples / AUDIO_OUTPUT_CHANNELS),
		};

		auto const scaled_samples {
			popped_frames * static_cast<size_t>(AUDIO_OUTPUT_CHANNELS),
		};
		for (size_t i { 0 }; i < scaled_samples; ++i) {
			out[i] = clamp_i16(static_cast<float>(out[i]) * volume);
		}

		return popped_frames;
	}

	auto song_ring_filled() const -> size_t
	{
		return static_cast<size_t>(song_ring.size() / AUDIO_OUTPUT_CHANNELS);
	}

	auto clear_song_ring() -> void { song_ring.reset(); }

	auto ensure_audio_started() -> AssetError
	{
		if (audio_initialized.load(std::memory_order_acquire)) {
			return AssetError::Ok;
		}

		auto const init_status { pspAudioInit() };
		if (init_status < 0) {
			return AssetError::AudioOutputFailed;
		}

		sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
		sceUtilityLoadModule(PSP_MODULE_AV_MP3);
		sceMp3InitResource();

		pspAudioSetChannelCallback(
		    MIX_CHANNEL_INDEX, &Impl::audio_callback, this);
		pspAudioSetChannelCallback(
		    SONG_CHANNEL_INDEX, &Impl::song_callback, this);
		audio_initialized.store(true, std::memory_order_release);
		return AssetError::Ok;
	}

	auto enqueue_play_sound_command(
	    uint32_t const handle, float const volume, float const speed) -> bool
	{
		auto const write { command_write.load(std::memory_order_relaxed) };
		auto const next {
			(write + 1u) % static_cast<uint32_t>(AUDIO_COMMAND_QUEUE_CAPACITY),
		};
		auto const read { command_read.load(std::memory_order_acquire) };
		if (next == read) {
			return false;
		}

		AudioCommand cmd {
			.type = AudioCommand::Type::PlaySound,
			.handle = handle,
			.volume = volume,
			.speed = speed,
		};

		command_queue[write] = cmd;
		command_write.store(next, std::memory_order_release);
		return true;
	}

	auto enqueue_handle_command(
	    AudioCommand::Type const type, uint32_t const handle) -> bool
	{
		auto const write { command_write.load(std::memory_order_relaxed) };
		auto const next {
			(write + 1u) % static_cast<uint32_t>(AUDIO_COMMAND_QUEUE_CAPACITY),
		};
		auto const read { command_read.load(std::memory_order_acquire) };
		if (next == read) {
			return false;
		}

		AudioCommand cmd {
			.type = type,
			.handle = handle,
		};

		command_queue[write] = cmd;
		command_write.store(next, std::memory_order_release);
		return true;
	}

	auto process_pending_commands_locked() -> void { }

	auto stop_voices_for_sound_slot_locked(uint32_t const slot_index) -> void
	{
		for (auto &voice : sound_voices) {
			if (voice.active && voice.slot_index == slot_index) {
				voice.active = false;
			}
		}
	}

	auto find_free_voice_index_rt() -> int
	{
		for (size_t i { 0 }; i < sound_voices.size(); ++i) {
			if (!sound_voices[i].active) {
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	auto steal_voice_index_rt() -> int { return 0; }

	auto process_pending_commands_rt() -> void
	{
		auto read { command_read.load(std::memory_order_relaxed) };
		auto const write { command_write.load(std::memory_order_acquire) };
		size_t processed {};

		while (read != write && processed < MAX_COMMANDS_PER_CALLBACK) {
			auto const &cmd { command_queue[read] };

			if (cmd.type == AudioCommand::Type::PlaySound) {
				if (cmd.handle < sound_slots.size()) {
					auto const generation {
						sound_slots[cmd.handle]->generation.load(
						    std::memory_order_acquire),
					};
					auto *sound {
						sound_slots[cmd.handle]->asset.load(
						    std::memory_order_acquire),
					};

					if (sound != nullptr) {
						auto voice_index { find_free_voice_index_rt() };
						if (voice_index < 0) {
							voice_index = steal_voice_index_rt();
						}

						if (voice_index >= 0) {
							auto &voice {
								sound_voices[static_cast<size_t>(voice_index)]
							};
							voice.active = true;
							voice.slot_index = cmd.handle;
							voice.generation = generation;
							voice.options.volume = clamp_volume(cmd.volume);
							voice.options.speed = cmd.speed;
							voice.cursor = 0.0;
						}
					}
				}
			} else if (cmd.type == AudioCommand::Type::UnloadSound) {
				if (cmd.handle < sound_slots.size()) {
					for (auto &voice : sound_voices) {
						if (voice.active && voice.slot_index == cmd.handle) {
							voice.active = false;
						}
					}
				}
			}

			read = (read + 1u)
			    % static_cast<uint32_t>(AUDIO_COMMAND_QUEUE_CAPACITY);
			++processed;
		}

		command_read.store(read, std::memory_order_release);
	}

	auto stop_song_locked() -> void
	{
		if (!song_state.has_value()) {
			song_playing.store(false, std::memory_order_release);
			song_paused.store(false, std::memory_order_release);
			song_position_seconds.store(0.0f, std::memory_order_release);
			song_underruns.store(0, std::memory_order_release);
			clear_song_ring();
			return;
		}
		if (song_state->format == SongSlot::Format::Mp3) {
			if (song_state->mp3_handle >= 0) {
				sceMp3ReleaseMp3Handle(song_state->mp3_handle);
				song_state->mp3_handle = -1;
			}
			if (song_state->mp3_stream_buf != nullptr) {
				std::free(song_state->mp3_stream_buf);
				song_state->mp3_stream_buf = nullptr;
			}
			if (song_state->mp3_pcm_buf != nullptr) {
				std::free(song_state->mp3_pcm_buf);
				song_state->mp3_pcm_buf = nullptr;
			}
		} else {
			if (song_state->use_media_engine_ogg) {
				psp_me_ogg_close();
			}
			if (song_state->decoder != nullptr) {
				stb_vorbis_close(song_state->decoder);
				song_state->decoder = nullptr;
			}
		}
		song_state.reset();
		song_playing.store(false, std::memory_order_release);
		song_paused.store(false, std::memory_order_release);
		song_position_seconds.store(0.0f, std::memory_order_release);
		song_underruns.store(0, std::memory_order_release);
		clear_song_ring();
	}

	auto can_reclaim_sound_slot_generation(uint32_t const slot_index,
	    uint32_t const retired_generation) const -> bool
	{
		for (auto const &voice : sound_voices) {
			if (!voice.active) {
				continue;
			}
			if (voice.slot_index == slot_index
			    && voice.generation == retired_generation) {
				return false;
			}
		}
		return true;
	}

	auto reclaim_retired_sounds_locked() -> void
	{
		auto out_it {
			std::remove_if(retired_sounds.begin(),
			    retired_sounds.end(),
			    [this](RetiredSound const &retired) {
			        return can_reclaim_sound_slot_generation(
			            retired.slot_index, retired.generation);
			    }),
		};
		retired_sounds.erase(out_it, retired_sounds.end());
	}

	auto decode_more_song_frames_locked(SongState &state) -> int
	{
		if (state.reached_eof) {
			return 0;
		}

		if (state.format == SongSlot::Format::Mp3) {
			return decode_more_song_frames_mp3_locked(state);
		}

		if (state.decoder == nullptr) {
			if (!state.use_media_engine_ogg) {
				return 0;
			}
		}

		auto const shorts_per_chunk {
			static_cast<size_t>(
			    SONG_DECODE_CHUNK_SAMPLES * state.source_channels),
		};
		state.decode_scratch.resize(shorts_per_chunk);

		int decoded_frames {};
		if (state.use_media_engine_ogg) {
			auto const decode_ok { psp_me_ogg_decode_interleaved(
				state.decode_scratch.data(),
				SONG_DECODE_CHUNK_SAMPLES,
				state.source_channels,
				&decoded_frames) };
			if (!decode_ok) {
				state.reached_eof = true;
				return 0;
			}
		} else {
			decoded_frames
			    = stb_vorbis_get_samples_short_interleaved(state.decoder,
			        state.source_channels,
			        state.decode_scratch.data(),
			        static_cast<int>(state.decode_scratch.size()));
		}

		if (decoded_frames <= 0) {
			state.reached_eof = true;
			return 0;
		}

		auto const shorts_decoded {
			static_cast<size_t>(decoded_frames * state.source_channels),
		};
		state.source_buffer.insert(state.source_buffer.end(),
		    state.decode_scratch.begin(),
		    state.decode_scratch.begin()
		        + static_cast<std::ptrdiff_t>(shorts_decoded));
		state.source_buffer_frames += static_cast<size_t>(decoded_frames);

		return decoded_frames;
	}

	auto decode_more_song_frames_mp3_locked(SongState &state) -> int
	{
		if (state.mp3_handle < 0) {
			return 0;
		}

		auto const needed_frames { static_cast<size_t>(
			SONG_DECODE_CHUNK_SAMPLES) };

		while (state.source_buffer_frames
		    < state.source_buffer_start_frame + needed_frames) {
			auto const need_stream { sceMp3CheckStreamDataNeeded(
				state.mp3_handle) };

			if (need_stream > 0) {
				SceUChar8 *dst {};
				SceInt32 towrite {};
				SceInt32 srcpos {};
				sceMp3GetInfoToAddStreamData(
				    state.mp3_handle, &dst, &towrite, &srcpos);

				if (towrite > 0
				    && state.mp3_file_pos < state.mp3_file_data.size()) {
					auto const remaining { static_cast<size_t>(
						state.mp3_file_data.size() - state.mp3_file_pos) };
					auto const to_copy { std::min(
						static_cast<size_t>(towrite), remaining) };
					std::copy_n(state.mp3_file_data.data() + state.mp3_file_pos,
					    to_copy,
					    dst);
					sceMp3NotifyAddStreamData(
					    state.mp3_handle, static_cast<SceInt32>(to_copy));
					state.mp3_file_pos += to_copy;
				}
			}

			SceShort16 *pcm_dst {};
			auto const result { sceMp3Decode(state.mp3_handle, &pcm_dst) };
			if (result < 0 || pcm_dst == nullptr) {
				if (state.mp3_file_pos >= state.mp3_file_data.size()) {
					state.reached_eof = true;
				}
				break;
			}

			auto const decoded_frames { static_cast<size_t>(result) / 2
				/ static_cast<size_t>(state.source_channels) };
			if (decoded_frames == 0) {
				if (state.mp3_file_pos >= state.mp3_file_data.size()) {
					state.reached_eof = true;
				}
				break;
			}

			auto const sample_count { decoded_frames
				* static_cast<size_t>(state.source_channels) };
			state.source_buffer.insert(
			    state.source_buffer.end(), pcm_dst, pcm_dst + sample_count);
			state.source_buffer_frames += decoded_frames;
		}

		return static_cast<int>(
		    state.source_buffer_frames - state.source_buffer_start_frame);
	}

	auto render_song_frames_locked(SongState &state, size_t const frames)
	    -> size_t
	{
		bool valid_decoder { state.format == SongSlot::Format::Mp3
			    ? (state.mp3_handle >= 0)
			    : (state.use_media_engine_ogg || state.decoder != nullptr) };
		if (!valid_decoder || state.source_sample_rate <= 0
		    || (state.source_channels != 1 && state.source_channels != 2)) {
			return 0;
		}

		state.output_scratch.resize(frames * AUDIO_OUTPUT_CHANNELS);
		auto const is_native_rate {
			state.source_sample_rate == AUDIO_OUTPUT_RATE,
		};
		auto const step {
			is_native_rate ? 1.0
			               : (static_cast<double>(state.source_sample_rate)
			                     / static_cast<double>(AUDIO_OUTPUT_RATE)),
		};

		auto const has_source_frame {
			[is_native_rate](size_t const cursor, size_t const available) {
			    return is_native_rate ? (cursor < available)
			                          : (cursor + 1 < available);
			},
		};

		size_t produced {};
		for (; produced < frames; ++produced) {
			while (true) {
				auto const idx0 { static_cast<size_t>(state.source_cursor) };
				auto const available {
					state.source_buffer_frames
					    - state.source_buffer_start_frame,
				};
				if (has_source_frame(idx0, available)) {
					break;
				}

				decode_more_song_frames_locked(state);
				auto const next_idx0 { static_cast<size_t>(
					state.source_cursor) };
				auto const next_available {
					state.source_buffer_frames
					    - state.source_buffer_start_frame,
				};
				if (has_source_frame(next_idx0, next_available)) {
					break;
				}

				if (!state.reached_eof) {
					continue;
				}

				if (!state.options.loop) {
					return produced;
				}

				if (state.format == SongSlot::Format::Mp3) {
					if (state.mp3_handle < 0) {
						return produced;
					}
					if (sceMp3ResetPlayPosition(state.mp3_handle) < 0) {
						return produced;
					}
					state.mp3_file_pos = 0;
				} else {
					if (state.use_media_engine_ogg) {
						if (!psp_me_ogg_seek_start()) {
							return produced;
						}
					} else if (stb_vorbis_seek_start(state.decoder) == 0) {
						return produced;
					}
				}

				state.source_buffer.clear();
				state.source_buffer_start_frame = 0;
				state.source_buffer_frames = 0;
				state.source_cursor = 0.0;
				state.reached_eof = false;
				state.position_seconds = 0.0;
				song_position_seconds.store(0.0f, std::memory_order_release);
			}

			auto const idx0_local { static_cast<size_t>(state.source_cursor) };
			auto const idx0 { state.source_buffer_start_frame + idx0_local };

			float left {};
			float right {};
			if (is_native_rate) {
				frame_at(state.source_buffer.data(),
				    state.source_channels,
				    idx0,
				    left,
				    right);
			} else {
				auto const idx1 { std::min(
					idx0 + 1, state.source_buffer_frames - 1) };
				auto const frac { std::clamp(
					static_cast<float>(
					    state.source_cursor - static_cast<double>(idx0_local)),
					0.0f,
					1.0f) };

				float l0, r0, l1, r1;
				frame_at(state.source_buffer.data(),
				    state.source_channels,
				    idx0,
				    l0,
				    r0);
				frame_at(state.source_buffer.data(),
				    state.source_channels,
				    idx1,
				    l1,
				    r1);

				left = l0 + (l1 - l0) * frac;
				right = r0 + (r1 - r0) * frac;
			}

			auto const dst { produced * AUDIO_OUTPUT_CHANNELS };
			state.output_scratch[dst] = clamp_i16(left);
			state.output_scratch[dst + 1] = clamp_i16(right);

			state.source_cursor += step;
		}

		size_t consumed_frames {};
		if (is_native_rate) {
			consumed_frames = static_cast<size_t>(state.source_cursor);
		} else if (state.source_cursor > 1.0) {
			consumed_frames = static_cast<size_t>(state.source_cursor) - 1;
		}
		if (consumed_frames > 0) {
			state.source_buffer_start_frame += consumed_frames;
			state.source_cursor -= static_cast<double>(consumed_frames);

			if (state.source_buffer_start_frame
			    >= SONG_COMPACT_THRESHOLD_FRAMES) {
				auto const samples_to_drop {
					state.source_buffer_start_frame
					    * static_cast<size_t>(state.source_channels),
				};
				state.source_buffer.erase(state.source_buffer.begin(),
				    state.source_buffer.begin()
				        + static_cast<std::ptrdiff_t>(samples_to_drop));
				state.source_buffer_frames -= state.source_buffer_start_frame;
				state.source_buffer_start_frame = 0;
			}
		}

		state.position_seconds += static_cast<double>(produced)
		    / static_cast<double>(AUDIO_OUTPUT_RATE);
		song_position_seconds.store(static_cast<float>(state.position_seconds),
		    std::memory_order_release);

		return produced;
	}

	auto song_thread_main() -> int
	{
		constexpr size_t TARGET_FRAMES { 2048 };
		while (!song_thread_stop.load(std::memory_order_acquire)) {
			if (song_paused.load(std::memory_order_acquire)) {
				sceKernelDelayThread(SONG_THREAD_SLEEP_US);
				continue;
			}

			{
				std::lock_guard<std::mutex> lock(song_state_mutex);
				bool valid_decoder { !song_state.has_value()
					    ? false
					    : (song_state->format == SongSlot::Format::Mp3
					              ? (song_state->mp3_handle >= 0)
					              : (song_state->use_media_engine_ogg
					                    || song_state->decoder != nullptr)) };
				if (!valid_decoder) {
					sceKernelDelayThread(SONG_THREAD_SLEEP_US);
					continue;
				}
			}

			auto const filled { song_ring_filled() };
			if (filled >= SONG_RING_HIGH_WATERMARK) {
				sceKernelDelayThread(SONG_THREAD_SLEEP_US);
				continue;
			}

			auto const target_fill { SONG_RING_LOW_WATERMARK };
			auto const needed {
				(filled >= target_fill) ? 0 : (target_fill - filled)
			};
			if (needed == 0) {
				sceKernelDelayThread(SONG_THREAD_SLEEP_US);
				continue;
			}

			auto const decode_frames { std::min(TARGET_FRAMES, needed) };

			size_t produced {};
			{
				std::lock_guard<std::mutex> lock(song_state_mutex);
				bool valid_decoder { !song_state.has_value()
					    ? false
					    : (song_state->format == SongSlot::Format::Mp3
					              ? (song_state->mp3_handle >= 0)
					              : (song_state->use_media_engine_ogg
					                    || song_state->decoder != nullptr)) };
				if (!valid_decoder || song_state->paused) {
					sceKernelDelayThread(SONG_THREAD_SLEEP_US);
					continue;
				}

				produced
				    = render_song_frames_locked(*song_state, decode_frames);
				if (produced == 0 && !song_state->options.loop) {
					song_playing.store(false, std::memory_order_release);
					song_paused.store(false, std::memory_order_release);
					song_state->paused = true;
					sceKernelDelayThread(SONG_THREAD_SLEEP_US);
					continue;
				}

				if (produced > 0) {
					ring_push_frames(
					    song_state->output_scratch.data(), produced);
				}
			}

			if (!song_playing.load(std::memory_order_acquire)
			    && song_ring_filled() >= SONG_RING_LOW_WATERMARK) {
				song_playing.store(true, std::memory_order_release);
			}
		}

		return 0;
	}

	auto mix_sound_voice_rt(SoundVoice &voice, unsigned int const frames)
	    -> bool
	{
		if (!voice.active) {
			return true;
		}
		if (voice.slot_index >= sound_slots.size()) {
			return true;
		}

		auto const slot_generation {
			sound_slots[voice.slot_index]->generation.load(
			    std::memory_order_acquire),
		};
		auto *sound {
			sound_slots[voice.slot_index]->asset.load(
			    std::memory_order_acquire),
		};

		if (sound == nullptr || slot_generation != voice.generation) {
			return true;
		}

		auto const src_channels { sound->channels };
		if ((src_channels != 1 && src_channels != 2) || sound->sample_rate <= 0
		    || sound->samples.empty()) {
			return true;
		}

		auto const total_frames {
			sound->samples.size() / static_cast<size_t>(src_channels),
		};
		auto const step {
			static_cast<double>(voice.options.speed)
			    * static_cast<double>(sound->sample_rate)
			    / static_cast<double>(AUDIO_OUTPUT_RATE),
		};
		auto const volume { clamp_volume(voice.options.volume) };
		auto const is_native_rate {
			(sound->sample_rate == AUDIO_OUTPUT_RATE)
			    && (voice.options.speed == 1.0f),
		};

		if (is_native_rate) {
			for (unsigned int i { 0 }; i < frames; ++i) {
				auto const frame_index { static_cast<size_t>(voice.cursor) };
				if (frame_index >= total_frames) {
					return true;
				}

				float left, right;
				frame_at(sound->samples.data(),
				    src_channels,
				    frame_index,
				    left,
				    right);

				auto const dst { static_cast<size_t>(i) * 2 };
				mix_accum[dst] += left * volume;
				mix_accum[dst + 1] += right * volume;

				voice.cursor += 1.0;
			}

			return false;
		}

		for (unsigned int i { 0 }; i < frames; ++i) {
			if (voice.cursor >= static_cast<double>(total_frames)) {
				return true;
			}

			auto const idx0 { static_cast<size_t>(voice.cursor) };
			auto const idx1 { std::min(idx0 + 1, total_frames - 1) };
			auto const frac {
				static_cast<float>(voice.cursor - static_cast<double>(idx0)),
			};

			float l0, r0, l1, r1;
			frame_at(sound->samples.data(), src_channels, idx0, l0, r0);
			frame_at(sound->samples.data(), src_channels, idx1, l1, r1);

			auto const left { (l0 + (l1 - l0) * frac) * volume };
			auto const right { (r0 + (r1 - r0) * frac) * volume };

			auto const dst { static_cast<size_t>(i) * 2 };
			mix_accum[dst] += left;
			mix_accum[dst + 1] += right;

			voice.cursor += step;
		}

		return false;
	}

	auto mix_song_channel(short *const out, unsigned int const frames) -> void
	{
		auto const sample_count {
			static_cast<size_t>(frames) * AUDIO_OUTPUT_CHANNELS,
		};
		std::fill_n(out, sample_count, static_cast<short>(0));

		if (!song_playing.load(std::memory_order_acquire)
		    || song_paused.load(std::memory_order_acquire)) {
			return;
		}

		auto const volume {
			clamp_volume(song_volume.load(std::memory_order_acquire))
			    * SONG_OUTPUT_GAIN,
		};
		auto const consumed { ring_pop_song_frames(out, frames, volume) };
		if (consumed < frames) {
			if (consumed == 0) {
				song_playing.store(false, std::memory_order_release);
				song_underruns.fetch_add(1u, std::memory_order_relaxed);
			}
			auto const start { consumed * AUDIO_OUTPUT_CHANNELS };
			for (size_t i { start }; i < sample_count; ++i) {
				out[i] = 0;
			}
		}
	}

	auto mix_audio(short *const out, unsigned int const frames) -> void
	{
		process_pending_commands_rt();

		auto const sample_count {
			static_cast<size_t>(frames) * AUDIO_OUTPUT_CHANNELS,
		};

		sassert(sample_count <= mix_accum.size(),
		    "mix_accum too small for callback request");

		std::fill_n(mix_accum.begin(), sample_count, 0.0f);

		for (auto &voice : sound_voices) {
			if (!voice.active) {
				continue;
			}

			auto const finished { mix_sound_voice_rt(voice, frames) };
			if (finished) {
				voice.active = false;
			}
		}

		for (size_t i { 0 }; i < sample_count; ++i) {
			out[i] = clamp_i16(mix_accum[i]);
		}
	}

	auto shutdown_audio_locked() -> bool
	{
		{
			std::lock_guard<std::mutex> song_lock(song_state_mutex);
			stop_song_locked();
		}
		for (auto &voice : sound_voices) {
			voice.active = false;
		}
		retired_sounds.clear();
		sounds.clear();
		command_read.store(0, std::memory_order_relaxed);
		command_write.store(0, std::memory_order_relaxed);
		if (!audio_initialized.load(std::memory_order_acquire)) {
			return false;
		}

		pspAudioSetChannelCallback(MIX_CHANNEL_INDEX, nullptr, nullptr);
		pspAudioSetChannelCallback(SONG_CHANNEL_INDEX, nullptr, nullptr);
		sceMp3TermResource();
		audio_initialized.store(false, std::memory_order_release);
		return true;
	}
};

AssetManager::AssetManager() : m_impl(std::make_unique<Impl>())
{
	m_impl->mix_accum.resize(
	    static_cast<size_t>(PSP_NUM_AUDIO_SAMPLES * AUDIO_OUTPUT_CHANNELS));

	for (auto &voice : m_impl->sound_voices) {
		voice.active = false;
	}

	m_impl->start_song_thread();
}

AssetManager::~AssetManager()
{
	shutdown_audio();
}

AssetManager::AssetManager(AssetManager &&) noexcept = default;

auto AssetManager::operator=(AssetManager &&) noexcept
    -> AssetManager & = default;

auto AssetManager::load_texture_from_file(
    std::string_view const name, std::string_view const path) -> AssetError
{
	TextureHandle unused {};
	return load_texture_from_file(name, path, unused);
}

auto AssetManager::load_texture_from_file(std::string_view const name,
    std::string_view const path,
    TextureHandle &out_handle) -> AssetError
{
	auto const bytes { read_file_bytes(path) };
	if (!bytes.has_value()) {
		return AssetError::FileReadFailed;
	}

	return load_texture_from_memory(name, *bytes, out_handle);
}

auto AssetManager::load_texture_from_memory(std::string_view const name,
    std::span<uint8_t const> const bytes) -> AssetError
{
	TextureHandle unused {};
	return load_texture_from_memory(name, bytes, unused);
}

auto AssetManager::load_texture_from_memory(std::string_view const name,
    std::span<uint8_t const> const bytes,
    TextureHandle &out_handle) -> AssetError
{
	if (name.empty() || bytes.empty()) {
		return AssetError::InvalidArgument;
	}

	std::lock_guard<std::mutex> lock(m_impl->texture_mutex);
	if (m_impl->texture_names.find(name) != m_impl->texture_names.end()) {
		return AssetError::AlreadyExists;
	}

	int width {}, height {}, channels {};
	auto *image {
		stbi_load_from_memory(bytes.data(),
		    static_cast<int>(bytes.size()),
		    &width,
		    &height,
		    &channels,
		    4),
	};
	if (image == nullptr || width <= 0 || height <= 0) {
		if (image != nullptr) {
			stbi_image_free(image);
		}
		return AssetError::DecodeFailed;
	}

	auto const pixels_count { static_cast<size_t>(width * height) };
	std::vector<uint32_t> rgba(pixels_count);
	std::memcpy(rgba.data(), image, pixels_count * sizeof(uint32_t));
	stbi_image_free(image);

	Texture texture {
		std::span<uint32_t const>(rgba.data(), rgba.size()), width, height
	};
	auto [it, inserted] {
		m_impl->textures.emplace(std::string(name), std::move(texture)),
	};
	if (!inserted) {
		return AssetError::AlreadyExists;
	}
	auto const handle { static_cast<uint32_t>(m_impl->texture_slots.size()) };
	m_impl->texture_names.emplace(std::string(name), handle);
	m_impl->texture_slots.push_back(
	    Impl::TextureSlot { &it->second, std::string(name) });
	out_handle.id = handle;
	return AssetError::Ok;
}

auto AssetManager::load_sound_from_file(
    std::string_view const name, std::string_view const path) -> AssetError
{
	SoundHandle unused {};
	return load_sound_from_file(name, path, unused);
}

auto AssetManager::load_sound_from_file(std::string_view const name,
    std::string_view const path,
    SoundHandle &out_handle) -> AssetError
{
	auto const bytes { read_file_bytes(path) };
	if (!bytes.has_value()) {
		return AssetError::FileReadFailed;
	}

	return load_sound_from_memory(name, *bytes, out_handle);
}

auto AssetManager::load_sound_from_memory(std::string_view const name,
    std::span<uint8_t const> const bytes) -> AssetError
{
	SoundHandle unused {};
	return load_sound_from_memory(name, bytes, unused);
}

auto AssetManager::load_sound_from_memory(std::string_view const name,
    std::span<uint8_t const> const bytes,
    SoundHandle &out_handle) -> AssetError
{
	if (name.empty() || bytes.empty()) {
		return AssetError::InvalidArgument;
	}

	std::lock_guard<std::mutex> lock(m_impl->audio_mutex);
	if (m_impl->sound_names.find(name) != m_impl->sound_names.end()) {
		return AssetError::AlreadyExists;
	}

	int channels {}, sample_rate {};
	short *output { nullptr };
	auto const sample_count {
		stb_vorbis_decode_memory(bytes.data(),
		    static_cast<int>(bytes.size()),
		    &channels,
		    &sample_rate,
		    &output),
	};

	if (sample_count <= 0 || output == nullptr) {
		if (output != nullptr) {
			std::free(output);
		}
		return AssetError::DecodeFailed;
	}

	if (channels != 1 && channels != 2) {
		std::free(output);
		return AssetError::UnsupportedFormat;
	}

	auto const sample_total { static_cast<size_t>(sample_count * channels) };
	auto sound_ptr { std::make_unique<Sound>() };
	sound_ptr->channels = channels;
	sound_ptr->sample_rate = sample_rate;
	sound_ptr->samples.assign(
	    output, output + static_cast<std::ptrdiff_t>(sample_total));
	std::free(output);

	auto raw { sound_ptr.get() };
	auto [it, inserted] {
		m_impl->sounds.emplace(std::string(name), std::move(sound_ptr)),
	};
	if (!inserted) {
		return AssetError::AlreadyExists;
	}

	auto const handle { static_cast<uint32_t>(m_impl->sound_slots.size()) };
	m_impl->sound_names.emplace(std::string(name), handle);

	auto new_slot { std::make_unique<Impl::SoundSlot>() };
	new_slot->name = std::string(name);
	new_slot->asset.store(raw, std::memory_order_release);
	new_slot->generation.store(1, std::memory_order_release);

	m_impl->sound_slots.push_back(std::move(new_slot));
	out_handle.id = handle;
	m_impl->reclaim_retired_sounds_locked();
	return AssetError::Ok;
}

auto AssetManager::load_song_from_file(
    std::string_view const name, std::string_view const path) -> AssetError
{
	SongHandle unused {};
	return load_song_from_file(name, path, unused);
}

auto AssetManager::load_song_from_file(std::string_view const name,
    std::string_view const path,
    SongHandle &out_handle) -> AssetError
{
	if (name.empty() || path.empty()) {
		return AssetError::InvalidArgument;
	}

	std::lock_guard<std::mutex> lock(m_impl->audio_mutex);
	if (m_impl->song_names.find(name) != m_impl->song_names.end()) {
		return AssetError::AlreadyExists;
	}

	auto const path_str { std::string(path) };
	auto const is_mp3 { path_str.size() >= 4
		&& (path_str.ends_with(".mp3") || path_str.ends_with(".MP3")) };

	Song song {};
	Impl::SongSlot::Format format { Impl::SongSlot::Format::Ogg };

	if (is_mp3) {
		sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
		sceUtilityLoadModule(PSP_MODULE_AV_MP3);
		sceMp3InitResource();

		auto const file_data { read_file_bytes(path) };
		if (!file_data.has_value() || file_data->empty()) {
			return AssetError::FileReadFailed;
		}

		Mp3DecoderResources mp3 {};
		if (!reserve_and_init_mp3_decoder(*file_data, mp3)) {
			return AssetError::DecodeFailed;
		}

		song.sample_rate = sceMp3GetSamplingRate(mp3.handle);
		song.channels = sceMp3GetMp3ChannelNum(mp3.handle);
		release_mp3_resources(mp3);

		if (song.channels != 1 && song.channels != 2) {
			return AssetError::UnsupportedFormat;
		}

		format = Impl::SongSlot::Format::Mp3;
	} else {
		int error {};
		auto *decoder { stb_vorbis_open_filename(
			path_str.c_str(), &error, nullptr) };
		if (decoder == nullptr) {
			(void)error;
			return AssetError::DecodeFailed;
		}

		auto const info { stb_vorbis_get_info(decoder) };
		stb_vorbis_close(decoder);

		if (info.channels != 1 && info.channels != 2) {
			return AssetError::UnsupportedFormat;
		}

		song.channels = info.channels;
		song.sample_rate = static_cast<int>(info.sample_rate);
	}

	song.path = path_str;
	auto [it, inserted] {
		m_impl->songs.emplace(std::string(name), std::move(song)),
	};
	if (!inserted) {
		return AssetError::AlreadyExists;
	}
	auto const handle { static_cast<uint32_t>(m_impl->song_slots.size()) };
	m_impl->song_names.emplace(std::string(name), handle);
	m_impl->song_slots.push_back(
	    Impl::SongSlot { &it->second, std::string(name), format });
	out_handle.id = handle;

	return AssetError::Ok;
}

auto AssetManager::load_font_from_file(
    std::string_view const name, std::string_view const path) -> AssetError
{
	FontHandle unused {};
	return load_font_from_file(name, path, unused);
}

auto AssetManager::load_font_from_file(std::string_view const name,
    std::string_view const path,
    FontHandle &out_handle) -> AssetError
{
	if (name.empty() || path.empty()) {
		return AssetError::InvalidArgument;
	}

	std::lock_guard<std::mutex> lock(m_impl->font_mutex);
	if (m_impl->font_names.find(name) != m_impl->font_names.end()) {
		return AssetError::AlreadyExists;
	}

	auto const path_string { std::string(path) };
	auto bytes_opt { read_file_bytes(path) };
	if (!bytes_opt.has_value()) {
		return AssetError::FileReadFailed;
	}
	auto bytes { std::move(*bytes_opt) };

	stbtt_fontinfo info {};
	if (stbtt_InitFont(&info, bytes.data(), 0) == 0) {
		return AssetError::DecodeFailed;
	}
	kbts_font kb_font {
		kbts_FontFromMemory(
		    bytes.data(), static_cast<int>(bytes.size()), 0, nullptr, nullptr),
	};
	if (kbts_FontIsValid(&kb_font) == 0) {
		return AssetError::DecodeFailed;
	}
	kbts_font_info2_1 info2 {};
	info2.Base.Size = sizeof(info2);
	kbts_GetFontInfo2(&kb_font, reinterpret_cast<kbts_font_info2 *>(&info2));
	kbts_FreeFont(&kb_font);

	int ascent {};
	int descent {};
	int line_gap {};
	stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);

	Font font_asset {
		.path = path_string,
		.ttf_data = std::move(bytes),
		.units_per_em = std::max(static_cast<int>(info2.UnitsPerEm), 1),
		.ascent = info2.Ascent != 0 ? info2.Ascent : ascent,
		.descent = info2.Descent != 0 ? info2.Descent : descent,
		.line_gap = info2.LineGap != 0 ? info2.LineGap : line_gap,
		.atlas_base_size_px = 64.0f,
		.atlas = Texture {},
	};
	font_asset.atlas.width = 512;
	font_asset.atlas.height = 512;
	font_asset.atlas.content_width = 512;
	font_asset.atlas.content_height = 512;
	font_asset.atlas.data.assign(static_cast<size_t>(512 * 512), 0);
	font_asset.atlas_pen_x = 1;
	font_asset.atlas_pen_y = 1;
	font_asset.atlas_row_height = 0;

	auto [it, inserted] {
		m_impl->fonts.emplace(std::string(name), std::move(font_asset)),
	};
	if (!inserted) {
		return AssetError::AlreadyExists;
	}
	auto const handle { static_cast<uint32_t>(m_impl->font_slots.size()) };
	m_impl->font_names.emplace(std::string(name), handle);
	m_impl->font_slots.push_back(
	    Impl::FontSlot { &it->second, std::string(name) });
	out_handle.id = handle;
	if (m_impl->active_font.id == 0xFFFFFFFFu) {
		m_impl->active_font = out_handle;
		m_impl->active_font_id.store(out_handle.id, std::memory_order_release);
		m_impl->active_font_ptr.store(&it->second, std::memory_order_release);
	}

	return AssetError::Ok;
}

auto AssetManager::unload_texture(std::string_view const name) -> AssetError
{
	return unload_texture(texture_handle(name));
}

auto AssetManager::unload_sound(std::string_view const name) -> AssetError
{
	return unload_sound(sound_handle(name));
}

auto AssetManager::unload_song(std::string_view const name) -> AssetError
{
	return unload_song(song_handle(name));
}

auto AssetManager::unload_font(std::string_view const name) -> AssetError
{
	return unload_font(font_handle(name));
}

auto AssetManager::unload_texture(TextureHandle const handle) -> AssetError
{
	if (handle.id == 0xFFFFFFFFu) {
		return AssetError::NotFound;
	}
	std::lock_guard<std::mutex> lock(m_impl->texture_mutex);
	if (handle.id >= m_impl->texture_slots.size()) {
		return AssetError::NotFound;
	}
	auto const slot { m_impl->texture_slots[handle.id] };
	if (slot.asset == nullptr) {
		return AssetError::NotFound;
	}
	m_impl->textures.erase(slot.name);
	m_impl->texture_names.erase(slot.name);
	m_impl->texture_slots[handle.id].asset = nullptr;
	m_impl->texture_slots[handle.id].name.clear();
	return AssetError::Ok;
}

auto AssetManager::unload_sound(SoundHandle const handle) -> AssetError
{
	if (handle.id == 0xFFFFFFFFu) {
		return AssetError::NotFound;
	}

	if (m_impl->audio_initialized.load(std::memory_order_acquire)) {
		if (!m_impl->enqueue_handle_command(
		        Impl::AudioCommand::Type::UnloadSound, handle.id)) {
			return AssetError::AudioChannelUnavailable;
		}
	}

	std::lock_guard<std::mutex> lock(m_impl->audio_mutex);

	if (handle.id >= m_impl->sound_slots.size()) {
		return AssetError::NotFound;
	}

	auto *current { m_impl->sound_slots[handle.id]->asset.load(
		std::memory_order_acquire) };
	if (current == nullptr) {
		return AssetError::NotFound;
	}

	m_impl->sound_slots[handle.id]->asset.store(
	    nullptr, std::memory_order_release);
	m_impl->sound_slots[handle.id]->generation.fetch_add(
	    1u, std::memory_order_acq_rel);

	auto const slot_name { m_impl->sound_slots[handle.id]->name };
	auto it { m_impl->sounds.find(slot_name) };
	if (it != m_impl->sounds.end()) {
		auto const retired_gen {
			m_impl->sound_slots[handle.id]->generation.load(
			    std::memory_order_acquire),
		};
		Impl::RetiredSound retired {};
		retired.sound = std::move(it->second);
		retired.slot_index = handle.id;
		retired.generation = retired_gen - 1;
		m_impl->retired_sounds.push_back(std::move(retired));
		m_impl->sounds.erase(it);
	}

	m_impl->sound_names.erase(slot_name);
	m_impl->sound_slots[handle.id]->name.clear();

	m_impl->reclaim_retired_sounds_locked();

	return AssetError::Ok;
}

auto AssetManager::unload_song(SongHandle const handle) -> AssetError
{
	if (handle.id == 0xFFFFFFFFu) {
		return AssetError::NotFound;
	}

	std::lock_guard<std::mutex> lock(m_impl->audio_mutex);
	if (handle.id >= m_impl->song_slots.size()) {
		return AssetError::NotFound;
	}
	auto const slot { m_impl->song_slots[handle.id] };
	if (slot.asset == nullptr) {
		return AssetError::NotFound;
	}
	{
		std::lock_guard<std::mutex> song_lock(m_impl->song_state_mutex);
		if (m_impl->song_state.has_value()
		    && m_impl->song_state->song_name == slot.name) {
			m_impl->stop_song_locked();
		}
	}
	m_impl->songs.erase(slot.name);
	m_impl->song_names.erase(slot.name);
	m_impl->song_slots[handle.id].asset = nullptr;
	m_impl->song_slots[handle.id].name.clear();
	return AssetError::Ok;
}

auto AssetManager::unload_font(FontHandle const handle) -> AssetError
{
	if (handle.id == 0xFFFFFFFFu) {
		return AssetError::NotFound;
	}
	std::lock_guard<std::mutex> lock(m_impl->font_mutex);
	if (handle.id >= m_impl->font_slots.size()) {
		return AssetError::NotFound;
	}
	auto const slot { m_impl->font_slots[handle.id] };
	if (slot.asset == nullptr) {
		return AssetError::NotFound;
	}
	m_impl->fonts.erase(slot.name);
	m_impl->font_names.erase(slot.name);
	m_impl->font_slots[handle.id].asset = nullptr;
	m_impl->font_slots[handle.id].name.clear();
	if (m_impl->active_font.id == handle.id) {
		m_impl->active_font = FontHandle {};
		m_impl->active_font_id.store(0xFFFFFFFFu, std::memory_order_release);
		m_impl->active_font_ptr.store(nullptr, std::memory_order_release);
	}
	return AssetError::Ok;
}

auto AssetManager::texture(std::string_view const name) const -> Texture const *
{
	return texture(texture_handle(name));
}

auto AssetManager::sound(std::string_view const name) const -> Sound const *
{
	return sound(sound_handle(name));
}

auto AssetManager::song(std::string_view const name) const -> Song const *
{
	return song(song_handle(name));
}

auto AssetManager::font(std::string_view const name) const -> Font const *
{
	return font(font_handle(name));
}

auto AssetManager::texture(TextureHandle const handle) const -> Texture const *
{
	if (handle.id == 0xFFFFFFFFu || handle.id >= m_impl->texture_slots.size()) {
		return nullptr;
	}
	return m_impl->texture_slots[handle.id].asset;
}

auto AssetManager::sound(SoundHandle const handle) const -> Sound const *
{
	if (handle.id == 0xFFFFFFFFu || handle.id >= m_impl->sound_slots.size()) {
		return nullptr;
	}
	return m_impl->sound_slots[handle.id]->asset.load(
	    std::memory_order_acquire);
}

auto AssetManager::song(SongHandle const handle) const -> Song const *
{
	if (handle.id == 0xFFFFFFFFu) {
		return nullptr;
	}
	std::lock_guard<std::mutex> lock(m_impl->audio_mutex);
	if (handle.id >= m_impl->song_slots.size()) {
		return nullptr;
	}
	return m_impl->song_slots[handle.id].asset;
}

auto AssetManager::font(FontHandle const handle) const -> Font const *
{
	if (handle.id == 0xFFFFFFFFu) {
		return nullptr;
	}
	auto const active_id {
		m_impl->active_font_id.load(std::memory_order_acquire),
	};
	if (handle.id == active_id) {
		return m_impl->active_font_ptr.load(std::memory_order_acquire);
	}
	std::lock_guard<std::mutex> lock(m_impl->font_mutex);
	if (handle.id >= m_impl->font_slots.size()) {
		return nullptr;
	}
	return m_impl->font_slots[handle.id].asset;
}

auto AssetManager::texture_handle(std::string_view const name) const
    -> TextureHandle
{
	std::lock_guard<std::mutex> lock(m_impl->texture_mutex);
	auto it { m_impl->texture_names.find(name) };
	if (it == m_impl->texture_names.end()) {
		return TextureHandle {};
	}
	return TextureHandle { it->second };
}

auto AssetManager::sound_handle(std::string_view const name) const
    -> SoundHandle
{
	std::lock_guard<std::mutex> lock(m_impl->audio_mutex);
	auto it { m_impl->sound_names.find(name) };
	if (it == m_impl->sound_names.end()) {
		return SoundHandle {};
	}
	return SoundHandle { it->second };
}

auto AssetManager::song_handle(std::string_view const name) const -> SongHandle
{
	std::lock_guard<std::mutex> lock(m_impl->audio_mutex);
	auto it { m_impl->song_names.find(name) };
	if (it == m_impl->song_names.end()) {
		return SongHandle {};
	}
	return SongHandle { it->second };
}

auto AssetManager::font_handle(std::string_view const name) const -> FontHandle
{
	std::lock_guard<std::mutex> lock(m_impl->font_mutex);
	auto it { m_impl->font_names.find(name) };
	if (it == m_impl->font_names.end()) {
		return FontHandle {};
	}
	return FontHandle { it->second };
}

auto AssetManager::set_active_font(FontHandle const handle) -> AssetError
{
	std::lock_guard<std::mutex> lock(m_impl->font_mutex);
	if (handle.id == 0xFFFFFFFFu || handle.id >= m_impl->font_slots.size()) {
		return AssetError::NotFound;
	}
	if (m_impl->font_slots[handle.id].asset == nullptr) {
		return AssetError::NotFound;
	}
	m_impl->active_font = handle;
	m_impl->active_font_id.store(handle.id, std::memory_order_release);
	m_impl->active_font_ptr.store(
	    m_impl->font_slots[handle.id].asset, std::memory_order_release);
	return AssetError::Ok;
}

auto AssetManager::active_font_handle() const -> FontHandle
{
	return FontHandle {
		m_impl->active_font_id.load(std::memory_order_acquire),
	};
}

auto AssetManager::play_sound(
    std::string_view const name, SoundPlaybackOptions options) -> AssetError
{
	auto const handle { sound_handle(name) };
	if (handle.id == 0xFFFFFFFFu) {
		return AssetError::NotFound;
	}
	return play_sound(handle, options);
}

auto AssetManager::play_sound(
    SoundHandle const handle, SoundPlaybackOptions options) -> AssetError
{
	if (handle.id == 0xFFFFFFFFu) {
		return AssetError::InvalidArgument;
	}
	if (!is_valid_speed(options.speed)) {
		return AssetError::InvalidArgument;
	}

	auto const start_audio_result { m_impl->ensure_audio_started() };
	if (start_audio_result != AssetError::Ok) {
		return start_audio_result;
	}

	if (!m_impl->enqueue_play_sound_command(
	        handle.id, clamp_volume(options.volume), options.speed)) {
		return AssetError::AudioChannelUnavailable;
	}

	return AssetError::Ok;
}

auto AssetManager::play_song(
    std::string_view const name, SongPlaybackOptions options) -> AssetError
{
	auto const handle { song_handle(name) };
	if (handle.id == 0xFFFFFFFFu) {
		return AssetError::NotFound;
	}
	return play_song(handle, options);
}

auto AssetManager::play_song(
    SongHandle const handle, SongPlaybackOptions options) -> AssetError
{
	if (handle.id == 0xFFFFFFFFu) {
		return AssetError::InvalidArgument;
	}

	auto const start_audio_result { m_impl->ensure_audio_started() };
	if (start_audio_result != AssetError::Ok) {
		return start_audio_result;
	}
	m_impl->start_song_thread();

	std::string song_path {};
	std::string song_name {};
	Impl::SongSlot::Format format { Impl::SongSlot::Format::Ogg };
	int source_channels {};
	int source_sample_rate {};
	{
		std::lock_guard<std::mutex> lock(m_impl->audio_mutex);
		if (handle.id >= m_impl->song_slots.size()) {
			return AssetError::NotFound;
		}
		auto const slot { m_impl->song_slots[handle.id] };
		if (slot.asset == nullptr) {
			return AssetError::NotFound;
		}
		song_path = slot.asset->path;
		song_name = slot.name;
		format = slot.format;
		source_channels = slot.asset->channels;
		source_sample_rate = slot.asset->sample_rate;
	}

	std::lock_guard<std::mutex> song_lock(m_impl->song_state_mutex);
	if (m_impl->song_state.has_value()
	    && m_impl->song_state->song_name == song_name
	    && m_impl->song_state->paused) {
		m_impl->song_state->paused = false;
		m_impl->song_state->options.volume = clamp_volume(options.volume);
		m_impl->song_state->options.loop = options.loop;
		m_impl->song_playing.store(true, std::memory_order_release);
		m_impl->song_paused.store(false, std::memory_order_release);
		m_impl->song_volume.store(
		    clamp_volume(options.volume), std::memory_order_release);
		return AssetError::Ok;
	}

	m_impl->stop_song_locked();

	Impl::SongState state {
	    .song_name = std::string(song_name),
	    .options =
	        {
	            .volume = clamp_volume(options.volume),
	            .loop = options.loop,
	        },
	    .paused = false,
	    .position_seconds = 0.0,
	    .decoder = nullptr,
	    .source_channels = source_channels,
	    .source_sample_rate = source_sample_rate,
	    .use_media_engine_ogg = false,
	    .format = format,
	    .mp3_handle = -1,
	};

	if (format == Impl::SongSlot::Format::Mp3) {
		auto const file_data { read_file_bytes(song_path) };
		if (!file_data.has_value() || file_data->empty()) {
			return AssetError::FileReadFailed;
		}

		state.mp3_file_data = std::move(*file_data);
		Mp3DecoderResources mp3 {};
		if (!reserve_and_init_mp3_decoder(state.mp3_file_data, mp3)) {
			return AssetError::DecodeFailed;
		}
		state.mp3_handle = mp3.handle;
		state.mp3_stream_buf = mp3.stream_buf;
		state.mp3_pcm_buf = mp3.pcm_buf;
		state.mp3_file_pos = mp3.file_pos;

		state.source_sample_rate = sceMp3GetSamplingRate(state.mp3_handle);
		state.source_channels = sceMp3GetMp3ChannelNum(state.mp3_handle);
	} else {
		auto const file_data { read_file_bytes(song_path) };
		if (!file_data.has_value() || file_data->empty()) {
			return AssetError::FileReadFailed;
		}

		state.ogg_file_data = *file_data;

		int me_channels {};
		int me_sample_rate {};
		auto const opened_on_me {
			psp_me_ogg_open(state.ogg_file_data.data(),
			    static_cast<uint32_t>(state.ogg_file_data.size()),
			    &me_channels,
			    &me_sample_rate),
		};
		if (opened_on_me) {
			state.use_media_engine_ogg = true;
			state.source_channels = me_channels;
			state.source_sample_rate = me_sample_rate;
		} else {
			int error {};
			auto *decoder {
				stb_vorbis_open_filename(song_path.c_str(), &error, nullptr),
			};
			if (decoder == nullptr) {
				(void)error;
				return AssetError::DecodeFailed;
			}
			auto const info { stb_vorbis_get_info(decoder) };
			if (info.channels != 1 && info.channels != 2) {
				stb_vorbis_close(decoder);
				return AssetError::UnsupportedFormat;
			}
			state.decoder = decoder;
			state.source_channels = info.channels;
			state.source_sample_rate = static_cast<int>(info.sample_rate);
		}
	}

	m_impl->song_state = std::move(state);

	m_impl->song_volume.store(
	    clamp_volume(options.volume), std::memory_order_release);
	m_impl->song_position_seconds.store(0.0f, std::memory_order_release);
	m_impl->song_underruns.store(0, std::memory_order_release);
	m_impl->song_paused.store(false, std::memory_order_release);
	m_impl->song_playing.store(true, std::memory_order_release);

	return AssetError::Ok;
}

auto AssetManager::pause_song() -> void
{
	std::lock_guard<std::mutex> lock(m_impl->song_state_mutex);
	if (!m_impl->song_state.has_value()) {
		return;
	}
	m_impl->song_state->paused = true;
	m_impl->song_paused.store(true, std::memory_order_release);
	m_impl->song_playing.store(false, std::memory_order_release);
}

auto AssetManager::is_playing_song() const -> bool
{
	return m_impl->song_playing.load(std::memory_order_acquire)
	    && !m_impl->song_paused.load(std::memory_order_acquire);
}

auto AssetManager::set_song_position(float const seconds) -> AssetError
{
	if (!std::isfinite(seconds) || seconds < 0.0f) {
		return AssetError::InvalidArgument;
	}

	std::lock_guard<std::mutex> lock(m_impl->song_state_mutex);
	if (!m_impl->song_state.has_value()
	    || m_impl->song_state->source_sample_rate <= 0) {
		return AssetError::NotFound;
	}

	bool seek_result { false };
	if (m_impl->song_state->format == Impl::SongSlot::Format::Mp3) {
		if (m_impl->song_state->mp3_handle < 0) {
			return AssetError::NotFound;
		}
		auto const target_frame { static_cast<SceUInt32>(seconds
			* static_cast<float>(m_impl->song_state->source_sample_rate)
			/ 1152.0f) };
		seek_result = sceMp3ResetPlayPositionByFrame(
		                  m_impl->song_state->mp3_handle, target_frame)
		    >= 0;
		m_impl->song_state->mp3_file_pos = 0;
	} else {
		if (m_impl->song_state->use_media_engine_ogg) {
			auto const target_sample {
				static_cast<uint32_t>(seconds
				    * static_cast<float>(
				        m_impl->song_state->source_sample_rate)),
			};
			seek_result = psp_me_ogg_seek_sample(target_sample);
		} else {
			if (m_impl->song_state->decoder == nullptr) {
				return AssetError::NotFound;
			}
			auto const target_sample {
				static_cast<unsigned int>(seconds
				    * static_cast<float>(
				        m_impl->song_state->source_sample_rate)),
			};
			seek_result
			    = stb_vorbis_seek(m_impl->song_state->decoder, target_sample)
			    != 0;
		}
	}

	if (!seek_result) {
		return AssetError::DecodeFailed;
	}

	m_impl->song_state->source_buffer.clear();
	m_impl->song_state->source_buffer_start_frame = 0;
	m_impl->song_state->source_buffer_frames = 0;
	m_impl->song_state->source_cursor = 0.0;
	m_impl->song_state->reached_eof = false;
	m_impl->song_state->position_seconds = static_cast<double>(seconds);
	m_impl->song_position_seconds.store(seconds, std::memory_order_release);
	m_impl->clear_song_ring();

	return AssetError::Ok;
}

auto AssetManager::get_song_position() const -> float
{
	return m_impl->song_position_seconds.load(std::memory_order_acquire);
}

auto AssetManager::set_song_volume(float const volume) -> AssetError
{
	if (!std::isfinite(volume)) {
		return AssetError::InvalidArgument;
	}

	std::lock_guard<std::mutex> lock(m_impl->song_state_mutex);
	if (!m_impl->song_state.has_value()) {
		return AssetError::NotFound;
	}

	auto const clamped { clamp_volume(volume) };
	m_impl->song_state->options.volume = clamped;
	m_impl->song_volume.store(clamped, std::memory_order_release);
	return AssetError::Ok;
}

auto AssetManager::get_song_volume() const -> float
{
	return m_impl->song_volume.load(std::memory_order_acquire);
}

auto AssetManager::stop_song() -> void
{
	std::lock_guard<std::mutex> lock(m_impl->song_state_mutex);
	m_impl->stop_song_locked();
}

auto AssetManager::update_audio() -> void { }

auto AssetManager::shutdown_audio() -> void
{
	m_impl->stop_song_thread();

	bool should_end_audio { false };
	{
		std::lock_guard<std::mutex> lock(m_impl->audio_mutex);
		should_end_audio = m_impl->shutdown_audio_locked();
	}

	if (should_end_audio) {
		pspAudioEndPre();
		pspAudioEnd();
	}

	psp_me_ogg_shutdown();
}

} // namespace Engine
