#include <string>
#include <array>
#include <algorithm>
#include <memory>
#include <cstdio>
#include <cstdint>
#include <cassert>

#include "junknes.h"
#include "nes.hpp"
#include "cpu.hpp"
#include "ppu.hpp"
#include "apu.hpp"
#include "util.hpp"

using namespace std;

namespace{
    template<size_t N>
    array<uint8_t, N> my_make_array(const uint8_t* data)
    {
        array<uint8_t, N> ary;
        copy(data, data+N, ary.begin());
        return ary;
    }
}

// mirror の値域チェックはライブラリインターフェース側で行う
Nes::Nes(const uint8_t* prg, const uint8_t* chr, JunknesMirroring mirror)
    : prg_(my_make_array<0x8000>(prg)), chr_(my_make_array<0x2000>(chr)), mirror_(mirror),
      cpu_(make_shared<CpuDoor>(*this)),
      ppu_(make_shared<PpuDoor>(*this)),
      apu_(make_shared<ApuDoor>(*this))
{
    initRW();
    initRWPpu();

    hardReset();
}

void Nes::initRW()
{
    // $0000-$07FF
    fill_n(readers_.begin(), 0x800, &Nes::readRam);
    fill_n(writers_.begin(), 0x800, &Nes::writeRam);

    // $0800-$1FFF
    fill_n(readers_.begin()+0x800, 3*0x800, &Nes::readRamMirror);
    fill_n(writers_.begin()+0x800, 3*0x800, &Nes::writeRamMirror);

    // $2000-$3FFF
    for(uint16_t addr = 0x2000; addr < 0x4000; addr += 8){
        readers_[addr+0] = &Nes::read200x;
        readers_[addr+1] = &Nes::read200x;
        readers_[addr+2] = &Nes::read2002;
        readers_[addr+3] = &Nes::read200x;
        readers_[addr+4] = &Nes::read2004;
        readers_[addr+5] = &Nes::read200x;
        readers_[addr+6] = &Nes::read200x;
        readers_[addr+7] = &Nes::read2007;

        writers_[addr+0] = &Nes::write2000;
        writers_[addr+1] = &Nes::write2001;
        writers_[addr+2] = &Nes::write2002;
        writers_[addr+3] = &Nes::write2003;
        writers_[addr+4] = &Nes::write2004;
        writers_[addr+5] = &Nes::write2005;
        writers_[addr+6] = &Nes::write2006;
        writers_[addr+7] = &Nes::write2007;
    }

    // $4000-$4017
    fill_n(readers_.begin()+0x4000, 0x15, &Nes::readNull);
    readers_[0x4015] = &Nes::read4015;
    readers_[0x4016] = &Nes::read4016;
    readers_[0x4017] = &Nes::read4017;

    writers_[0x4000] = &Nes::write4000;
    writers_[0x4001] = &Nes::write4001;
    writers_[0x4002] = &Nes::write4002;
    writers_[0x4003] = &Nes::write4003;
    writers_[0x4004] = &Nes::write4004;
    writers_[0x4005] = &Nes::write4005;
    writers_[0x4006] = &Nes::write4006;
    writers_[0x4007] = &Nes::write4007;
    writers_[0x4008] = &Nes::write4008;
    writers_[0x4009] = &Nes::writeNull;
    writers_[0x400A] = &Nes::write400A;
    writers_[0x400B] = &Nes::write400B;
    writers_[0x400C] = &Nes::write400C;
    writers_[0x400D] = &Nes::writeNull;
    writers_[0x400E] = &Nes::write400E;
    writers_[0x400F] = &Nes::write400F;
    writers_[0x4010] = &Nes::write4010;
    writers_[0x4011] = &Nes::write4011;
    writers_[0x4012] = &Nes::write4012;
    writers_[0x4013] = &Nes::write4013;
    writers_[0x4014] = &Nes::write4014;
    writers_[0x4015] = &Nes::write4015;
    writers_[0x4016] = &Nes::write4016;
    writers_[0x4017] = &Nes::write4017;

    // $4018-$7FFF
    fill_n(readers_.begin()+0x4018, 0x8000-0x4018, &Nes::readNull);
    fill_n(writers_.begin()+0x4018, 0x8000-0x4018, &Nes::writeNull);

    // $8000-$FFFF
    fill_n(readers_.begin()+0x8000, 0x8000, &Nes::readPrg);
    fill_n(writers_.begin()+0x8000, 0x8000, &Nes::writeNull);
}

