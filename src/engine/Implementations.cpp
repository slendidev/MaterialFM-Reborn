#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#define KB_TEXT_SHAPE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION

#include <kb_text_shape.h>
#include <stb_image.h>
#include <stb_truetype.h>
#include <stb_vorbis.c>

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
