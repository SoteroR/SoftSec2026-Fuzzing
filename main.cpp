#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

int main() {
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        return 1;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        return 1;
    }


    Mix_CloseAudio();
    SDL_Quit();
    return 0;
}
