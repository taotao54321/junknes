/**
 * 基本的なiNES仕様のみ対応
 * マッパー0のみ対応
 */

#pragma once

#include "nes.hpp"

bool ines_split(const char* path, Nes::Prg& prg, Nes::Chr& chr, Nes::NtMirror& mirror);
