#include <array>
#include <algorithm>
#include <cstdint>

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
