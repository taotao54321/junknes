#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import collections
import itertools
import ctypes
import atexit

import sdl

import junknes
import ines


NES_W = 256
NES_H = 240

SCALE = 2
WIN_W = SCALE * NES_W
WIN_H = SCALE * NES_H

AUDIO_FREQ     = 44100
AUDIO_FORMAT   = sdl.AUDIO_S16SYS
AUDIO_CHANNELS = 1
AUDIO_SAMPLES  = 4096

FPS = 60

InputMapEntry = collections.namedtuple("InputMapEntry", ("key", "port", "button"))
INPUT_MAP = tuple(itertools.starmap(InputMapEntry, (
    (sdl.SDLK_d, 0, junknes.JUNKNES_JOY_R),
    (sdl.SDLK_a, 0, junknes.JUNKNES_JOY_L),
    (sdl.SDLK_s, 0, junknes.JUNKNES_JOY_D),
    (sdl.SDLK_w, 0, junknes.JUNKNES_JOY_U),
    (sdl.SDLK_e, 0, junknes.JUNKNES_JOY_T),
    (sdl.SDLK_q, 0, junknes.JUNKNES_JOY_S),
    (sdl.SDLK_x, 0, junknes.JUNKNES_JOY_B),
    (sdl.SDLK_z, 0, junknes.JUNKNES_JOY_A),
)))

PaletteRgb = junknes.JunknesRgb * 0x40
PALETTE_MASTER = PaletteRgb(*itertools.starmap(junknes.JunknesRgb, (
    ( 0x74, 0x74, 0x74 ), ( 0x24, 0x18, 0x8C ), ( 0x00, 0x00, 0xA8 ), ( 0x44, 0x00, 0x9C ),
    ( 0x8C, 0x00, 0x74 ), ( 0xA8, 0x00, 0x10 ), ( 0xA4, 0x00, 0x00 ), ( 0x7C, 0x08, 0x00 ),
    ( 0x40, 0x2C, 0x00 ), ( 0x00, 0x44, 0x00 ), ( 0x00, 0x50, 0x00 ), ( 0x00, 0x3C, 0x14 ),
    ( 0x18, 0x3C, 0x5C ), ( 0x00, 0x00, 0x00 ), ( 0x00, 0x00, 0x00 ), ( 0x00, 0x00, 0x00 ),

    ( 0xBC, 0xBC, 0xBC ), ( 0x00, 0x70, 0xEC ), ( 0x20, 0x38, 0xEC ), ( 0x80, 0x00, 0xF0 ),
    ( 0xBC, 0x00, 0xBC ), ( 0xE4, 0x00, 0x58 ), ( 0xD8, 0x28, 0x00 ), ( 0xC8, 0x4C, 0x0C ),
    ( 0x88, 0x70, 0x00 ), ( 0x00, 0x94, 0x00 ), ( 0x00, 0xA8, 0x00 ), ( 0x00, 0x90, 0x38 ),
    ( 0x00, 0x80, 0x88 ), ( 0x00, 0x00, 0x00 ), ( 0x00, 0x00, 0x00 ), ( 0x00, 0x00, 0x00 ),

    ( 0xFC, 0xFC, 0xFC ), ( 0x3C, 0xBC, 0xFC ), ( 0x5C, 0x94, 0xFC ), ( 0xCC, 0x88, 0xFC ),
    ( 0xF4, 0x78, 0xFC ), ( 0xFC, 0x74, 0xB4 ), ( 0xFC, 0x74, 0x60 ), ( 0xFC, 0x98, 0x38 ),
    ( 0xF0, 0xBC, 0x3C ), ( 0x80, 0xD0, 0x10 ), ( 0x4C, 0xDC, 0x48 ), ( 0x58, 0xF8, 0x98 ),
    ( 0x00, 0xE8, 0xD8 ), ( 0x78, 0x78, 0x78 ), ( 0x00, 0x00, 0x00 ), ( 0x00, 0x00, 0x00 ),

    ( 0xFC, 0xFC, 0xFC ), ( 0xA8, 0xE4, 0xFC ), ( 0xC4, 0xD4, 0xFC ), ( 0xD4, 0xC8, 0xFC ),
    ( 0xFC, 0xC4, 0xFC ), ( 0xFC, 0xC4, 0xD8 ), ( 0xFC, 0xBC, 0xB0 ), ( 0xFC, 0xD8, 0xA8 ),
    ( 0xFC, 0xE4, 0xA0 ), ( 0xE0, 0xFC, 0xA0 ), ( 0xA8, 0xF0, 0xBC ), ( 0xB0, 0xFC, 0xCC ),
    ( 0x9C, 0xFC, 0xF0 ), ( 0xC4, 0xC4, 0xC4 ), ( 0x00, 0x00, 0x00 ), ( 0x00, 0x00, 0x00 )
)))


