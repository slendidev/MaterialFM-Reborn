#include "engine/platform/PSP/MediaEngineOgg.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <pspkernel.h>

#include <me-core.h>

#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
#undef STB_VORBIS_HEADER_ONLY

enum
{
	ME_OGG_CMD_NONE = 0,
	ME_OGG_CMD_OPEN = 1,
	ME_OGG_CMD_CLOSE = 2,
	ME_OGG_CMD_DECODE = 3,
	ME_OGG_CMD_SEEK_START = 4,
	ME_OGG_CMD_SEEK_SAMPLE = 5,
};

enum
{
	ME_OGG_STATUS_OK = 0,
	ME_OGG_STATUS_ERROR = 1,
};

enum
{
	ME_OGG_MAX_DECODE_FRAMES = 4096,
	ME_OGG_OUTPUT_CHANNELS = 2,
};

typedef struct MeOggShared
{
	volatile uint32_t request_seq;
	volatile uint32_t response_seq;
	volatile uint32_t command;
	volatile uint32_t status;

	volatile uint32_t input_ptr;
	volatile uint32_t input_size;
	volatile uint32_t arg0;

	volatile int32_t info_channels;
	volatile int32_t info_sample_rate;
	volatile int32_t decode_channels;
	volatile int32_t decode_frames;

	volatile short
	    decode_pcm[ME_OGG_MAX_DECODE_FRAMES * ME_OGG_OUTPUT_CHANNELS];
} MeOggShared;

static volatile MeOggShared *g_shared;
static int g_me_ready;
static int g_me_init_result;
static int g_me_init_attempted;
static int g_me_broken;

static uint32_t make_uncached_ptr(void const *ptr)
{
	uint32_t address = (uint32_t)(uintptr_t)ptr;
	return address | UNCACHED_USER_MASK;
}

static int me_submit_request(void)
{
	if (g_shared == NULL || !g_me_ready || g_me_broken) {
		return 0;
	}

	uint32_t const request = g_shared->request_seq + 1;
	g_shared->request_seq = request;
	asm volatile("sync" ::: "memory");

	uint32_t const start_us = sceKernelGetSystemTimeLow();
	while (g_shared->response_seq != request) {
		if ((sceKernelGetSystemTimeLow() - start_us) > 20000u) {
			g_me_broken = 1;
			g_me_ready = 0;
			return 0;
		}
		sceKernelDelayThread(10);
	}

	asm volatile("sync" ::: "memory");
	return g_shared->status == ME_OGG_STATUS_OK;
}

void meLibOnProcess(void)
{
	stb_vorbis *decoder = NULL;

	while (1) {
		if (g_shared == NULL) {
			meLibDelayPipeline();
			continue;
		}

		uint32_t const request = g_shared->request_seq;
		if (request == g_shared->response_seq) {
			meLibDelayPipeline();
			continue;
		}

		uint32_t const command = g_shared->command;
		g_shared->status = ME_OGG_STATUS_OK;

		if (command == ME_OGG_CMD_OPEN) {
			if (decoder != NULL) {
				stb_vorbis_close(decoder);
				decoder = NULL;
			}

			int error = 0;
			decoder = stb_vorbis_open_memory(
			    (unsigned char const *)(uintptr_t)g_shared->input_ptr,
			    (int)g_shared->input_size,
			    &error,
			    NULL);

			if (decoder == NULL) {
				g_shared->status = ME_OGG_STATUS_ERROR;
			} else {
				stb_vorbis_info const info = stb_vorbis_get_info(decoder);
				g_shared->info_channels = (int32_t)info.channels;
				g_shared->info_sample_rate = (int32_t)info.sample_rate;
			}
		} else if (command == ME_OGG_CMD_CLOSE) {
			if (decoder != NULL) {
				stb_vorbis_close(decoder);
				decoder = NULL;
			}
		} else if (command == ME_OGG_CMD_DECODE) {
			if (decoder == NULL) {
				g_shared->decode_frames = 0;
				g_shared->status = ME_OGG_STATUS_ERROR;
			} else {
				int channels = (int)g_shared->decode_channels;
				int frames = (int)g_shared->arg0;
				if (channels < 1 || channels > 2 || frames < 1
				    || frames > ME_OGG_MAX_DECODE_FRAMES) {
					g_shared->decode_frames = 0;
					g_shared->status = ME_OGG_STATUS_ERROR;
				} else {
					int const decoded
					    = stb_vorbis_get_samples_short_interleaved(decoder,
					        channels,
					        (short *)(void *)g_shared->decode_pcm,
					        frames * channels);
					g_shared->decode_frames = (int32_t)decoded;
				}
			}
		} else if (command == ME_OGG_CMD_SEEK_START) {
			if (decoder == NULL || stb_vorbis_seek_start(decoder) == 0) {
				g_shared->status = ME_OGG_STATUS_ERROR;
			}
		} else if (command == ME_OGG_CMD_SEEK_SAMPLE) {
			if (decoder == NULL
			    || stb_vorbis_seek(decoder, (unsigned int)g_shared->arg0)
			        == 0) {
				g_shared->status = ME_OGG_STATUS_ERROR;
			}
		} else {
			g_shared->status = ME_OGG_STATUS_ERROR;
		}

		asm volatile("sync" ::: "memory");
		g_shared->response_seq = request;
		asm volatile("sync" ::: "memory");
	}
}

