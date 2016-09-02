# -*- coding: utf-8 -*-

import os

vars = Variables(None, ARGUMENTS)
vars.Add("CXX")

env = Environment(
    ENV={
        "PATH" : os.environ["PATH"], # 自分の sdl2-config を使うため
    },
    variables=vars,
)

#env.Append(CXXFLAGS = ["-std=c++14", "-Wall", "-Wextra"])
env.Append(CXXFLAGS = ["-std=c++14", "-Wall", "-Wextra", "-g3", "-O0"])
#env.Append(CXXFLAGS = ["-std=c++14", "-Wall", "-Wextra", "-O2"])
#env.Append(CXXFLAGS = ["-std=c++14", "-Wall", "-Wextra", "-O3"])
env.ParseConfig("sdl2-config --cflags --libs")

env.Program("junknes", Glob("*.cpp"))