void Nes::initRWPpu()
{
    // $0000-$1FFF
    fill_n(readersPpu_.begin(), 0x2000, &Nes::readPpuChr);
    fill_n(writersPpu_.begin(), 0x2000, &Nes::writeNull);

    // $2000-$3EFF
    fill_n(readersPpu_.begin()+0x2000, 0x1F00,
           mirror_ == JUNKNES_MIRROR_H ? &Nes::readPpuVramH : &Nes::readPpuVramV);
    fill_n(writersPpu_.begin()+0x2000, 0x1F00,
           mirror_ == JUNKNES_MIRROR_H ? &Nes::writePpuVramH : &Nes::writePpuVramV);

    // $3F00-$3FFF
    fill_n(readersPpu_.begin()+0x3F00, 0x100, &Nes::readPpuPltram);
    fill_n(writersPpu_.begin()+0x3F00, 0x100, &Nes::writePpuPltram);
}

void Nes::hardReset()
{
    ram_.fill(0);
    vram_.fill(0);

    cpu_.hardReset();
    ppu_.hardReset();
    apu_.hardReset();

    ppuWarmup_ = 2;
    oddFrame_ = false;

    input_.fill(0);
    inputBit_.fill(0);
    inputStrobe_ = false;

    screen_.fill(0);
}

void Nes::softReset()
{
    cpu_.softReset();
    ppu_.softReset();
    apu_.softReset();

    ppuWarmup_ = 2;
    oddFrame_ = false;

    input_.fill(0);
    inputBit_.fill(0);
    inputStrobe_ = false;

    screen_.fill(0);
}

// port の値域チェックはライブラリインターフェース側で行う
void Nes::setInput(int port, unsigned int value)
{
    input_[port] = value;
}

// ライン単位でエミュレート
// タイミングは全てFCEUXと同じ。line 240 (post-render) がフレーム境界
void Nes::emulateFrame()
{
    if(ppuWarmup_){
        apu_.startFrame();
        cpu_.exec(341 * 262);
        apu_.endFrame();
        --ppuWarmup_;
        return;
    }

    apu_.startFrame();

    // line 240 (post-render)
    cpu_.exec(341);

    // line 241
    ppu_.setVBlank(true);
    ppu_.resetOamAddr(); // PPU[3] = PPUSPL = 0
    cpu_.exec(12);
    if(ppu_.nmiEnabled()) triggerNmi();
    cpu_.exec(329);

    // line 242-260
    cpu_.exec(341 * 19);

    // line 261 (pre-render)
    ppu_.setSprOver(false);
    ppu_.setSpr0Hit(false);
    ppu_.setVBlank(false);
    cpu_.exec(325);
    ppu_.reloadAddr(); // if(isRenderingOn()) v = t
    // TODO: ここで以下のコード実行
    //   spork = numsprites = 0;
    //   ResetRL(XBuf);
    cpu_.exec(oddFrame_ ? 15 : 16);
    oddFrame_ ^= 1;

    // line 0-239
    // TODO: ここでframeskip時にspr_overを1にしてるが…
    for(int line = 0; line < 240; ++line){
        // TODO: FCEUXの DoLine() と同じにする
        ppu_.startLine();
        ppu_.doLine(line, screen_.data() + 256*line);
        cpu_.exec(341);
        ppu_.endLine();
    }

    apu_.endFrame();

#if 0
    ppu_.startFrame();
    apu_.startFrame();

    // line 0-239
    for(int line = 0; line < 240; ++line){
        ppu_.startLine();
        ppu_.doLine(line, screen_.data() + 256*line);
        cpu_.exec(341);
        ppu_.endLine();
    }

    // line 240-261 (post-render, VBLANK, pre-render)
    for(int line = 240; line < 262; ++line){
        if(line == 241)
            ppu_.startVBlank();
        cpu_.exec(341);
    }

    apu_.endFrame();
#endif
}

const uint8_t* Nes::screen() const
{
    return screen_.data();
}

