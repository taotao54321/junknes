# -*- coding: utf-8 -*-

import os

CXXFLAGS_BASE   = [
    "-std=c++14", "-Wall", "-Wextra",
    "-fdiagnostics-color",
]
CXXFLAGS_OPTDBG = ["-g3", "-O0"]
#CXXFLAGS_OPTDBG = []
#CXXFLAGS_OPTDBG = ["-O2", "-DNDEBUG"]
#CXXFLAGS_OPTDBG = ["-O3", "-DNDEBUG"]

vars = Variables(None, ARGUMENTS)
vars.Add("CXX")

env_lib = Environment(variables=vars)
env_lib.Append(
    CXXFLAGS = CXXFLAGS_BASE + CXXFLAGS_OPTDBG + [
        "-fvisibility=hidden", "-fvisibility-inlines-hidden",
    ],
)
env_lib.SharedLibrary(
    "junknes",
    ["junknes.cpp", "nes.cpp", "cpu.cpp", "ppu.cpp", "apu.cpp"],
)

env_ines = Environment(variables=vars)
env_ines.Append(CXXFLAGS = CXXFLAGS_BASE + CXXFLAGS_OPTDBG)
obj_ines = env_ines.Object("ines.cpp")

env_main_sdl2 = Environment(
    ENV = {
        "PATH" : os.environ["PATH"], # 自分の sdl2-config を使うため
    },
    variables = vars,
)
env_main_sdl2.Append(
    CXXFLAGS = CXXFLAGS_BASE + CXXFLAGS_OPTDBG,
)
env_main_sdl2.ParseConfig("sdl2-config --cflags --libs")
env_main_sdl2.Requires("junknes-sdl2", "libjunknes.so")
env_main_sdl2.Program(
    "junknes-sdl2",
    ["main-sdl2.cpp", obj_ines],
    LIBS = ["SDL2", "junknes"],
    LIBPATH = ["."],
    RPATH = ["."],
)

env_main_sdl1 = Environment(
    ENV = {
        "PATH" : os.environ["PATH"], # 自分の sdl-config を使うため
    },
    variables = vars,
)
env_main_sdl1.Append(
    CXXFLAGS = CXXFLAGS_BASE + CXXFLAGS_OPTDBG,
)
env_main_sdl1.ParseConfig("sdl-config --cflags --libs")
env_main_sdl1.Requires("junknes-sdl1", "libjunknes.so")
env_main_sdl1.Program(
    "junknes-sdl1",
    ["main-sdl1.cpp", obj_ines],
    LIBS = ["SDL", "junknes"],
    LIBPATH = ["."],
    RPATH = ["."],
)
