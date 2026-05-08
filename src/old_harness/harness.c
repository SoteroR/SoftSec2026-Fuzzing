#include <SDL2/SDL.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//diavlo
#define SAMPLE_RATE 44100
#define CHANNELS 1
#define BITS_PER_SAMPLE 16

static void write_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void write_u16_le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static uint8_t *read_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    long len = ftell(f);
    if (len <= 0) {
        fclose(f);
        return NULL;
    }

    rewind(f);

    uint8_t *buf = (uint8_t *)malloc((size_t)len);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);

    *out_size = (size_t)len;
    return buf;
}

static uint8_t *bytes_to_wav_from_memory(
    const uint8_t *input,
    size_t input_size,
    size_t *out_size
) {
    size_t data_size = input_size;

    if (data_size % 2 != 0) {
        data_size++;
    }

    *out_size = 44 + data_size;

    uint8_t *wav = (uint8_t *)malloc(*out_size);
    if (!wav) {
        return NULL;
    }

    memset(wav, 0, *out_size);

    uint32_t byte_rate =
        SAMPLE_RATE * CHANNELS * (BITS_PER_SAMPLE / 8);

    uint16_t block_align =
        CHANNELS * (BITS_PER_SAMPLE / 8);

    memcpy(wav + 0, "RIFF", 4);
    write_u32_le(wav + 4, (uint32_t)(36 + data_size));
    memcpy(wav + 8, "WAVE", 4);

    memcpy(wav + 12, "fmt ", 4);
    write_u32_le(wav + 16, 16);
    write_u16_le(wav + 20, 1);
    write_u16_le(wav + 22, CHANNELS);
    write_u32_le(wav + 24, SAMPLE_RATE);
    write_u32_le(wav + 28, byte_rate);
    write_u16_le(wav + 32, block_align);
    write_u16_le(wav + 34, BITS_PER_SAMPLE);

    memcpy(wav + 36, "data", 4);
    write_u32_le(wav + 40, (uint32_t)data_size);

    memcpy(wav + 44, input, input_size);

    return wav;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        return 1;
    }

    size_t input_size = 0;
    uint8_t *input = read_file(argv[1], &input_size);

    if (!input) {
        SDL_Quit();
        return 1;
    }

    uint8_t *data_for_sdl = NULL;
    size_t data_for_sdl_size = 0;
    int should_free_data_for_sdl = 0;

    /*
        Hybrid fuzzing:

        Half of inputs are passed directly into SDL_LoadWAV_RW.
        This fuzzes the WAV/RIFF parser.

        Half of inputs are wrapped into a valid WAV container.
        This fuzzes SDL audio loading/conversion after parse succeeds.
    */

    if ((input[0] & 1) == 0) {
        data_for_sdl =
            bytes_to_wav_from_memory(
                input,
                input_size,
                &data_for_sdl_size
            );

        if (!data_for_sdl) {
            free(input);
            SDL_Quit();
            return 1;
        }

        should_free_data_for_sdl = 1;
    } else {
        data_for_sdl = input;
        data_for_sdl_size = input_size;
        should_free_data_for_sdl = 0;
    }

    SDL_RWops *rw =
        SDL_RWFromMem(data_for_sdl, (int)data_for_sdl_size);

    if (!rw) {
        if (should_free_data_for_sdl) {
            free(data_for_sdl);
        }
        free(input);
        SDL_Quit();
        return 1;
    }

    SDL_AudioSpec format;
    Uint8 *audio_buf = NULL;
    Uint32 audio_len = 0;

    if (SDL_LoadWAV_RW(
            rw,
            1,
            &format,
            &audio_buf,
            &audio_len) == NULL) {

        if (should_free_data_for_sdl) {
            free(data_for_sdl);
        }

        free(input);
        SDL_Quit();
        return 0;
    }

    SDL_AudioCVT cvt;

    int cvtres =
        SDL_BuildAudioCVT(
            &cvt,
            format.format,
            format.channels,
            format.freq,
            AUDIO_S16LSB,
            1,
            47743);

    if (cvtres < 0) {
        SDL_FreeWAV(audio_buf);

        if (should_free_data_for_sdl) {
            free(data_for_sdl);
        }

        free(input);
        SDL_Quit();
        return 0;
    }

    if (cvtres > 0) {
        cvt.len = (int)audio_len;

        cvt.buf =
            (Uint8 *)malloc((size_t)audio_len * cvt.len_mult);

        if (cvt.buf) {
            memcpy(cvt.buf, audio_buf, audio_len);
            SDL_ConvertAudio(&cvt);
            free(cvt.buf);
        }
    }

    SDL_FreeWAV(audio_buf);

    if (should_free_data_for_sdl) {
        free(data_for_sdl);
    }

    free(input);
    SDL_Quit();

    return 0;
}