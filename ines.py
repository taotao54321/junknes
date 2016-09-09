# -*- coding: utf-8 -*-

#---------------------------------------------------------------------
# 基本的なINES仕様のみ対応
# マッパー0のみ対応
#---------------------------------------------------------------------

import junknes

class Error(Exception): pass

def _read_exact(in_, n_bytes):
    buf = in_.read(n_bytes)
    if len(buf) != n_bytes: raise Error("incomplete file")
    return buf

def ines_split(in_):
    INES_MAGIC = b"NES\x1A"

    header = _read_exact(in_, 16)
    if header[:4] != INES_MAGIC: raise Error("INES magic not found")

    prg_count = header[4]
    chr_count = header[5]
    flags     = header[6]
    # ゴミが書かれている場合があるのでByte7は見ない

    if not (1 <= prg_count <= 2): raise Error("invalid prg count: {}".format(prg_count))
    # 一部のテストROMはCHRを持たない
    if not (0 <= chr_count <= 1): raise Error("invalid chr count: {}".format(chr_count))
    if flags & 2: raise Error("sram is not supported")
    if flags & 4: raise Error("trainer is not supported")
    if flags & 8: raise Error("fourscreen is not supported")
    mapper = flags >> 4
    if mapper != 0: raise Error("only mapper 0 is supported")

    # 16K PRGはミラーして32Kに
    prg  = _read_exact(in_, 0x4000*prg_count)
    if prg_count == 1: prg *= 2
    # CHRがなければ0フィル
    chr_ = _read_exact(in_, 0x2000*chr_count) if chr_count else b"\0"*0x2000

    mirror = junknes.JUNKNES_MIRROR_V if (flags&1) else junknes.JUNKNES_MIRROR_H

    return prg, chr_, mirror
