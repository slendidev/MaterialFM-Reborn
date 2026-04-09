#pragma once

#include "engine/Asset.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace Engine
{

struct TextureHandle
{
	uint32_t id { 0xFFFFFFFFu };
};

struct SoundHandle
{
	uint32_t id { 0xFFFFFFFFu };
};

struct SongHandle
{
	uint32_t id { 0xFFFFFFFFu };
};

struct FontHandle
{
	uint32_t id { 0xFFFFFFFFu };
};

enum class AssetError
{
	Ok = 0,
	InvalidArgument,
	AlreadyExists,
	NotFound,
	FileReadFailed,
	DecodeFailed,
	UnsupportedFormat,
	AudioChannelUnavailable,
	AudioOutputFailed,
};

struct SoundPlaybackOptions
{
	float volume { 1.0f };
	float speed { 1.0f };
};

struct SongPlaybackOptions
{
	float volume { 1.0f };
	bool loop { true };
};

struct AssetManager
{
	AssetManager();
	~AssetManager();

	AssetManager(AssetManager &&) noexcept;
	auto operator=(AssetManager &&) noexcept -> AssetManager &;

	AssetManager(AssetManager const &) = delete;
	auto operator=(AssetManager const &) -> AssetManager & = delete;

	auto load_texture_from_file(std::string_view name, std::string_view path)
	    -> AssetError;
	auto load_texture_from_file(
	    std::string_view name, std::string_view path, TextureHandle &out_handle)
	    -> AssetError;
	auto load_texture_from_memory(
	    std::string_view name, std::span<uint8_t const> bytes) -> AssetError;
	auto load_texture_from_memory(std::string_view name,
	    std::span<uint8_t const> bytes,
	    TextureHandle &out_handle) -> AssetError;
	auto load_sound_from_file(std::string_view name, std::string_view path)
	    -> AssetError;
	auto load_sound_from_file(
	    std::string_view name, std::string_view path, SoundHandle &out_handle)
	    -> AssetError;
	auto load_sound_from_memory(
	    std::string_view name, std::span<uint8_t const> bytes) -> AssetError;
	auto load_sound_from_memory(std::string_view name,
	    std::span<uint8_t const> bytes,
	    SoundHandle &out_handle) -> AssetError;
	auto load_song_from_file(std::string_view name, std::string_view path)
	    -> AssetError;
	auto load_song_from_file(
	    std::string_view name, std::string_view path, SongHandle &out_handle)
	    -> AssetError;
	auto load_font_from_file(std::string_view name, std::string_view path)
	    -> AssetError;
	auto load_font_from_file(
	    std::string_view name, std::string_view path, FontHandle &out_handle)
	    -> AssetError;

	auto unload_texture(std::string_view name) -> AssetError;
	auto unload_sound(std::string_view name) -> AssetError;
	auto unload_song(std::string_view name) -> AssetError;
	auto unload_font(std::string_view name) -> AssetError;
	auto unload_texture(TextureHandle handle) -> AssetError;
	auto unload_sound(SoundHandle handle) -> AssetError;
	auto unload_song(SongHandle handle) -> AssetError;
	auto unload_font(FontHandle handle) -> AssetError;

	auto texture(std::string_view name) const -> Texture const *;
	auto sound(std::string_view name) const -> Sound const *;
	auto song(std::string_view name) const -> Song const *;
	auto font(std::string_view name) const -> Font const *;
	auto texture(TextureHandle handle) const -> Texture const *;
	auto sound(SoundHandle handle) const -> Sound const *;
	auto song(SongHandle handle) const -> Song const *;
	auto font(FontHandle handle) const -> Font const *;
	auto texture_handle(std::string_view name) const -> TextureHandle;
	auto sound_handle(std::string_view name) const -> SoundHandle;
	auto song_handle(std::string_view name) const -> SongHandle;
	auto font_handle(std::string_view name) const -> FontHandle;
	auto set_active_font(FontHandle handle) -> AssetError;
	auto active_font_handle() const -> FontHandle;

	auto play_sound(std::string_view name,
	    SoundPlaybackOptions options = SoundPlaybackOptions {}) -> AssetError;
	auto play_sound(SoundHandle handle,
	    SoundPlaybackOptions options = SoundPlaybackOptions {}) -> AssetError;
	auto play_song(std::string_view name,
	    SongPlaybackOptions options = SongPlaybackOptions {}) -> AssetError;
	auto play_song(SongHandle handle,
	    SongPlaybackOptions options = SongPlaybackOptions {}) -> AssetError;
	auto pause_song() -> void;
	auto is_playing_song() const -> bool;
	auto set_song_position(float seconds) -> AssetError;
	auto get_song_position() const -> float;
	auto set_song_volume(float volume) -> AssetError;
	auto get_song_volume() const -> float;
	auto stop_song() -> void;
	auto update_audio() -> void;
	auto shutdown_audio() -> void;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl {};
};

} // namespace Engine