class SDLError(Exception):
    def __init__(self, msg):
        msg_all = "{}: {}".format(msg, sdl.SDL_GetError().decode(errors="replace"))
        super().__init__(msg_all)

def error(msg):
    sys.exit(msg)

def get_input():
    inputs = [0, 0]
    sdl.SDL_PumpEvents()
    keys = sdl.SDL_GetKeyState(None)
    for m in INPUT_MAP:
        if keys[m.key]: inputs[m.port] |= m.button
    return inputs

def draw(blit, surf, buf):
    # SDL_MUSTLOCK() が使えないのでとりあえず無条件でロック
    # TODO: ただのソフトウェアサーフェスならロック不要?
    if sdl.SDL_LockSurface(surf) != 0: raise SDLError("SDL_LockSurface()")
    junknes.junknes_blit_do(blit, buf, surf.contents.pixels, SCALE)
    sdl.SDL_UnlockSurface(surf)

def usage():
    error("main-sdl1 <INES>")

def main():
    if len(sys.argv) != 2: usage()
    prg, chr_, mirror = ines.ines_split(open(sys.argv[1], "rb"))

    if(sdl.SDL_Init(sdl.SDL_INIT_VIDEO | sdl.SDL_INIT_TIMER) != 0):
        raise SDLError("SDL_Init()")
    atexit.register(sdl.SDL_Quit)

    screen = sdl.SDL_SetVideoMode(WIN_W, WIN_H, 32, sdl.SDL_SWSURFACE)
    if not screen: raise SDLError("SDL_SetVideoMode()")
    assert screen.contents.format.contents.BytesPerPixel == 4


    blit = junknes.junknes_blit_create(PALETTE_MASTER, junknes.JUNKNES_PIXEL_XRGB8888)
    if not blit: error("junknes_blit_create() failed")
    mixer = junknes.junknes_mixer_create(AUDIO_FREQ, AUDIO_SAMPLES, FPS)
    if not mixer: error("junknes_mixer_create() failed")


    want = sdl.SDL_AudioSpec()
    want.freq = AUDIO_FREQ
    want.format = AUDIO_FORMAT
    want.channels = AUDIO_CHANNELS
    want.samples = AUDIO_SAMPLES
    want.callback = ctypes.cast(junknes.junknes_mixer_pull_sdl, sdl.SDL_AudioCallback)
    want.userdata = ctypes.cast(mixer, ctypes.c_void_p)

    have = sdl.SDL_AudioSpec()
    if sdl.SDL_OpenAudio(ctypes.byref(want), ctypes.byref(have)) != 0:
        raise SDLError("SDL_OpenAudio() failed")

    if want.freq     != have.freq or\
       want.format   != have.format or\
       want.channels != have.channels: error("spec changed")


    prg_ary = (0x8000*ctypes.c_uint8).from_buffer_copy(prg)
    chr_ary = (0x2000*ctypes.c_uint8).from_buffer_copy(chr_)
    nes = junknes.junknes_create(prg_ary, chr_ary, mirror)

    sdl.SDL_PauseAudio(0)

    start_ms = sdl.SDL_GetTicks()
    frame = 0
    running = True
    while running:
        ev = sdl.SDL_Event()
        while sdl.SDL_PollEvent(ctypes.byref(ev)):
            if (ev.type == sdl.SDL_QUIT or
                (ev.type == sdl.SDL_KEYDOWN and ev.key.keysym.sym == sdl.SDLK_ESCAPE)):
                running = False

        inputs = get_input()
        for i, value in enumerate(inputs):
            junknes.junknes_set_input(nes, i, value)

        junknes.junknes_emulate_frame(nes)

        sound = junknes.JunknesSound()
        junknes.junknes_sound(nes, ctypes.byref(sound))
        junknes.junknes_mixer_push(mixer, ctypes.byref(sound))

        draw(blit, screen, junknes.junknes_screen(nes))
        sdl.SDL_UpdateRect(screen, 0, 0, 0, 0)

        frame += 1
        if frame == 1000:
            end_ms = sdl.SDL_GetTicks()
            if end_ms == start_ms: end_ms += 1
            print("fps: {:.2f}".format(1000.0 * frame / (end_ms-start_ms)))
            start_ms = end_ms
            frame = 0

        sdl.SDL_Delay(7)

    junknes.junknes_destroy(nes)

    sdl.SDL_CloseAudio()

    junknes.junknes_mixer_destroy(mixer)
    junknes.junknes_blit_destroy(blit)


if __name__ == "__main__": main()