JunknesSound Nes::sound() const
{
    return JunknesSound{
        apu_.soundSq1(),
        apu_.soundSq2(),
        apu_.soundTri(),
        apu_.soundNoi(),
        apu_.soundDmc()
    };
}

void Nes::beforeExec(JunknesCpuHook hook, void* userdata)
{
    cpu_.beforeExec(hook, userdata);
}


void Nes::triggerNmi() { cpu_.triggerNmi(); }
void Nes::triggerIrq() { cpu_.triggerIrq(); }

uint8_t Nes::read(uint16_t addr)
{
    return (this->*readers_[addr])(addr);
}

void Nes::write(uint16_t addr, uint8_t value)
{
    (this->*writers_[addr])(addr, value);
}

uint8_t Nes::readPpu(uint16_t addr)
{
    return (this->*readersPpu_[addr])(addr);
}

void Nes::writePpu(uint16_t addr, uint8_t value)
{
    (this->*writersPpu_[addr])(addr, value);
}


uint8_t Nes::readNull(uint16_t) { return 0; }
void Nes::writeNull(uint16_t, uint8_t) {}

uint8_t Nes::readRam(uint16_t addr)
{
    return ram_[addr];
}

void Nes::writeRam(uint16_t addr, uint8_t value)
{
    ram_[addr] = value;
}

uint8_t Nes::readRamMirror(uint16_t addr)
{
    return ram_[addr & 0x7FF];
}

void Nes::writeRamMirror(uint16_t addr, uint8_t value)
{
    ram_[addr & 0x7FF] = value;
}

uint8_t Nes::readPrg(uint16_t addr)
{
    return prg_[addr - 0x8000];
}

uint8_t Nes::read200x(uint16_t) { return ppu_.read200x(); }
uint8_t Nes::read2002(uint16_t) { return ppu_.read2002(); }
uint8_t Nes::read2004(uint16_t) { return ppu_.read2004(); }
uint8_t Nes::read2007(uint16_t) { return ppu_.read2007(); }

void Nes::write2000(uint16_t, uint8_t value) { ppu_.write2000(value); }
void Nes::write2001(uint16_t, uint8_t value) { ppu_.write2001(value); }
void Nes::write2002(uint16_t, uint8_t value) { ppu_.write2002(value); }
void Nes::write2003(uint16_t, uint8_t value) { ppu_.write2003(value); }
void Nes::write2004(uint16_t, uint8_t value) { ppu_.write2004(value); }
void Nes::write2005(uint16_t, uint8_t value) { ppu_.write2005(value); }
void Nes::write2006(uint16_t, uint8_t value) { ppu_.write2006(value); }
void Nes::write2007(uint16_t, uint8_t value) { ppu_.write2007(value); }

void Nes::write4014(uint16_t, uint8_t value)
{
    uint16_t addr_base = value << 8;
    array<uint8_t, 0x100> buf;
    for(int i = 0; i < 0x100; ++i)
        buf[i] = read(addr_base + i);

    cpu_.oamDmaDelay();
    ppu_.oamDma(buf.data());
}

uint8_t Nes::read4015(uint16_t) { return apu_.read4015(); }
void Nes::write4000(uint16_t, uint8_t value) { apu_.write4000(value); }
void Nes::write4001(uint16_t, uint8_t value) { apu_.write4001(value); }
void Nes::write4002(uint16_t, uint8_t value) { apu_.write4002(value); }
void Nes::write4003(uint16_t, uint8_t value) { apu_.write4003(value); }
void Nes::write4004(uint16_t, uint8_t value) { apu_.write4004(value); }
void Nes::write4005(uint16_t, uint8_t value) { apu_.write4005(value); }
void Nes::write4006(uint16_t, uint8_t value) { apu_.write4006(value); }
void Nes::write4007(uint16_t, uint8_t value) { apu_.write4007(value); }
void Nes::write4008(uint16_t, uint8_t value) { apu_.write4008(value); }
void Nes::write400A(uint16_t, uint8_t value) { apu_.write400A(value); }
void Nes::write400B(uint16_t, uint8_t value) { apu_.write400B(value); }
void Nes::write400C(uint16_t, uint8_t value) { apu_.write400C(value); }
void Nes::write400E(uint16_t, uint8_t value) { apu_.write400E(value); }
void Nes::write400F(uint16_t, uint8_t value) { apu_.write400F(value); }
void Nes::write4010(uint16_t, uint8_t value) { apu_.write4010(value); }
void Nes::write4011(uint16_t, uint8_t value) { apu_.write4011(value); }
void Nes::write4012(uint16_t, uint8_t value) { apu_.write4012(value); }
void Nes::write4013(uint16_t, uint8_t value) { apu_.write4013(value); }
void Nes::write4015(uint16_t, uint8_t value) { apu_.write4015(value); }
void Nes::write4017(uint16_t, uint8_t value) { apu_.write4017(value); }

