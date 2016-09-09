#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>

#define JUNKNES_API __attribute__((visibility("default")))

struct Junknes;

enum JunknesMirroring{
    JUNKNES_MIRROR_H = 0,
    JUNKNES_MIRROR_V = 1
};

enum{
    JUNKNES_JOY_A = (1<<0),
    JUNKNES_JOY_B = (1<<1),
    JUNKNES_JOY_S = (1<<2),
    JUNKNES_JOY_T = (1<<3),
    JUNKNES_JOY_U = (1<<4),
    JUNKNES_JOY_D = (1<<5),
    JUNKNES_JOY_L = (1<<6),
    JUNKNES_JOY_R = (1<<7)
};

struct JunknesSoundChannel{
    int len;
    const uint8_t* data;
};
struct JunknesSound{
    struct JunknesSoundChannel sq1; // [0, 15]
    struct JunknesSoundChannel sq2; // [0, 15]
    struct JunknesSoundChannel tri; // [0, 15]
    struct JunknesSoundChannel noi; // [0, 15]
    struct JunknesSoundChannel dmc; // [0,127]
};

struct JunknesCpuState{
    uint16_t PC;
    uint8_t A;
    uint8_t X;
    uint8_t Y;
    uint8_t S;
    struct{
        uint8_t C;
        uint8_t Z;
        uint8_t I;
        uint8_t D;
        uint8_t V;
        uint8_t N;
    } P;
};
typedef void (*JunknesCpuHook)(const struct JunknesCpuState* state,
                               uint8_t opcode, uint16_t operand,
                               void* userdata);

JUNKNES_API struct Junknes* junknes_create(const uint8_t* prg, // size: 0x8000
                                           const uint8_t* chr, // size: 0x2000
                                           enum JunknesMirroring mirror);
JUNKNES_API void junknes_destroy(struct Junknes* nes);

JUNKNES_API void junknes_emulate_frame(struct Junknes* nes);
JUNKNES_API void junknes_set_input(struct Junknes* nes, int port, unsigned int input);

JUNKNES_API const uint8_t* junknes_screen(const struct Junknes* nes); // size: 256*240
JUNKNES_API void junknes_sound(const struct Junknes* nes, struct JunknesSound* sound);

JUNKNES_API void junknes_before_exec(struct Junknes* nes, JunknesCpuHook hook, void* userdata);


struct JunknesRgb{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t unused;
};
enum JunknesPixelFormat{
    JUNKNES_PIXEL_XRGB8888 = 0,
};
struct JunknesBlit;
struct JunknesMixer;

JUNKNES_API
struct JunknesBlit* junknes_blit_create(const struct JunknesRgb* palette, // size: 0x40
                                        enum JunknesPixelFormat format);
JUNKNES_API void junknes_blit_destroy(struct JunknesBlit* blit);

// 256x240, 整数倍拡大可
JUNKNES_API
void junknes_blit_do(struct JunknesBlit* blit, const uint8_t* src, void* dst, int scale);

// int16_t (native endian), モノラル
JUNKNES_API struct JunknesMixer* junknes_mixer_create(int freq, int bufsize, int fps);
JUNKNES_API void junknes_mixer_destroy(struct JunknesMixer* mixer);

JUNKNES_API void junknes_mixer_push(struct JunknesMixer* mixer, const struct JunknesSound* sound);

// SDLオーディオコールバック関数としてそのまま使える
// 予め JunknesMixer ポインタを userdata として設定しておくこと
JUNKNES_API void junknes_mixer_pull_sdl(void* userdata, uint8_t* stream, int len);


#ifdef __cplusplus
}
#endif
