#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cassert>

#include <SDL.h>

#include "junknes.h"
#include "ines.hpp"
#include "util.hpp"

using namespace std;

namespace{
    constexpr int FPS = 60;

    struct InputMap{
        int scancode;
        int port;
        unsigned int button;
    };
    constexpr InputMap INPUT_MAP[] = {
        { SDLK_d, 0, JUNKNES_JOY_R },
        { SDLK_a, 0, JUNKNES_JOY_L },
        { SDLK_s, 0, JUNKNES_JOY_D },
        { SDLK_w, 0, JUNKNES_JOY_U },
        { SDLK_e, 0, JUNKNES_JOY_T },
        { SDLK_q, 0, JUNKNES_JOY_S },
        { SDLK_x, 0, JUNKNES_JOY_B },
        { SDLK_z, 0, JUNKNES_JOY_A },
    };

    constexpr int      AUDIO_FREQ     = 44100;
    constexpr uint16_t AUDIO_FORMAT   = AUDIO_S16SYS;
    constexpr int      AUDIO_CHANNELS = 1;
    constexpr int      AUDIO_SAMPLES  = 4096;

    constexpr int NES_W = 256;
    constexpr int NES_H = 240;
    constexpr int SCALE = 2;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    constexpr JunknesRgb PALETTE_MASTER[0x40] = {
        { 0x74, 0x74, 0x74 }, { 0x24, 0x18, 0x8C }, { 0x00, 0x00, 0xA8 }, { 0x44, 0x00, 0x9C },
        { 0x8C, 0x00, 0x74 }, { 0xA8, 0x00, 0x10 }, { 0xA4, 0x00, 0x00 }, { 0x7C, 0x08, 0x00 },
        { 0x40, 0x2C, 0x00 }, { 0x00, 0x44, 0x00 }, { 0x00, 0x50, 0x00 }, { 0x00, 0x3C, 0x14 },
        { 0x18, 0x3C, 0x5C }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00 },

        { 0xBC, 0xBC, 0xBC }, { 0x00, 0x70, 0xEC }, { 0x20, 0x38, 0xEC }, { 0x80, 0x00, 0xF0 },
        { 0xBC, 0x00, 0xBC }, { 0xE4, 0x00, 0x58 }, { 0xD8, 0x28, 0x00 }, { 0xC8, 0x4C, 0x0C },
        { 0x88, 0x70, 0x00 }, { 0x00, 0x94, 0x00 }, { 0x00, 0xA8, 0x00 }, { 0x00, 0x90, 0x38 },
        { 0x00, 0x80, 0x88 }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00 },

        { 0xFC, 0xFC, 0xFC }, { 0x3C, 0xBC, 0xFC }, { 0x5C, 0x94, 0xFC }, { 0xCC, 0x88, 0xFC },
        { 0xF4, 0x78, 0xFC }, { 0xFC, 0x74, 0xB4 }, { 0xFC, 0x74, 0x60 }, { 0xFC, 0x98, 0x38 },
        { 0xF0, 0xBC, 0x3C }, { 0x80, 0xD0, 0x10 }, { 0x4C, 0xDC, 0x48 }, { 0x58, 0xF8, 0x98 },
        { 0x00, 0xE8, 0xD8 }, { 0x78, 0x78, 0x78 }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00 },

        { 0xFC, 0xFC, 0xFC }, { 0xA8, 0xE4, 0xFC }, { 0xC4, 0xD4, 0xFC }, { 0xD4, 0xC8, 0xFC },
        { 0xFC, 0xC4, 0xFC }, { 0xFC, 0xC4, 0xD8 }, { 0xFC, 0xBC, 0xB0 }, { 0xFC, 0xD8, 0xA8 },
        { 0xFC, 0xE4, 0xA0 }, { 0xE0, 0xFC, 0xA0 }, { 0xA8, 0xF0, 0xBC }, { 0xB0, 0xFC, 0xCC },
        { 0x9C, 0xFC, 0xF0 }, { 0xC4, 0xC4, 0xC4 }, { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x00 }
    };
