# -*- coding: utf-8 -*-

import sys

from ctypes import cdll,\
                   Structure, Union, POINTER, CFUNCTYPE,\
                   c_int, c_int32, c_uint8, c_uint16, c_uint32,\
                   c_void_p, c_char_p

_lib = cdll.LoadLibrary("libSDL.so")

def _funcdef(name, res, args):
    func = getattr(_lib, name)
    func.restype  = res
    func.argtypes = args
    return func


SDL_INIT_TIMER    = 0x00000001
SDL_INIT_AUDIO    = 0x00000010
SDL_INIT_VIDEO    = 0x00000020
SDL_INIT_JOYSTICK = 0x00000200


SDL_SWSURFACE = 0x00000000


AUDIO_S16LSB = 0x8010
AUDIO_S16MSB = 0x9010
AUDIO_S16SYS = AUDIO_S16LSB if sys.byteorder == "little" else AUDIO_S16MSB


SDL_KEYDOWN =  2
SDL_KEYUP   =  3
SDL_QUIT    = 12


SDLK_UNKNOWN            = 0
SDLK_FIRST              = 0
SDLK_BACKSPACE          = 8
SDLK_TAB                = 9
SDLK_CLEAR              = 12
SDLK_RETURN             = 13
SDLK_PAUSE              = 19
SDLK_ESCAPE             = 27
SDLK_SPACE              = 32
SDLK_EXCLAIM            = 33
SDLK_QUOTEDBL           = 34
SDLK_HASH               = 35
SDLK_DOLLAR             = 36
SDLK_AMPERSAND          = 38
SDLK_QUOTE              = 39
SDLK_LEFTPAREN          = 40
SDLK_RIGHTPAREN         = 41
SDLK_ASTERISK           = 42
SDLK_PLUS               = 43
SDLK_COMMA              = 44
SDLK_MINUS              = 45
SDLK_PERIOD             = 46
SDLK_SLASH              = 47
SDLK_0                  = 48
SDLK_1                  = 49
SDLK_2                  = 50
SDLK_3                  = 51
SDLK_4                  = 52
SDLK_5                  = 53
SDLK_6                  = 54
SDLK_7                  = 55
SDLK_8                  = 56
SDLK_9                  = 57
SDLK_COLON              = 58
SDLK_SEMICOLON          = 59
SDLK_LESS               = 60
SDLK_EQUALS             = 61
SDLK_GREATER            = 62
SDLK_QUESTION           = 63
SDLK_AT                 = 64

# Skip uppercase letters

SDLK_LEFTBRACKET        = 91
SDLK_BACKSLASH          = 92
SDLK_RIGHTBRACKET       = 93
SDLK_CARET              = 94
SDLK_UNDERSCORE         = 95
SDLK_BACKQUOTE          = 96
SDLK_a                  = 97
SDLK_b                  = 98
SDLK_c                  = 99
SDLK_d                  = 100
SDLK_e                  = 101
SDLK_f                  = 102
SDLK_g                  = 103
SDLK_h                  = 104
SDLK_i                  = 105
SDLK_j                  = 106
SDLK_k                  = 107
SDLK_l                  = 108
SDLK_m                  = 109
SDLK_n                  = 110
SDLK_o                  = 111
SDLK_p                  = 112
SDLK_q                  = 113
SDLK_r                  = 114
SDLK_s                  = 115
SDLK_t                  = 116
SDLK_u                  = 117
SDLK_v                  = 118
SDLK_w                  = 119
SDLK_x                  = 120
SDLK_y                  = 121
SDLK_z                  = 122
SDLK_DELETE             = 127
# End of ASCII mapped keysyms

