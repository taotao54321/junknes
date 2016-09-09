#include <array>
#include <algorithm>
#include <cstdint>

#include <boost/lockfree/spsc_queue.hpp>

#include "junknes.h"
#include "nes.hpp"

using namespace std;

struct Junknes{
    Junknes(const uint8_t* prg, const uint8_t* chr, JunknesMirroring mirror)
        : impl(prg, chr, mirror) {}
    Nes impl;
};

extern "C" struct Junknes* junknes_create(const uint8_t* prg,
                                          const uint8_t* chr,
                                          enum JunknesMirroring mirror)
{
    if(!(mirror == JUNKNES_MIRROR_H || mirror == JUNKNES_MIRROR_V)) return nullptr;

    return new Junknes(prg, chr, mirror);
}

extern "C" void junknes_destroy(struct Junknes* nes)
{
    delete nes;
}

extern "C" void junknes_hardreset(struct Junknes* nes)
{
    nes->impl.hardReset();
}

extern "C" void junknes_softreset(struct Junknes* nes)
{
    nes->impl.softReset();
}

extern "C" void junknes_emulate_frame(struct Junknes* nes)
{
    nes->impl.emulateFrame();
}

extern "C" void junknes_set_input(struct Junknes* nes, int port, unsigned int input)
{
    if(!(port == 0 || port == 1)) return;

    nes->impl.setInput(port, input);
}

extern "C" const uint8_t* junknes_screen(const struct Junknes* nes)
{
    return nes->impl.screen();
}

extern "C" void junknes_sound(const struct Junknes* nes, struct JunknesSound* sound)
{
    *sound = nes->impl.sound();
}

extern "C" void junknes_before_exec(struct Junknes* nes, JunknesCpuHook hook, void* userdata)
{
    nes->impl.beforeExec(hook, userdata);
}


namespace{
    constexpr int NES_W = 256;
    constexpr int NES_H = 240;
}

struct JunknesBlit{
    JunknesBlit(const array<uint32_t, 0x40>& palette_arg) : palette(palette_arg) {}
    array<uint32_t, 0x40> palette;
};

struct JunknesMixer{
    JunknesMixer(int freq_arg, int bufsize_arg, int fps_arg)
        : freq(freq_arg), bufsize(bufsize_arg), fps(fps_arg), offset(0),
          queue(16*freq/fps) {} // とりあえずキューサイズは多めで
    int freq;
    int bufsize;
    int fps;
    int offset;
    boost::lockfree::spsc_queue<int16_t> queue;
};

namespace{
    void palette_convert_32(const JunknesRgb* master, array<uint32_t, 0x40>& target,
                            int rshift, int gshift, int bshift)
    {
        for(int i = 0; i < 0x40; ++i){
            uint8_t r = master[i].r;
            uint8_t g = master[i].g;
            uint8_t b = master[i].b;
            target[i] = (r<<rshift) | (g<<gshift) | (b<<bshift);
        }
    }
}

extern "C" struct JunknesBlit* junknes_blit_create(const struct JunknesRgb* palette,
                                                   enum JunknesPixelFormat format)
{
    array<uint32_t, 0x40> palette_target;

    int rshift, gshift, bshift;
    switch(format){
    case JUNKNES_PIXEL_XRGB8888:
        rshift = 16;
        gshift =  8;
        bshift =  0;
        break;
    default:
        // not supported
        return nullptr;
    }

    palette_convert_32(palette, palette_target, rshift, gshift, bshift);
    return new JunknesBlit(palette_target);
}

extern "C" void junknes_blit_destroy(struct JunknesBlit* blit)
{
    delete blit;
}

extern "C" void junknes_blit_do(struct JunknesBlit* blit,
                                const uint8_t* src, void* dst, int scale)
{
    if(scale <= 0) return;

    uint32_t* p = reinterpret_cast<uint32_t*>(dst);
    for(int y = 0; y < NES_H; ++y)
        for(int i = 0; i < scale; ++i)
            for(int x = 0; x < NES_W; ++x)
                for(int j = 0; j < scale; ++j)
                    *p++ = blit->palette[src[NES_W*y + x]];
}

extern "C" struct JunknesMixer* junknes_mixer_create(int freq, int bufsize, int fps)
{
    if(freq <= 0) return nullptr;
    if(bufsize <= 0) return nullptr;
    if(fps <= 0) return nullptr;
    return new JunknesMixer(freq, bufsize, fps);
}

extern "C" void junknes_mixer_destroy(struct JunknesMixer* mixer)
{
    delete mixer;
}

extern "C" void junknes_mixer_push(struct JunknesMixer* mixer, const struct JunknesSound* sound)
{
    int len = sound->sq1.len;

    static constexpr double CPU_FREQ = 6.0 * 39375000.0/11.0 / 12.0;
    double step = CPU_FREQ / mixer->freq;

    int n_sample = static_cast<int>((len-mixer->offset) / step);
    // TODO: 1step未満の場合の offset 補正(起こり得ないとは思うが…)
    if(n_sample <= 0) return;

    double pos = mixer->offset;
    int n_push = 0;
    while(n_push < n_sample){
        int pos_int = static_cast<int>(pos);
        uint8_t v_sq1 = sound->sq1.data[pos_int]; // [0, 15]
        uint8_t v_sq2 = sound->sq2.data[pos_int]; // [0, 15]
        uint8_t v_tri = sound->tri.data[pos_int]; // [0, 15]
        uint8_t v_noi = sound->noi.data[pos_int]; // [0, 15]
        uint8_t v_dmc = sound->dmc.data[pos_int]; // [0,127]

        // http://wiki.nesdev.com/w/index.php/APU_Mixer
        double mixed = 0.00752*(v_sq1+v_sq2) + 0.00851*v_tri + 0.00494*v_noi + 0.00335*v_dmc;
        int16_t sample = 20000 * (mixed-0.5);

        if(mixer->queue.push(sample)){
            ++n_push;
            pos += step;
        }
        else{
            // overflow
            break;
        }
    }

    if(n_push == n_sample){
        mixer->offset = static_cast<int>(step - (len-pos));
    }
    else{
        // overflow
        // データを捨てるので offset もリセット
        mixer->offset = 0;
    }
}

extern "C" void junknes_mixer_pull_sdl(void* userdata, uint8_t* stream, int len)
{
    assert(userdata != nullptr);
    assert(!(len&1));

    JunknesMixer* mixer = reinterpret_cast<JunknesMixer*>(userdata);
    unsigned int n_sample = len / 2;

    int16_t* p = reinterpret_cast<int16_t*>(stream);
    size_t n_pop = mixer->queue.pop(p, n_sample);
    if(n_pop < n_sample){
        // underflow
        fill_n(p+n_pop, 0, n_sample-n_pop);
    }
}
