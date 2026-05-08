#include <SDL2/SDL.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

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

static uint32_t read_u32_or_default(const uint8_t *data, size_t size, size_t off, uint32_t def) {
    if (off + 4 > size) {
        return def;
    }

    return ((uint32_t)data[off]) |
           ((uint32_t)data[off + 1] << 8) |
           ((uint32_t)data[off + 2] << 16) |
           ((uint32_t)data[off + 3] << 24);
}

static uint8_t *read_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
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

    uint8_t *buf = malloc((size_t)len);
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

static void cleanup(uint8_t *input, uint8_t *wav) {
    free(wav);
    free(input);
}

static uint16_t choose_audio_format(const uint8_t *input, size_t size) {
    if (size < 1) {
        return 1;
    }

    switch (input[0] % 5) {
        case 0: return 1;      /* PCM */
        case 1: return 3;      /* IEEE float */
        case 2: return 6;      /* A-law */
        case 3: return 7;      /* mu-law */
        default: return 1;
    }
}

static uint16_t choose_channels(const uint8_t *input, size_t size) {
    if (size < 2) {
        return 1;
    }

    switch (input[1] % 5) {
        case 0: return 1;
        case 1: return 2;
        case 2: return 4;
        case 3: return 6;
        default: return 8;
    }
}

static uint16_t choose_bits_per_sample(const uint8_t *input, size_t size) {
    if (size < 3) {
        return 16;
    }

    switch (input[2] % 5) {
        case 0: return 8;
        case 1: return 16;
        case 2: return 24;
        case 3: return 32;
        default: return 16;
    }
}

static uint32_t choose_sample_rate(const uint8_t *input, size_t size) {
    static const uint32_t rates[] = {
        8000, 11025, 16000, 22050, 32000, 44100, 48000, 96000, 192000
    };

    if (size < 4) {
        return 44100;
    }

    return rates[input[3] % (sizeof(rates) / sizeof(rates[0]))];
}

static uint8_t *bytes_to_variable_wav(
    const uint8_t *input,
    size_t input_size,
    size_t *out_size
) {
    uint16_t audio_format = choose_audio_format(input, input_size);
    uint16_t channels = choose_channels(input, input_size);
    uint16_t bits_per_sample = choose_bits_per_sample(input, input_size);
    uint32_t sample_rate = choose_sample_rate(input, input_size);

    size_t header_control_size = 8;
    const uint8_t *payload = input;
    size_t payload_size = input_size;

    if (input_size > header_control_size) {
        payload = input + header_control_size;
        payload_size = input_size - header_control_size;
    }

    size_t data_size = payload_size;
    if (data_size % 2 != 0) {
        data_size++;
    }

    if (data_size > UINT32_MAX - 36) {
        return NULL;
    }

    if (data_size > SIZE_MAX - 44) {
        return NULL;
    }

    *out_size = 44 + data_size;

    uint8_t *wav = malloc(*out_size);
    if (!wav) {
        return NULL;
    }

    memset(wav, 0, *out_size);

    uint16_t block_align = (uint16_t)(channels * (bits_per_sample / 8));
    uint32_t byte_rate = sample_rate * block_align;

    memcpy(wav + 0, "RIFF", 4);
    write_u32_le(wav + 4, (uint32_t)(36 + data_size));
    memcpy(wav + 8, "WAVE", 4);

    memcpy(wav + 12, "fmt ", 4);
    write_u32_le(wav + 16, 16);
    write_u16_le(wav + 20, audio_format);
    write_u16_le(wav + 22, channels);
    write_u32_le(wav + 24, sample_rate);
    write_u32_le(wav + 28, byte_rate);
    write_u16_le(wav + 32, block_align);
    write_u16_le(wav + 34, bits_per_sample);

    memcpy(wav + 36, "data", 4);
    write_u32_le(wav + 40, (uint32_t)data_size);

    memcpy(wav + 44, payload, payload_size);

    return wav;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        return 1;
    }

    size_t input_size = 0;
    uint8_t *input = read_file(argv[1], &input_size);
    if (!input) {
        return 1;
    }

    size_t wav_size = 0;
    uint8_t *wav = bytes_to_variable_wav(input, input_size, &wav_size);
    if (!wav) {
        free(input);
        return 0;
    }

    if (wav_size > INT_MAX) {
        cleanup(input, wav);
        return 0;
    }

    SDL_RWops *rw = SDL_RWFromConstMem(wav, (int)wav_size);
    if (!rw) {
        cleanup(input, wav);
        return 0;
    }

    SDL_AudioSpec format;
    Uint8 *audio_buf = NULL;
    Uint32 audio_len = 0;

    if (SDL_LoadWAV_RW(rw, 1, &format, &audio_buf, &audio_len) == NULL) {
        cleanup(input, wav);
        return 0;
    }

    SDL_AudioCVT cvt;

    int cvtres = SDL_BuildAudioCVT(
        &cvt,
        format.format,
        format.channels,
        format.freq,
        AUDIO_S16LSB,
        1,
        47743
    );

    if (cvtres < 0) {
        SDL_FreeWAV(audio_buf);
        cleanup(input, wav);
        return 0;
    }

    if (cvtres > 0) {
        if (audio_len <= INT_MAX &&
            cvt.len_mult > 0 &&
            audio_len <= SIZE_MAX / (size_t)cvt.len_mult) {

            cvt.len = (int)audio_len;
            cvt.buf = malloc((size_t)audio_len * (size_t)cvt.len_mult);

            if (cvt.buf) {
                memcpy(cvt.buf, audio_buf, audio_len);
                SDL_ConvertAudio(&cvt);
                free(cvt.buf);
            }
        }
    }

    SDL_FreeWAV(audio_buf);
    cleanup(input, wav);

    return 0;
}