# -*- coding: utf-8 -*-

from ctypes import cdll,\
                   Structure, POINTER, CFUNCTYPE,\
                   c_int, c_uint, c_uint8, c_uint16,\
                   c_void_p

_lib = cdll.LoadLibrary("./libjunknes.so")

def _funcdef(name, res, args):
    func = getattr(_lib, name)
    func.restype  = res
    func.argtypes = args
    return func


JUNKNES_MIRROR_H = 0
JUNKNES_MIRROR_V = 1

JUNKNES_JOY_A = (1<<0)
JUNKNES_JOY_B = (1<<1)
JUNKNES_JOY_S = (1<<2)
JUNKNES_JOY_T = (1<<3)
JUNKNES_JOY_U = (1<<4)
JUNKNES_JOY_D = (1<<5)
JUNKNES_JOY_L = (1<<6)
JUNKNES_JOY_R = (1<<7)

class Junknes(Structure): pass

class JunknesSoundChannel(Structure):
    _fields_ = (
        ("len", c_int),
        ("data", POINTER(c_uint8)),
    )

class JunknesSound(Structure):
    _fields_ = (
        ("sq1", JunknesSoundChannel),
        ("sq2", JunknesSoundChannel),
        ("tri", JunknesSoundChannel),
        ("noi", JunknesSoundChannel),
        ("dmc", JunknesSoundChannel),
    )

class _JunknesCpuStateP(Structure):
    _fields_ = (
        ("C", c_uint8),
        ("Z", c_uint8),
        ("I", c_uint8),
        ("D", c_uint8),
        ("V", c_uint8),
        ("N", c_uint8),
    )

class JunknesCpuState(Structure):
    _fields_ = (
        ("PC", c_uint16),
        ("A", c_uint8),
        ("X", c_uint8),
        ("Y", c_uint8),
        ("S", c_uint8),
        ("P", _JunknesCpuStateP),
    )

JunknesCpuHook = CFUNCTYPE(None, POINTER(JunknesCpuState), c_uint8, c_uint16, c_void_p)

junknes_create = _funcdef("junknes_create",
                          POINTER(Junknes), (POINTER(c_uint8), POINTER(c_uint8), c_int))
junknes_destroy = _funcdef("junknes_destroy", None, (POINTER(Junknes),))

junknes_emulate_frame = _funcdef("junknes_emulate_frame", None, (POINTER(Junknes),))
junknes_set_input = _funcdef("junknes_set_input", None, (POINTER(Junknes), c_int, c_uint))

junknes_screen = _funcdef("junknes_screen", POINTER(c_uint8), (POINTER(Junknes),))
junknes_sound = _funcdef("junknes_sound", None, (POINTER(Junknes), POINTER(JunknesSound)))

junknes_before_exec = _funcdef("junknes_before_exec",
                               None, (POINTER(Junknes), JunknesCpuHook, c_void_p))


JUNKNES_PIXEL_XRGB8888 = 0

class JunknesRgb(Structure):
    _fields_ = (
        ("r", c_uint8),
        ("g", c_uint8),
        ("b", c_uint8),
        ("unused", c_uint8),
    )

class JunknesBlit(Structure): pass
class JunknesMixer(Structure): pass

junknes_blit_create = _funcdef("junknes_blit_create",
                               POINTER(JunknesBlit), (POINTER(JunknesRgb), c_int))
junknes_blit_destroy = _funcdef("junknes_blit_destroy", None, (POINTER(JunknesBlit),))
junknes_blit_do = _funcdef("junknes_blit_do",
                           None, (POINTER(JunknesBlit), POINTER(c_uint8), c_void_p, c_int))

junknes_mixer_create = _funcdef("junknes_mixer_create",
                                POINTER(JunknesMixer), (c_int, c_int, c_int))
junknes_mixer_destroy = _funcdef("junknes_mixer_destroy", None, (POINTER(JunknesMixer),))
junknes_mixer_push = _funcdef("junknes_mixer_push",
                              None, (POINTER(JunknesMixer), POINTER(JunknesSound)))
junknes_mixer_pull_sdl = _funcdef("junknes_mixer_pull_sdl",
                                  None, (c_void_p, POINTER(c_uint8), c_int))


