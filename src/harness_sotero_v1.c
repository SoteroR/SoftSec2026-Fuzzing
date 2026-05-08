// fuzz_sdl2_audio_main.c
#include <SDL2/SDL.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT_SIZE (1 << 20)
#define MAX_PAYLOAD_SIZE (1 << 18)

static int fuzz_one_input(const uint8_t *data, size_t size) {
    if (size < 32 || size > MAX_INPUT_SIZE) {
        return 0;
    }

    SDL_AudioCVT cvt;
    memset(&cvt, 0, sizeof(cvt));

    SDL_AudioFormat formats[] = {
        AUDIO_U8,
        AUDIO_S8,
        AUDIO_U16LSB,
        AUDIO_S16LSB,
        AUDIO_U16MSB,
        AUDIO_S16MSB,
        AUDIO_S32LSB,
        AUDIO_S32MSB,
        AUDIO_F32LSB,
        AUDIO_F32MSB
    };

    SDL_AudioFormat src_format =
        formats[data[2] % (sizeof(formats) / sizeof(formats[0]))];

    SDL_AudioFormat dst_format =
        formats[data[3] % (sizeof(formats) / sizeof(formats[0]))];

    int src_channels = 1 + (data[4] % 8);
    int dst_channels = 1 + (data[5] % 8);

    int src_freq = 8000 + (data[0] % 96) * 1000;
    int dst_freq = 8000 + (data[1] % 96) * 1000;

    size_t payload_offset = 32;
    size_t payload_size = size - payload_offset;

    if (payload_size > MAX_PAYLOAD_SIZE) {
        payload_size = MAX_PAYLOAD_SIZE;
    }

    if (SDL_BuildAudioCVT(
            &cvt,
            src_format,
            src_channels,
            src_freq,
            dst_format,
            dst_channels,
            dst_freq) < 0) {
        return 0;
    }

    if (cvt.len_mult <= 0 || cvt.len_mult > 64) {
        return 0;
    }

    if (payload_size > ((size_t)INT32_MAX / (size_t)cvt.len_mult)) {
        return 0;
    }

    cvt.len = (int)payload_size;
    cvt.buf = (Uint8 *)SDL_malloc((size_t)cvt.len * (size_t)cvt.len_mult);

    if (!cvt.buf) {
        return 0;
    }

    memcpy(cvt.buf, data + payload_offset, payload_size);

    SDL_ConvertAudio(&cvt);

    SDL_free(cvt.buf);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        return 0;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        return 0;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 0;
    }

    long len = ftell(fp);
    if (len < 0 || len > MAX_INPUT_SIZE) {
        fclose(fp);
        return 0;
    }

    rewind(fp);

    uint8_t *buf = (uint8_t *)malloc((size_t)len);
    if (!buf) {
        fclose(fp);
        return 0;
    }

    size_t nread = fread(buf, 1, (size_t)len, fp);
    fclose(fp);

    if (nread == (size_t)len) {
        fuzz_one_input(buf, (size_t)len);
    }

    free(buf);
    return 0;
}