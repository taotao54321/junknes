#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdint>

#include "nes.hpp"
#include "ines.hpp"
#include "util.hpp"

using namespace std;

namespace{
    constexpr char INES_MAGIC[4] = { 'N', 'E', 'S', '\x1A' };

    union INesFlags{
        uint8_t raw;
        explicit INesFlags(uint8_t value) : raw(value) {}
        BitField8<0>   mirror_v;
        BitField8<1>   sram;
        BitField8<2>   trainer;
        BitField8<3>   mirror_four;
        BitField8<4,4> mapper;
    };
}

bool ines_split(const char* path, Nes::Prg& prg, Nes::Chr& chr, Nes::NtMirror& mirror)
{
    FILE* in = fopen(path, "rb");
    if(!in) return false;

    uint8_t hdr[16];
    if(fread(hdr, 1, sizeof(hdr), in) != sizeof(hdr)) return false;
    if(memcmp(hdr, INES_MAGIC, 4) != 0) return false;

    unsigned int prg_count = hdr[4];
    unsigned int chr_count = hdr[5];
    INesFlags flags(hdr[6]);
    // ゴミが書かれている場合があるのでByte7は見ない

    if(!(prg_count == 1 || prg_count == 2)) return false;
    if(!(chr_count == 0 || chr_count == 1)) return false; // 一部のテストROMはCHRを持たない
    if(flags.sram) return false;
    if(flags.trainer) return false;
    if(flags.mirror_four) return false;
    if(flags.mapper != 0) return false;

    if(fread(prg.data(), 1, 0x4000*prg_count, in) != 0x4000*prg_count) return false;
    if(chr_count)
        if(fread(chr.data(), 1, 0x2000, in) != 0x2000) return false;

    fclose(in);

    // 16K PRGはミラーして32Kにする
    if(prg_count == 1)
        copy_n(prg.begin(), 0x4000, prg.begin()+0x4000);
    // CHRがなければ0で埋める
    if(chr_count == 0)
        chr.fill(0);

    mirror = flags.mirror_v ? Nes::NtMirror::VERT : Nes::NtMirror::HORIZ;

    return true;
}
