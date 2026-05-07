#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>


int main(int argc, char **argv) {

    //change depending on how many arguments we want (so far, just filenmae)
    if (argc != 2) {
        fprintf(stderr,"filename missing");
        return 1;
    }

    SDL_AudioSpec format;
    Uint8 *audio_buf;
    Uint32 audio_len;


    //target funciton
    if (SDL_LoadWAV(argv[1], &format, &audio_buf, &audio_len) == NULL) {
        SDL_Quit();
        return 2;
    }

    SDL_AudioCVT cvt;
    int cvtres = SDL_BuildAudioCVT(&cvt,
        format.format, format.channels, format.freq,
        AUDIO_S16LSB, 1, 47743);

    if (cvtres < 0)
    {
        // failure
        SDL_FreeWAV(audio_buf);
        return 3;
    }

    if (cvtres > 0){
        if (SDL_ConvertAudio(&cvt) < 0)
        {
            SDL_FreeWAV(audio_buf);
            return 4;
        }
    }


    SDL_FreeWAV(audio_buf);
    SDL_Quit();
    return 0;
}