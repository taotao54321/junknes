/**
 * 基本的なiNES仕様のみ対応
 * マッパー0のみ対応
 */

#pragma once

#include <array>
#include <cstdint>

#include "junknes.h"

bool ines_split(const char* path,
                std::array<std::uint8_t, 0x8000>& prg,
                std::array<std::uint8_t, 0x2000>& chr,
                JunknesMirroring& mirror);
