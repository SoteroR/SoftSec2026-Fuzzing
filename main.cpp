#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>


int main(int argc, char **argv) {

    //change depending on how many arguments we want (so far, just filenmae)
    if (argc != 2) {
        fprintf(stderr,"filename missing");
        return 2;
    }

    SDL_AudioSpec spec;
    Uint8 *audio_buf;
    Uint32 audio_len;

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        return 1;
    }
    //target funciton
    if (SDL_LoadWAV(argv[1], &spec, &audio_buf, &audio_len) == NULL) {
        SDL_Quit();
        return 1;
    }

    SDL_FreeWAV(audio_buf);
    SDL_Quit();
    return 0;
}