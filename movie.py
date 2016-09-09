# -*- coding: utf-8 -*-

import re

import junknes

COMMAND_SOFTRESET = (1<<0)
COMMAND_HARDRESET = (1<<1)

def fm2_read(in_):
    """input行のみを読む。形式チェックなどはほぼなし"""
    BUTTON_MAP = {
        "R" : junknes.JUNKNES_JOY_R,
        "L" : junknes.JUNKNES_JOY_L,
        "D" : junknes.JUNKNES_JOY_D,
        "U" : junknes.JUNKNES_JOY_U,
        "T" : junknes.JUNKNES_JOY_T,
        "S" : junknes.JUNKNES_JOY_S,
        "B" : junknes.JUNKNES_JOY_B,
        "A" : junknes.JUNKNES_JOY_A,
        "." : 0
    }
    RE_FRAME = re.compile(r"^\|(\d+)\|([RLDUTSBA.]{8})\|([RLDUTSBA.]{8})\|\|$")

    def inp(str_):
        value = 0
        for c in str_:
            value |= BUTTON_MAP[c]
        return value

    movie_ = []
    for line in in_:
        m = RE_FRAME.match(line)
        if not m : continue
        cmd = int(m.group(1))
        inputs = (inp(m.group(2)), inp(m.group(3)))
        movie_.append((cmd, inputs))

    return movie_