#pragma GCC diagnostic pop

    void warn(const char* msg)
    {
        fputs(msg, stderr);
        putc('\n', stderr);
    }

    [[noreturn]] void error(const char* msg)
    {
        warn(msg);
        exit(1);
    }

    void print_video_info()
    {
        char driver[1024];
        if(!SDL_VideoDriverName(driver, sizeof(driver))) error("SDL_VideoDriverName() failed");
        const SDL_VideoInfo* info = SDL_GetVideoInfo();

        printf("driver : %s\n", driver);
        printf("  hw_avail   : %u\n", info->hw_available);
        printf("  wm_avail   : %u\n", info->wm_available);
        printf("  blit_hw    : %u\n", info->blit_hw);
        printf("  blit_hw_CC : %u\n", info->blit_hw_CC);
        printf("  blit_hw_A  : %u\n", info->blit_hw_A);
        printf("  blit_sw    : %u\n", info->blit_sw);
        printf("  blit_sw_CC : %u\n", info->blit_sw_CC);
        printf("  blit_sw_A  : %u\n", info->blit_sw_A);
        printf("  blit_fill  : %u\n", info->blit_fill);
        printf("  video_mem  : %u KB\n", info->video_mem);
        printf("  [best pixel format]\n");
        printf("    BitsPerPixel  : %u\n", info->vfmt->BitsPerPixel);
        printf("    BytesPerPixel : %u\n", info->vfmt->BytesPerPixel);
        printf("    Rmask         : 0x%08X\n", info->vfmt->Rmask);
        printf("    Gmask         : 0x%08X\n", info->vfmt->Gmask);
        printf("    Bmask         : 0x%08X\n", info->vfmt->Bmask);
        printf("    Amask         : 0x%08X\n", info->vfmt->Amask);
    }

    void print_surface(const SDL_Surface* surf)
    {
        printf("size : %d x %d\n", surf->w, surf->h);
        printf("pitch : %u\n", surf->pitch);
        printf("flags : 0x%08X\n", surf->flags);
        printf("[pixel format]\n");
        printf("  BitsPerPixel  : %u\n", surf->format->BitsPerPixel);
        printf("  BytesPerPixel : %u\n", surf->format->BytesPerPixel);
        printf("  Rmask         : 0x%08X\n", surf->format->Rmask);
        printf("  Gmask         : 0x%08X\n", surf->format->Gmask);
        printf("  Bmask         : 0x%08X\n", surf->format->Bmask);
        printf("  Amask         : 0x%08X\n", surf->format->Amask);
    }

    void print_audio_spec(const SDL_AudioSpec* spec, bool obtained)
    {
        printf("freq     : %d\n", spec->freq);
        printf("format   : %d\n", spec->format);
        printf("channels : %u\n", spec->channels);
        printf("samples  : %u\n", spec->samples);
        if(obtained){
            printf("silence  : %u\n", spec->silence);
            printf("size     : %u\n", spec->size);
        }
    }

    void input(unsigned int inputs[2])
    {
        SDL_PumpEvents();
        const uint8_t* keys = SDL_GetKeyState(nullptr);

        for(const auto& m : INPUT_MAP){
            if(keys[m.scancode])
                inputs[m.port] |= m.button;
        }
    }

    void draw(JunknesBlit* blit, SDL_Surface* surf, const uint8_t* buf)
    {
        assert(surf->format->BytesPerPixel == 4);

        if(SDL_MUSTLOCK(surf))
            if(SDL_LockSurface(surf) != 0) error("SDL_LockSurface() failed");

        junknes_blit_do(blit, buf, surf->pixels, SCALE);

        if(SDL_MUSTLOCK(surf))
            SDL_UnlockSurface(surf);
    }

    void usage()
    {
        error("Usage: junknes-sdl1 <INES>");
    }
}

int main(int argc, char** argv)
{
    if(argc < 2) usage();
    array<uint8_t, 0x8000> prg;
    array<uint8_t, 0x2000> chr;
    JunknesMirroring mirror;
    if(!ines_split(argv[1], prg, chr, mirror)) error("Cannot load iNES ROM");

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0)
        error("SDL_Init() failed");
    atexit(SDL_Quit);

    puts("[Video]");
    print_video_info();
    puts("");

    SDL_Surface* screen = SDL_SetVideoMode(SCALE*NES_W, SCALE*NES_H, 32, SDL_SWSURFACE);
    if(!screen) error("SDL_SetVideoMode() failed");
    puts("[Screen]");
    print_surface(screen);
    puts("");


    JunknesBlit* blit = junknes_blit_create(PALETTE_MASTER, JUNKNES_PIXEL_XRGB8888);
    if(!blit) error("junknes_bilt_create() failed");
    JunknesMixer* mixer = junknes_mixer_create(AUDIO_FREQ, AUDIO_SAMPLES, FPS);
    if(!mixer) error("junknes_mixer_create() failed");


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    SDL_AudioSpec want = {};
#pragma GCC diagnostic pop
    want.freq     = AUDIO_FREQ;
    want.format   = AUDIO_FORMAT;
    want.channels = AUDIO_CHANNELS;
    want.samples  = AUDIO_SAMPLES;
    want.callback = junknes_mixer_pull_sdl;
    want.userdata = mixer;
    puts("[Desired audio spec]");
    print_audio_spec(&want, false);
    puts("");

    SDL_AudioSpec have;
    if(SDL_OpenAudio(&want, &have) != 0) error("SDL_OpenAudio() failed");

    if(want.freq     != have.freq ||
       want.format   != have.format ||
       want.channels != have.channels) error("spec changed");

    puts("[Obtained audio spec]");
    print_audio_spec(&have, true);
    puts("");


    Junknes* nes = junknes_create(prg.data(), chr.data(), mirror);
    if(!nes) error("junknes_create() failed");

    SDL_PauseAudio(0);

    uint32_t start_ms = SDL_GetTicks();
    int frame = 0;
    bool running = true;
    while(running){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(ev.type == SDL_QUIT ||
               (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE))
                running = false;
        }

        unsigned int inputs[2] = {};
        input(inputs);
        junknes_set_input(nes, 0, inputs[0]);
        junknes_set_input(nes, 1, inputs[1]);

        junknes_emulate_frame(nes);

        JunknesSound sound;
        junknes_sound(nes, &sound);
        junknes_mixer_push(mixer, &sound);

        draw(blit, screen, junknes_screen(nes));
        SDL_UpdateRect(screen, 0, 0, 0, 0);

        ++frame;
        if(frame == 1000){
            uint32_t end_ms = SDL_GetTicks();
            if(end_ms == start_ms) ++end_ms;
            printf("fps: %.2f\n", 1000.0 * frame / (end_ms-start_ms));
            start_ms = end_ms;
            frame = 0;
        }

        SDL_Delay(7);
    }

    junknes_destroy(nes);

    SDL_CloseAudio();

    junknes_mixer_destroy(mixer);
    junknes_blit_destroy(blit);

    return 0;
}
