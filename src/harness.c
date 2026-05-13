// fuzz_sdl2_audio_main.c
#include <SDL2/SDL.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT_SIZE   (1 << 20)
#define MAX_PAYLOAD_SIZE (1 << 18)

static int fuzz_one_input(const uint8_t *data, size_t size) {

    //Do not allow excessive small inputs (we need 32 minimum for everything to work correctly)
    //Do not allow excessive big inputs (performance)
    if (size < 32 || size > MAX_INPUT_SIZE) {
        return 0;
    }

    //Initialise audioconverter with 0s
    SDL_AudioCVT cvt;
    memset(&cvt, 0, sizeof(cvt));

    static const SDL_AudioFormat formats[] = {
        AUDIO_U8,
        AUDIO_S8,
        AUDIO_U16LSB,
        AUDIO_S16LSB,
        AUDIO_U16MSB,
        AUDIO_S16MSB,
        AUDIO_U16SYS,
        AUDIO_S16SYS,
        AUDIO_S32LSB,
        AUDIO_S32MSB,
        AUDIO_S32SYS,
        AUDIO_F32LSB,
        AUDIO_F32MSB,
        AUDIO_F32SYS
    };

    static const int freqs[] = {
        1,
        2,
        4000,
        8000,
        11025,
        16000,
        22050,
        32000,
        44100,
        48000,
        96000,
        192000
    };

    //randomly choose formats and frequencies
    const size_t num_formats =
        sizeof(formats) / sizeof(formats[0]);

    const size_t num_freqs =
        sizeof(freqs) / sizeof(freqs[0]);

    SDL_AudioFormat src_format =
        formats[data[2] % num_formats];

    SDL_AudioFormat dst_format =
        formats[data[3] % num_formats];

    int src_channels = 1 + (data[4] % 8);
    int dst_channels = 1 + (data[5] % 8);

    int src_freq =
        freqs[data[0] % num_freqs];

    int dst_freq =
        freqs[data[1] % num_freqs];



    //Sometimes force same format, channels or frequency
    if (data[7] & 1) {
        dst_format = src_format;
    }

    if (data[7] & 2) {
        dst_channels = src_channels;
    }

    if (data[7] & 4) {
        dst_freq = src_freq;
    }

    //Extract payload

    size_t payload_offset = 8 + (data[6] % 24);

    if (payload_offset >= size) {
        return 0;
    }

    size_t payload_size = size - payload_offset;

    if (payload_size == 0) {
        return 0;
    }

    if (payload_size > MAX_PAYLOAD_SIZE) {
        payload_size = MAX_PAYLOAD_SIZE;
    }

    //Create Audio converter from the random sources to the random destinations

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


    //Prevent excessive allocations, maximum x64
    if (cvt.len_mult <= 0 || cvt.len_mult > 64) {
        return 0;
    }

    //Prevent integer overflow
    if (payload_size >
        ((size_t)INT_MAX / (size_t)cvt.len_mult)) {
        return 0;
    }

    cvt.len = (int)payload_size;

    size_t alloc_size =
        (size_t)cvt.len * (size_t)cvt.len_mult;

    cvt.buf = (Uint8 *)SDL_malloc(alloc_size);

    if (!cvt.buf) {
        return 0;
    }

    memcpy(cvt.buf,
           data + payload_offset,
           payload_size);


    //Apply the conversor
    SDL_ConvertAudio(&cvt);

    SDL_free(cvt.buf);

    //mixer part
    if (payload_size >= 4) {
        //divide payload into 2 audios
        size_t half = payload_size / 2;

        Uint8 *mix_dst = (Uint8 *)SDL_malloc(half);
        Uint8 *mix_src = (Uint8 *)SDL_malloc(half);

        if (mix_dst && mix_src) {
            memcpy(mix_dst,
                   data + payload_offset,
                   half);

            memcpy(mix_src,
                   data + payload_offset + half,
                   half);

            int volume =
                data[8 % size] %
                (SDL_MIX_MAXVOLUME + 1);

            //mix the 2 audios and apply volume
            SDL_MixAudioFormat(
                mix_dst,
                mix_src,
                src_format,
                (Uint32)half,
                volume);
        }

        SDL_free(mix_dst);
        SDL_free(mix_src);
    }

    //Stream part

    //prepare stream comverser
    SDL_AudioStream *stream =
    SDL_NewAudioStream(
        src_format,
        src_channels,
        src_freq,
        dst_format,
        dst_channels,
        dst_freq);

    if (stream) {
        //Add the data to the stream
        SDL_AudioStreamPut(
            stream,
            data + payload_offset,
            (int)payload_size);

        //signal that all data was added
        SDL_AudioStreamFlush(stream);

        //Check if data is available
        int available = SDL_AudioStreamAvailable(stream);

        if (available > 0 && available <= (int)MAX_PAYLOAD_SIZE) {
            //allocate data for the stream data
            Uint8 *out = (Uint8 *)SDL_malloc((size_t)available);

            if (out) {

                //get all data out
                SDL_AudioStreamGet(stream, out, available);
                SDL_free(out);
            }
        }

        SDL_FreeAudioStream(stream);
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        return 0;
    }

    //Initializing SDL
    if (SDL_Init(0) != 0) {
        return 0;
    }

    //Next part is just reading the fuzzer file and storing it in a buffer
    FILE *fp = fopen(argv[1], "rb");

    if (!fp) {
        SDL_Quit();
        return 0;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        SDL_Quit();
        return 0;
    }

    long len = ftell(fp);

    if (len < 0 || len > MAX_INPUT_SIZE) {
        fclose(fp);
        SDL_Quit();
        return 0;
    }

    rewind(fp);

    uint8_t *buf = (uint8_t *)malloc((size_t)len);

    if (!buf) {
        fclose(fp);
        SDL_Quit();
        return 0;
    }

    size_t nread =
        fread(buf, 1, (size_t)len, fp);

    fclose(fp);

    //Reading finish///////////

    //If file read successfully, fuzz the program
    if (nread == (size_t)len) {
        fuzz_one_input(buf, (size_t)len);
    }

    free(buf);

    SDL_Quit();

    return 0;
}