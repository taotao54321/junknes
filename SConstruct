# -*- coding: utf-8 -*-

import os

CXXFLAGS_BASE   = ["-std=c++14", "-Wall", "-Wextra"]
CXXFLAGS_OPTDBG = ["-g3", "-O0"]
#CXXFLAGS_OPTDBG = []
#CXXFLAGS_OPTDBG = ["-O2"]
#CXXFLAGS_OPTDBG = ["-O3"]

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

env_main = Environment(
    ENV = {
        "PATH" : os.environ["PATH"], # 自分の sdl2-config を使うため
    },
    variables = vars,
)
env_main.Append(
    CXXFLAGS = CXXFLAGS_BASE + CXXFLAGS_OPTDBG,
)
env_main.ParseConfig("sdl2-config --cflags --libs")
env_main.Requires("junknes", "libjunknes.so")
env_main.Program(
    "junknes-sdl2",
    ["main-sdl2.cpp", "ines.cpp"],
    LIBS = ["SDL2", "junknes"],
    LIBPATH = ["."],
    RPATH = ["."],
)