# International keyboard syms
SDLK_WORLD_0            = 160 # 0xA0
SDLK_WORLD_1            = 161
SDLK_WORLD_2            = 162
SDLK_WORLD_3            = 163
SDLK_WORLD_4            = 164
SDLK_WORLD_5            = 165
SDLK_WORLD_6            = 166
SDLK_WORLD_7            = 167
SDLK_WORLD_8            = 168
SDLK_WORLD_9            = 169
SDLK_WORLD_10           = 170
SDLK_WORLD_11           = 171
SDLK_WORLD_12           = 172
SDLK_WORLD_13           = 173
SDLK_WORLD_14           = 174
SDLK_WORLD_15           = 175
SDLK_WORLD_16           = 176
SDLK_WORLD_17           = 177
SDLK_WORLD_18           = 178
SDLK_WORLD_19           = 179
SDLK_WORLD_20           = 180
SDLK_WORLD_21           = 181
SDLK_WORLD_22           = 182
SDLK_WORLD_23           = 183
SDLK_WORLD_24           = 184
SDLK_WORLD_25           = 185
SDLK_WORLD_26           = 186
SDLK_WORLD_27           = 187
SDLK_WORLD_28           = 188
SDLK_WORLD_29           = 189
SDLK_WORLD_30           = 190
SDLK_WORLD_31           = 191
SDLK_WORLD_32           = 192
SDLK_WORLD_33           = 193
SDLK_WORLD_34           = 194
SDLK_WORLD_35           = 195
SDLK_WORLD_36           = 196
SDLK_WORLD_37           = 197
SDLK_WORLD_38           = 198
SDLK_WORLD_39           = 199
SDLK_WORLD_40           = 200
SDLK_WORLD_41           = 201
SDLK_WORLD_42           = 202
SDLK_WORLD_43           = 203
SDLK_WORLD_44           = 204
SDLK_WORLD_45           = 205
SDLK_WORLD_46           = 206
SDLK_WORLD_47           = 207
SDLK_WORLD_48           = 208
SDLK_WORLD_49           = 209
SDLK_WORLD_50           = 210
SDLK_WORLD_51           = 211
SDLK_WORLD_52           = 212
SDLK_WORLD_53           = 213
SDLK_WORLD_54           = 214
SDLK_WORLD_55           = 215
SDLK_WORLD_56           = 216
SDLK_WORLD_57           = 217
SDLK_WORLD_58           = 218
SDLK_WORLD_59           = 219
SDLK_WORLD_60           = 220
SDLK_WORLD_61           = 221
SDLK_WORLD_62           = 222
SDLK_WORLD_63           = 223
SDLK_WORLD_64           = 224
SDLK_WORLD_65           = 225
SDLK_WORLD_66           = 226
SDLK_WORLD_67           = 227
SDLK_WORLD_68           = 228
SDLK_WORLD_69           = 229
SDLK_WORLD_70           = 230
SDLK_WORLD_71           = 231
SDLK_WORLD_72           = 232
SDLK_WORLD_73           = 233
SDLK_WORLD_74           = 234
SDLK_WORLD_75           = 235
SDLK_WORLD_76           = 236
SDLK_WORLD_77           = 237
SDLK_WORLD_78           = 238
SDLK_WORLD_79           = 239
SDLK_WORLD_80           = 240
SDLK_WORLD_81           = 241
SDLK_WORLD_82           = 242
SDLK_WORLD_83           = 243
SDLK_WORLD_84           = 244
SDLK_WORLD_85           = 245
SDLK_WORLD_86           = 246
SDLK_WORLD_87           = 247
SDLK_WORLD_88           = 248
SDLK_WORLD_89           = 249
SDLK_WORLD_90           = 250
SDLK_WORLD_91           = 251
SDLK_WORLD_92           = 252
SDLK_WORLD_93           = 253
SDLK_WORLD_94           = 254
SDLK_WORLD_95           = 255 # 0xFF

# Numeric keypad
SDLK_KP0                = 256
SDLK_KP1                = 257
SDLK_KP2                = 258
SDLK_KP3                = 259
SDLK_KP4                = 260
SDLK_KP5                = 261
SDLK_KP6                = 262
SDLK_KP7                = 263
SDLK_KP8                = 264
SDLK_KP9                = 265
SDLK_KP_PERIOD          = 266
SDLK_KP_DIVIDE          = 267
SDLK_KP_MULTIPLY        = 268
SDLK_KP_MINUS           = 269
SDLK_KP_PLUS            = 270
SDLK_KP_ENTER           = 271
SDLK_KP_EQUALS          = 272

# Arrows + Home/End pad
SDLK_UP                 = 273
SDLK_DOWN               = 274
SDLK_RIGHT              = 275
SDLK_LEFT               = 276
SDLK_INSERT             = 277
SDLK_HOME               = 278
SDLK_END                = 279
SDLK_PAGEUP             = 280
SDLK_PAGEDOWN           = 281

# Function keys
SDLK_F1                 = 282
SDLK_F2                 = 283
SDLK_F3                 = 284
SDLK_F4                 = 285
SDLK_F5                 = 286
SDLK_F6                 = 287
SDLK_F7                 = 288
SDLK_F8                 = 289
SDLK_F9                 = 290
SDLK_F10                = 291
SDLK_F11                = 292
SDLK_F12                = 293
SDLK_F13                = 294
SDLK_F14                = 295
SDLK_F15                = 296

# Key state modifier keys
SDLK_NUMLOCK            = 300
SDLK_CAPSLOCK           = 301
SDLK_SCROLLOCK          = 302
SDLK_RSHIFT             = 303
SDLK_LSHIFT             = 304
SDLK_RCTRL              = 305
SDLK_LCTRL              = 306
SDLK_RALT               = 307
SDLK_LALT               = 308
SDLK_RMETA              = 309
SDLK_LMETA              = 310
SDLK_LSUPER             = 311 # Left "Windows" key
SDLK_RSUPER             = 312 # Right "Windows" key
SDLK_MODE               = 313 # "Alt Gr" key
SDLK_COMPOSE            = 314 # Multi-key compose key

