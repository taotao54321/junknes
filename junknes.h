#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>

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

__attribute__((visibility("default")))
struct Junknes* junknes_create(const uint8_t* prg, // size: 0x8000
                               const uint8_t* chr, // size: 0x2000
                               enum JunknesMirroring mirror);

__attribute__((visibility("default")))
void junknes_destroy(struct Junknes* nes);

__attribute__((visibility("default")))
void junknes_emulate_frame(struct Junknes* nes);

__attribute__((visibility("default")))
void junknes_set_input(struct Junknes* nes, int port, unsigned int input);

__attribute__((visibility("default")))
const uint8_t* junknes_screen(const struct Junknes* nes); // size: 256*240

__attribute__((visibility("default")))
void junknes_sound(const struct Junknes* nes, struct JunknesSound* sound);

__attribute__((visibility("default")))
void junknes_before_exec(struct Junknes* nes, JunknesCpuHook hook, void* userdata);


#ifdef __cplusplus
}
#endif