uint8_t Nes::read4016(uint16_t)
{
    // fourscoreでなければこれでよい?
    unsigned int bit = inputBit_[0];
    uint8_t ret = bit >= 8 ? 1 : ((input_[0]>>bit)&1);
    ++inputBit_[0];
    return ret;
}

uint8_t Nes::read4017(uint16_t)
{
    // fourscoreでなければこれでよい?
    unsigned int bit = inputBit_[1];
    uint8_t ret = bit >= 8 ? 1 : ((input_[1]>>bit)&1);
    ++inputBit_[1];
    return ret;
}

void Nes::write4016(uint16_t, uint8_t value)
{
    // 1 がstrobe要求
    // 1, 0 の順で書き込まれたら読めるようになる
    bool strobe_req = value & 1;
    if(inputStrobe_ && !strobe_req)
        inputBit_.fill(0);
    inputStrobe_ = strobe_req;
}

uint8_t Nes::readPpuChr(uint16_t addr)
{
    return chr_[addr];
}

namespace{
    uint16_t vram_addr_horiz(uint16_t addr)
    {
        uint16_t hi = (addr & 0x800) >> 1;
        uint16_t lo = addr & 0x3FF;
        return hi | lo;
    }

    uint16_t vram_addr_vert(uint16_t addr)
    {
        uint16_t hi = addr & 0x400;
        uint16_t lo = addr & 0x3FF;
        return hi | lo;
    }
}

uint8_t Nes::readPpuVramH(uint16_t addr)
{
    return vram_[vram_addr_horiz(addr)];
}

void Nes::writePpuVramH(uint16_t addr, uint8_t value)
{
    vram_[vram_addr_horiz(addr)] = value;
}

uint8_t Nes::readPpuVramV(uint16_t addr)
{
    return vram_[vram_addr_vert(addr)];
}

void Nes::writePpuVramV(uint16_t addr, uint8_t value)
{
    vram_[vram_addr_vert(addr)] = value;
}

uint8_t Nes::readPpuPltram(uint16_t addr)
{
    return ppu_.readPltram(addr);
}

void Nes::writePpuPltram(uint16_t addr, uint8_t value)
{
    ppu_.writePltram(addr, value);
}


Nes::CpuDoor::CpuDoor(Nes& nes) : nes_(nes) {}

uint8_t Nes::CpuDoor::read(uint16_t addr)
{
    return nes_.read(addr);
}

void Nes::CpuDoor::write(uint16_t addr, uint8_t value)
{
    nes_.write(addr, value);
}

void Nes::CpuDoor::tickApu(int cycle)
{
    nes_.apu_.tick(cycle);
}


Nes::PpuDoor::PpuDoor(Nes& nes) : nes_(nes) {}

uint8_t Nes::PpuDoor::readPpu(uint16_t addr)
{
    return nes_.readPpu(addr);
}

void Nes::PpuDoor::writePpu(uint16_t addr, uint8_t value)
{
    nes_.writePpu(addr, value);
}

void Nes::PpuDoor::triggerNmi()
{
    nes_.triggerNmi();
}


Nes::ApuDoor::ApuDoor(Nes& nes) : nes_(nes) {}

uint8_t Nes::ApuDoor::readDmc(uint16_t addr)
{
    assert(0x8000 <= addr /* && addr <= 0xFFFF */);

    nes_.cpu_.dmcDmaDelay(4); // FCEUXと同じ
    return nes_.read(addr);
}

void Nes::ApuDoor::triggerDmcIrq()
{
    nes_.triggerIrq();
}

void Nes::ApuDoor::triggerFrameIrq()
{
    nes_.triggerIrq();
}
