#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool psp_me_ogg_initialize(void);
void psp_me_ogg_shutdown(void);
bool psp_me_ogg_available(void);

bool psp_me_ogg_open(
    uint8_t const *bytes, uint32_t byte_count, int *channels, int *sample_rate);
void psp_me_ogg_close(void);

bool psp_me_ogg_decode_interleaved(
    short *output, int frames, int channels, int *decoded_frames);

bool psp_me_ogg_seek_start(void);
bool psp_me_ogg_seek_sample(uint32_t sample_index);
#ifdef __cplusplus
}
#endif