bool psp_me_ogg_initialize(void)
{
	if (g_me_ready && !g_me_broken) {
		return true;
	}
	if (g_me_broken) {
		return false;
	}

	if (g_shared == NULL) {
		volatile u32Me *shared_words = NULL;
		uint32_t const words
		    = (uint32_t)((sizeof(MeOggShared) + sizeof(u32Me) - 1u)
		        / sizeof(u32Me));
		meLibGetUncached32(&shared_words, words);
		if (shared_words == NULL) {
			return false;
		}
		g_shared = (volatile MeOggShared *)(void *)shared_words;
		memset((void *)g_shared, 0, sizeof(MeOggShared));
	}

	if (!g_me_init_attempted) {
		g_me_init_attempted = 1;
		g_me_init_result = meLibDefaultInit();
		if (g_me_init_result < 0) {
			g_me_broken = 1;
			return false;
		}
	} else if (g_me_init_result < 0) {
		g_me_broken = 1;
		return false;
	}

	g_me_ready = 1;
	return true;
}

void psp_me_ogg_shutdown(void)
{
	if (g_shared == NULL) {
		return;
	}
	psp_me_ogg_close();
}

bool psp_me_ogg_available(void)
{
	return g_me_ready != 0 && g_me_broken == 0;
}

bool psp_me_ogg_open(uint8_t const *bytes,
    uint32_t const byte_count,
    int *channels,
    int *sample_rate)
{
	if (!psp_me_ogg_initialize() || bytes == NULL || byte_count == 0u) {
		return false;
	}

	sceKernelDcacheWritebackInvalidateRange((void *)bytes, byte_count);

	g_shared->command = ME_OGG_CMD_OPEN;
	g_shared->input_ptr = make_uncached_ptr(bytes);
	g_shared->input_size = byte_count;
	g_shared->arg0 = 0;
	g_shared->decode_frames = 0;

	if (!me_submit_request()) {
		return false;
	}

	if (channels != NULL) {
		*channels = g_shared->info_channels;
	}
	if (sample_rate != NULL) {
		*sample_rate = g_shared->info_sample_rate;
	}

	return g_shared->info_channels >= 1 && g_shared->info_channels <= 2
	    && g_shared->info_sample_rate > 0;
}

void psp_me_ogg_close(void)
{
	if (!psp_me_ogg_available()) {
		return;
	}

	g_shared->command = ME_OGG_CMD_CLOSE;
	g_shared->arg0 = 0;
	(void)me_submit_request();
}

bool psp_me_ogg_decode_interleaved(
    short *output, int const frames, int const channels, int *decoded_frames)
{
	if (!psp_me_ogg_available() || output == NULL || decoded_frames == NULL
	    || frames < 1 || frames > ME_OGG_MAX_DECODE_FRAMES
	    || (channels != 1 && channels != 2)) {
		return false;
	}

	g_shared->command = ME_OGG_CMD_DECODE;
	g_shared->arg0 = (uint32_t)frames;
	g_shared->decode_channels = channels;
	g_shared->decode_frames = 0;

	if (!me_submit_request()) {
		return false;
	}

	int const decoded = g_shared->decode_frames;
	if (decoded < 0 || decoded > frames) {
		return false;
	}

	memcpy(output,
	    (void const *)g_shared->decode_pcm,
	    (size_t)(decoded * channels) * sizeof(short));
	*decoded_frames = decoded;
	return true;
}

bool psp_me_ogg_seek_start(void)
{
	if (!psp_me_ogg_available()) {
		return false;
	}

	g_shared->command = ME_OGG_CMD_SEEK_START;
	g_shared->arg0 = 0;
	return me_submit_request() != 0;
}

bool psp_me_ogg_seek_sample(uint32_t const sample_index)
{
	if (!psp_me_ogg_available()) {
		return false;
	}

	g_shared->command = ME_OGG_CMD_SEEK_SAMPLE;
	g_shared->arg0 = sample_index;
	return me_submit_request() != 0;
}