# Miscellaneous function keys
SDLK_HELP               = 315
SDLK_PRINT              = 316
SDLK_SYSREQ             = 317
SDLK_BREAK              = 318
SDLK_MENU               = 319
SDLK_POWER              = 320 # Power Macintosh power key
SDLK_EURO               = 321 # Some european keyboards
SDLK_UNDO               = 322 # Atari keyboard has Undo

# Add any other keys here
SDLK_LAST               = 323


class SDL_Color(Structure):
    _fields_ = (
        ("r", c_uint8),
        ("g", c_uint8),
        ("b", c_uint8),
        ("unused", c_uint8),
    )

class SDL_Palette(Structure):
    _fields_ = (
        ("ncolors", c_int),
        ("colors", POINTER(SDL_Color)),
    )

class SDL_PixelFormat(Structure):
    _fields_ = (
        ("palette", POINTER(SDL_Palette)),
        ("BitsPerPixel", c_uint8),
        ("BytesPerPixel", c_uint8),
        ("Rloss", c_uint8),
        ("Gloss", c_uint8),
        ("Bloss", c_uint8),
        ("Aloss", c_uint8),
        ("Rshift", c_uint8),
        ("Gshift", c_uint8),
        ("Bshift", c_uint8),
        ("Ashift", c_uint8),
        ("Rmask", c_uint32),
        ("Gmask", c_uint32),
        ("Bmask", c_uint32),
        ("Amask", c_uint32),
        ("colorkey", c_uint32),
        ("alpha", c_uint8),
    )

class SDL_Surface(Structure):
    _fields_ = (
        ("flags", c_uint32),
        ("format", POINTER(SDL_PixelFormat)),
        ("w", c_int),
        ("h", c_int),
        ("pitch", c_uint16),
        ("pixels", c_void_p),
    )


SDL_AudioCallback = CFUNCTYPE(None, c_void_p, POINTER(c_uint8), c_int)

class SDL_AudioSpec(Structure):
    _fields_ = (
        ("freq", c_int),
        ("format", c_uint16),
        ("channels", c_uint8),
        ("silence", c_uint8),
        ("samples", c_uint16),
        ("padding", c_uint16),
        ("size", c_uint32),
        ("callback", SDL_AudioCallback),
        ("userdata", c_void_p),
    )


class SDL_keysym(Structure):
    _fields_ = (
        ("scancode", c_uint8),
        ("sym", c_int),
        ("mod", c_int),
        ("unicode", c_uint16),
    )

class SDL_KeyboardEvent(Structure):
    _fields_= (
        ("type", c_uint8),
        ("which", c_uint8),
        ("state", c_uint8),
        ("keysym", SDL_keysym),
    )

class SDL_QuitEvent(Structure):
    _fields_ = (
        ("type", c_uint8),
    )

class SDL_Event(Union):
    _fields_ = (
        ("type", c_uint8),
        ("key", SDL_KeyboardEvent),
        ("quit", SDL_QuitEvent),
    )


SDL_GetError = _funcdef("SDL_GetError", c_char_p, None)

SDL_Init = _funcdef("SDL_Init", c_int, (c_uint32,))
SDL_Quit = _funcdef("SDL_Quit", None, None)

SDL_SetVideoMode = _funcdef("SDL_SetVideoMode",
                            POINTER(SDL_Surface), (c_int, c_int, c_int, c_uint32))
SDL_UpdateRect = _funcdef("SDL_UpdateRect",
                          None, (POINTER(SDL_Surface), c_int32, c_int32, c_int32, c_int32))
SDL_LockSurface = _funcdef("SDL_LockSurface", c_int, (POINTER(SDL_Surface),))
SDL_UnlockSurface = _funcdef("SDL_UnlockSurface", None, (POINTER(SDL_Surface),))

SDL_OpenAudio = _funcdef("SDL_OpenAudio", c_int, (POINTER(SDL_AudioSpec), POINTER(SDL_AudioSpec)))
SDL_CloseAudio = _funcdef("SDL_CloseAudio", None, None)
SDL_PauseAudio = _funcdef("SDL_PauseAudio", None, (c_int,))

SDL_PollEvent = _funcdef("SDL_PollEvent", c_int, (POINTER(SDL_Event),))
SDL_PumpEvents = _funcdef("SDL_PumpEvents", None, None)
SDL_GetKeyState = _funcdef("SDL_GetKeyState", POINTER(c_uint8), (POINTER(c_int),))

SDL_GetTicks = _funcdef("SDL_GetTicks", c_uint32, None)
SDL_Delay = _funcdef("SDL_Delay", None, (c_uint32,))


