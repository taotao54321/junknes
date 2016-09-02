#include <string>
#include <array>
#include <algorithm>
#include <memory>
#include <cstdio>
#include <cstdint>
#include <cassert>

#include "cpu.hpp"
#include "ppu.hpp"
#include "apu.hpp"
#include "mapper.hpp"
#include "util.hpp"

using namespace std;

Mapper::Mapper(const Prg& prg, const Chr& chr, NtMirror ntMirror)
    : prg_(prg), chr_(chr), ntMirror_(ntMirror),
      cpu_(make_shared<CpuDoor>(*this)),
      ppu_(make_shared<PpuDoor>(*this)),
      apu_(make_shared<ApuDoor>(*this))
{
    initRW();
    initRWPpu();

    cpu_.setDebugDoor(make_shared<CpuDebugDoor>());
}

void Mapper::initRW()
{
    // $0000-$07FF
    fill_n(readers_.begin(), 0x800, &Mapper::readRam);
    fill_n(writers_.begin(), 0x800, &Mapper::writeRam);

    // $0800-$1FFF
    fill_n(readers_.begin()+0x800, 3*0x800, &Mapper::readRamMirror);
    fill_n(writers_.begin()+0x800, 3*0x800, &Mapper::writeRamMirror);

    // $2000-$3FFF
    for(uint16_t addr = 0x2000; addr < 0x4000; addr += 8){
        readers_[addr+0] = &Mapper::read200x;
        readers_[addr+1] = &Mapper::read200x;
        readers_[addr+2] = &Mapper::read2002;
        readers_[addr+3] = &Mapper::read200x;
        readers_[addr+4] = &Mapper::read2004;
        readers_[addr+5] = &Mapper::read200x;
        readers_[addr+6] = &Mapper::read200x;
        readers_[addr+7] = &Mapper::read2007;

        writers_[addr+0] = &Mapper::write2000;
        writers_[addr+1] = &Mapper::write2001;
        writers_[addr+2] = &Mapper::write2002;
        writers_[addr+3] = &Mapper::write2003;
        writers_[addr+4] = &Mapper::write2004;
        writers_[addr+5] = &Mapper::write2005;
        writers_[addr+6] = &Mapper::write2006;
        writers_[addr+7] = &Mapper::write2007;
    }

    // $4000-$4017
    fill_n(readers_.begin()+0x4000, 0x15, &Mapper::readNull);
    readers_[0x4015] = &Mapper::read4015;
    readers_[0x4016] = &Mapper::read4016;
    readers_[0x4017] = &Mapper::read4017;

    writers_[0x4000] = &Mapper::write4000;
    writers_[0x4001] = &Mapper::write4001;
    writers_[0x4002] = &Mapper::write4002;
    writers_[0x4003] = &Mapper::write4003;
    writers_[0x4004] = &Mapper::write4004;
    writers_[0x4005] = &Mapper::write4005;
    writers_[0x4006] = &Mapper::write4006;
    writers_[0x4007] = &Mapper::write4007;
    writers_[0x4008] = &Mapper::write4008;
    writers_[0x4009] = &Mapper::writeNull;
    writers_[0x400A] = &Mapper::write400A;
    writers_[0x400B] = &Mapper::write400B;
    writers_[0x400C] = &Mapper::write400C;
    writers_[0x400D] = &Mapper::writeNull;
    writers_[0x400E] = &Mapper::write400E;
    writers_[0x400F] = &Mapper::write400F;
    writers_[0x4010] = &Mapper::write4010;
    writers_[0x4011] = &Mapper::write4011;
    writers_[0x4012] = &Mapper::write4012;
    writers_[0x4013] = &Mapper::write4013;
    writers_[0x4014] = &Mapper::write4014;
    writers_[0x4015] = &Mapper::write4015;
    writers_[0x4016] = &Mapper::write4016;
    writers_[0x4017] = &Mapper::write4017;

    // $4018-$7FFF
    fill_n(readers_.begin()+0x4018, 0x8000-0x4018, &Mapper::readNull);
    fill_n(writers_.begin()+0x4018, 0x8000-0x4018, &Mapper::writeNull);

    // $8000-$FFFF
    fill_n(readers_.begin()+0x8000, 0x8000, &Mapper::readPrg);
    fill_n(writers_.begin()+0x8000, 0x8000, &Mapper::writeNull);
}

void Mapper::initRWPpu()
{
    // $0000-$1FFF
    fill_n(readersPpu_.begin(), 0x2000, &Mapper::readPpuChr);
    fill_n(writersPpu_.begin(), 0x2000, &Mapper::writeNull);

    // $2000-$3EFF
    fill_n(readersPpu_.begin()+0x2000, 0x1F00,
           ntMirror_ == NtMirror::HORIZ ? &Mapper::readPpuVramH : &Mapper::readPpuVramV);
    fill_n(writersPpu_.begin()+0x2000, 0x1F00,
           ntMirror_ == NtMirror::HORIZ ? &Mapper::writePpuVramH : &Mapper::writePpuVramV);

    // $3F00-$3FFF
    fill_n(readersPpu_.begin()+0x3F00, 0x100, &Mapper::readPpuPltram);
    fill_n(writersPpu_.begin()+0x3F00, 0x100, &Mapper::writePpuPltram);
}

void Mapper::hardReset()
{
    ram_.fill(0);
    vram_.fill(0);

    cpu_.hardReset();
    ppu_.hardReset();
    apu_.hardReset();

    input_.fill(0);
    inputBit_.fill(0);
    inputStrobe_ = false;

    screen_.fill(0);
}

void Mapper::softReset()
{
    cpu_.softReset();
    ppu_.softReset();
    apu_.softReset();

    input_.fill(0);
    inputBit_.fill(0);
    inputStrobe_ = false;

    screen_.fill(0);
}

void Mapper::setInput(InputPort port, unsigned int value)
{
    input_[static_cast<int>(port)] = value;
}

// ライン単位でエミュレート
void Mapper::emulateFrame()
{
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
}

auto Mapper::screen() const -> const Screen&
{
    return screen_;
}

auto Mapper::soundSq1() const -> SoundSq  { return apu_.soundSq1(); }
auto Mapper::soundSq2() const -> SoundSq  { return apu_.soundSq2(); }
auto Mapper::soundTri() const -> SoundTri { return apu_.soundTri(); }
auto Mapper::soundNoi() const -> SoundNoi { return apu_.soundNoi(); }


void Mapper::triggerNmi() { cpu_.triggerNmi(); }
void Mapper::triggerIrq() { cpu_.triggerIrq(); }

uint8_t Mapper::read(uint16_t addr)
{
    return (this->*readers_[addr])(addr);
}

void Mapper::write(uint16_t addr, uint8_t value)
{
    (this->*writers_[addr])(addr, value);
}

uint8_t Mapper::readPpu(uint16_t addr)
{
    return (this->*readersPpu_[addr])(addr);
}

void Mapper::writePpu(uint16_t addr, uint8_t value)
{
    (this->*writersPpu_[addr])(addr, value);
}


uint8_t Mapper::readNull(uint16_t) { return 0; }
void Mapper::writeNull(uint16_t, uint8_t) {}

uint8_t Mapper::readRam(uint16_t addr)
{
    return ram_[addr];
}

void Mapper::writeRam(uint16_t addr, uint8_t value)
{
    ram_[addr] = value;
}

uint8_t Mapper::readRamMirror(uint16_t addr)
{
    return ram_[addr & 0x7FF];
}

void Mapper::writeRamMirror(uint16_t addr, uint8_t value)
{
    ram_[addr & 0x7FF] = value;
}

uint8_t Mapper::readPrg(uint16_t addr)
{
    return prg_[addr - 0x8000];
}

uint8_t Mapper::read200x(uint16_t) { return ppu_.read200x(); }
uint8_t Mapper::read2002(uint16_t) { return ppu_.read2002(); }
uint8_t Mapper::read2004(uint16_t) { return ppu_.read2004(); }
uint8_t Mapper::read2007(uint16_t) { return ppu_.read2007(); }

void Mapper::write2000(uint16_t, uint8_t value) { ppu_.write2000(value); }
void Mapper::write2001(uint16_t, uint8_t value) { ppu_.write2001(value); }
void Mapper::write2002(uint16_t, uint8_t value) { ppu_.write2002(value); }
void Mapper::write2003(uint16_t, uint8_t value) { ppu_.write2003(value); }
void Mapper::write2004(uint16_t, uint8_t value) { ppu_.write2004(value); }
void Mapper::write2005(uint16_t, uint8_t value) { ppu_.write2005(value); }
void Mapper::write2006(uint16_t, uint8_t value) { ppu_.write2006(value); }
void Mapper::write2007(uint16_t, uint8_t value) { ppu_.write2007(value); }

void Mapper::write4014(uint16_t, uint8_t value)
{
    uint16_t addr_base = value << 8;
    array<uint8_t, 0x100> buf;
    for(int i = 0; i < 0x100; ++i)
        buf[i] = read(addr_base + i);

    cpu_.oamDmaDelay();
    ppu_.oamDma(buf.data());
}

uint8_t Mapper::read4015(uint16_t) { return apu_.read4015(); }
void Mapper::write4000(uint16_t, uint8_t value) { apu_.write4000(value); }
void Mapper::write4001(uint16_t, uint8_t value) { apu_.write4001(value); }
void Mapper::write4002(uint16_t, uint8_t value) { apu_.write4002(value); }
void Mapper::write4003(uint16_t, uint8_t value) { apu_.write4003(value); }
void Mapper::write4004(uint16_t, uint8_t value) { apu_.write4004(value); }
void Mapper::write4005(uint16_t, uint8_t value) { apu_.write4005(value); }
void Mapper::write4006(uint16_t, uint8_t value) { apu_.write4006(value); }
void Mapper::write4007(uint16_t, uint8_t value) { apu_.write4007(value); }
void Mapper::write4008(uint16_t, uint8_t value) { apu_.write4008(value); }
void Mapper::write400A(uint16_t, uint8_t value) { apu_.write400A(value); }
void Mapper::write400B(uint16_t, uint8_t value) { apu_.write400B(value); }
void Mapper::write400C(uint16_t, uint8_t value) { apu_.write400C(value); }
void Mapper::write400E(uint16_t, uint8_t value) { apu_.write400E(value); }
void Mapper::write400F(uint16_t, uint8_t value) { apu_.write400F(value); }
void Mapper::write4010(uint16_t, uint8_t value) { apu_.write4010(value); }
void Mapper::write4011(uint16_t, uint8_t value) { apu_.write4011(value); }
void Mapper::write4012(uint16_t, uint8_t value) { apu_.write4012(value); }
void Mapper::write4013(uint16_t, uint8_t value) { apu_.write4013(value); }
void Mapper::write4015(uint16_t, uint8_t value) { apu_.write4015(value); }
void Mapper::write4017(uint16_t, uint8_t value) { apu_.write4017(value); }

uint8_t Mapper::read4016(uint16_t)
{
    // fourscoreでなければこれでよい?
    unsigned int bit = inputBit_[0];
    uint8_t ret = bit >= 8 ? 1 : ((input_[0]>>bit)&1);
    ++inputBit_[0];
    return ret;
}

uint8_t Mapper::read4017(uint16_t)
{
    // fourscoreでなければこれでよい?
    unsigned int bit = inputBit_[1];
    uint8_t ret = bit >= 8 ? 1 : ((input_[1]>>bit)&1);
    ++inputBit_[1];
    return ret;
}

void Mapper::write4016(uint16_t, uint8_t value)
{
    // 1 がstrobe要求
    // 1, 0 の順で書き込まれたら読めるようになる
    bool strobe_req = value & 1;
    if(inputStrobe_ && !strobe_req)
        inputBit_.fill(0);
    inputStrobe_ = strobe_req;
}

uint8_t Mapper::readPpuChr(uint16_t addr)
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

uint8_t Mapper::readPpuVramH(uint16_t addr)
{
    return vram_[vram_addr_horiz(addr)];
}

void Mapper::writePpuVramH(uint16_t addr, uint8_t value)
{
    vram_[vram_addr_horiz(addr)] = value;
}

uint8_t Mapper::readPpuVramV(uint16_t addr)
{
    return vram_[vram_addr_vert(addr)];
}

void Mapper::writePpuVramV(uint16_t addr, uint8_t value)
{
    vram_[vram_addr_vert(addr)] = value;
}

uint8_t Mapper::readPpuPltram(uint16_t addr)
{
    return ppu_.readPltram(addr);
}

void Mapper::writePpuPltram(uint16_t addr, uint8_t value)
{
    ppu_.writePltram(addr, value);
}


Mapper::CpuDoor::CpuDoor(Mapper& mapper) : mapper_(mapper) {}

uint8_t Mapper::CpuDoor::read(uint16_t addr)
{
    return mapper_.read(addr);
}

void Mapper::CpuDoor::write(uint16_t addr, uint8_t value)
{
    mapper_.write(addr, value);
}

void Mapper::CpuDoor::tickApu(int cycle)
{
    mapper_.apu_.tick(cycle);
}


namespace{
    enum class AdrMode{
        NONE,
        IM,
        ZP,
        ZPX,
        ZPY,
        AB,
        ABX,
        ABY,
        IX,
        IY,
        REL,
        IND,
        BRK,
    };

    constexpr char OP_NAME[0x100][4] = {
        /*0x00*/ "brk","ora","kil","slo", "dop","ora","asl","slo", "php","ora","asl","aac", "top","ora","asl","slo",
        /*0x10*/ "bpl","ora","kil","slo", "dop","ora","asl","slo", "clc","ora","nop","slo", "top","ora","asl","slo",
        /*0x20*/ "jsr","and","kil","rla", "bit","and","rol","rla", "plp","and","rol","aac", "bit","and","rol","rla",
        /*0x30*/ "bmi","and","kil","rla", "dop","and","rol","rla", "sec","and","nop","rla", "top","and","rol","rla",
        /*0x40*/ "rti","eor","kil","sre", "dop","eor","lsr","sre", "pha","eor","lsr","asr", "jmp","eor","lsr","sre",
        /*0x50*/ "bvc","eor","kil","sre", "dop","eor","lsr","sre", "cli","eor","nop","sre", "top","eor","lsr","sre",
        /*0x60*/ "rts","adc","kil","rra", "dop","adc","ror","rra", "pla","adc","ror","arr", "jmp","adc","ror","rra",
        /*0x70*/ "bvs","adc","kil","rra", "dop","adc","ror","rra", "sei","adc","nop","rra", "top","adc","ror","rra",
        /*0x80*/ "dop","sta","dop","aax", "sty","sta","stx","aax", "dey","dop","txa","xaa", "sty","sta","stx","aax",
        /*0x90*/ "bcc","sta","kil","axa", "sty","sta","stx","aax", "tya","sta","txs","xas", "sya","sta","sxa","axa",
        /*0xA0*/ "ldy","lda","ldx","lax", "ldy","lda","ldx","lax", "tay","lda","tax","atx", "ldy","lda","ldx","lax",
        /*0xB0*/ "bcs","lda","kil","lax", "ldy","lda","ldx","lax", "clv","lda","tsx","lar", "ldy","lda","ldx","lax",
        /*0xC0*/ "cpy","cmp","dop","dcp", "cpy","cmp","dec","dcp", "iny","cmp","dex","axs", "cpy","cmp","dec","dcp",
        /*0xD0*/ "bne","cmp","kil","dcp", "dop","cmp","dec","dcp", "cld","cmp","nop","dcp", "top","cmp","dec","dcp",
        /*0xE0*/ "cpx","sbc","dop","isb", "cpx","sbc","inc","isb", "inx","sbc","nop","sbc", "cpx","sbc","inc","isb",
        /*0xF0*/ "beq","sbc","kil","isb", "dop","sbc","inc","isb", "sed","sbc","nop","isb", "top","sbc","inc","isb"
    };

#define AM AdrMode
    constexpr AdrMode OP_ADRMODE[0x100] = {
        /*0x00*/ AM::BRK , AM::IX , AM::NONE, AM::IX , AM::ZP , AM::ZP , AM::ZP , AM::ZP ,
        /*0x08*/ AM::NONE, AM::IM , AM::NONE, AM::IM , AM::AB , AM::AB , AM::AB , AM::AB ,
        /*0x10*/ AM::REL , AM::IY , AM::NONE, AM::IY , AM::ZPX, AM::ZPX, AM::ZPX, AM::ZPX,
        /*0x18*/ AM::NONE, AM::ABY, AM::NONE, AM::ABY, AM::ABX, AM::ABX, AM::ABX, AM::ABX,
        /*0x20*/ AM::AB  , AM::IX , AM::NONE, AM::IX , AM::ZP , AM::ZP , AM::ZP , AM::ZP ,
        /*0x28*/ AM::NONE, AM::IM , AM::NONE, AM::IM , AM::AB , AM::AB , AM::AB , AM::AB ,
        /*0x30*/ AM::REL , AM::IY , AM::NONE, AM::IY , AM::ZPX, AM::ZPX, AM::ZPX, AM::ZPX,
        /*0x38*/ AM::NONE, AM::ABY, AM::NONE, AM::ABY, AM::ABX, AM::ABX, AM::ABX, AM::ABX,
        /*0x40*/ AM::NONE, AM::IX , AM::NONE, AM::IX , AM::ZP , AM::ZP , AM::ZP , AM::ZP ,
        /*0x48*/ AM::NONE, AM::IM , AM::NONE, AM::IM , AM::AB , AM::AB , AM::AB , AM::AB ,
        /*0x50*/ AM::REL , AM::IY , AM::NONE, AM::IY , AM::ZPX, AM::ZPX, AM::ZPX, AM::ZPX,
        /*0x58*/ AM::NONE, AM::ABY, AM::NONE, AM::ABY, AM::ABX, AM::ABX, AM::ABX, AM::ABX,
        /*0x60*/ AM::NONE, AM::IX , AM::NONE, AM::IX , AM::ZP , AM::ZP , AM::ZP , AM::ZP ,
        /*0x68*/ AM::NONE, AM::IM , AM::NONE, AM::IM , AM::IND, AM::AB , AM::AB , AM::AB ,
        /*0x70*/ AM::REL , AM::IY , AM::NONE, AM::IY , AM::ZPX, AM::ZPX, AM::ZPX, AM::ZPX,
        /*0x78*/ AM::NONE, AM::ABY, AM::NONE, AM::ABY, AM::ABX, AM::ABX, AM::ABX, AM::ABX,
        /*0x80*/ AM::IM  , AM::IX , AM::IM  , AM::IX , AM::ZP , AM::ZP , AM::ZP , AM::ZP ,
        /*0x88*/ AM::NONE, AM::IM , AM::NONE, AM::IM , AM::AB , AM::AB , AM::AB , AM::AB ,
        /*0x90*/ AM::REL , AM::IY , AM::NONE, AM::IY , AM::ZPX, AM::ZPX, AM::ZPY, AM::ZPY,
        /*0x98*/ AM::NONE, AM::ABY, AM::NONE, AM::ABY, AM::ABX, AM::ABX, AM::ABY, AM::ABY,
        /*0xA0*/ AM::IM  , AM::IX , AM::IM  , AM::IX , AM::ZP , AM::ZP , AM::ZP , AM::ZP ,
        /*0xA8*/ AM::NONE, AM::IM , AM::NONE, AM::IM , AM::AB , AM::AB , AM::AB , AM::AB ,
        /*0xB0*/ AM::REL , AM::IY , AM::NONE, AM::IY , AM::ZPX, AM::ZPX, AM::ZPY, AM::ZPY,
        /*0xB8*/ AM::NONE, AM::ABY, AM::NONE, AM::ABY, AM::ABX, AM::ABX, AM::ABY, AM::ABY,
        /*0xC0*/ AM::IM  , AM::IX , AM::IM  , AM::IX , AM::ZP , AM::ZP , AM::ZP , AM::ZP ,
        /*0xC8*/ AM::NONE, AM::IM , AM::NONE, AM::IM , AM::AB , AM::AB , AM::AB , AM::AB ,
        /*0xD0*/ AM::REL , AM::IY , AM::NONE, AM::IY , AM::ZPX, AM::ZPX, AM::ZPX, AM::ZPX,
        /*0xD8*/ AM::NONE, AM::ABY, AM::NONE, AM::ABY, AM::ABX, AM::ABX, AM::ABX, AM::ABX,
        /*0xE0*/ AM::IM  , AM::IX , AM::IM  , AM::IX , AM::ZP , AM::ZP , AM::ZP , AM::ZP ,
        /*0xE8*/ AM::NONE, AM::IM , AM::NONE, AM::IM , AM::AB , AM::AB , AM::AB , AM::AB ,
        /*0xF0*/ AM::REL , AM::IY , AM::NONE, AM::IY , AM::ZPX, AM::ZPX, AM::ZPX, AM::ZPX,
        /*0xF8*/ AM::NONE, AM::ABY, AM::NONE, AM::ABY, AM::ABX, AM::ABX, AM::ABX, AM::ABX
    };
#undef AM

    string operand_str(int opcode, uint16_t operand)
    {
        char buf[1024];
        switch(Cpu::OP_ARGLEN[opcode]){
        case 0: return "";
        case 1:
            snprintf(buf, sizeof(buf), " %02X", operand);
            return buf;
        case 2:
            snprintf(buf, sizeof(buf), " %02X %02X", operand&0xFF, operand>>8);
            return buf;
        default:
            assert(false);
        }
    }

    uint16_t rel_addr(uint16_t addr, uint16_t operand)
    {
        return addr + 2 + static_cast<int8_t>(operand);
    }

    string dis_one(uint16_t addr, uint8_t opcode, uint16_t operand)
    {
        const char* name = OP_NAME[opcode];
        char buf[1024];
        switch(OP_ADRMODE[opcode]){
        case AdrMode::NONE:
            snprintf(buf, sizeof(buf), "%s", name);
            return buf;
        case AdrMode::IM:
            snprintf(buf, sizeof(buf), "%s #$%02X", name, operand);
            return buf;
        case AdrMode::ZP:
            snprintf(buf, sizeof(buf), "%s $%02X", name, operand);
            return buf;
        case AdrMode::ZPX:
            snprintf(buf, sizeof(buf), "%s $%02X,x", name, operand);
            return buf;
        case AdrMode::ZPY:
            snprintf(buf, sizeof(buf), "%s $%02X,y", name, operand);
            return buf;
        case AdrMode::AB:
            snprintf(buf, sizeof(buf), "%s $%04X", name, operand);
            return buf;
        case AdrMode::ABX:
            snprintf(buf, sizeof(buf), "%s $%04X,x", name, operand);
            return buf;
        case AdrMode::ABY:
            snprintf(buf, sizeof(buf), "%s $%04X,y", name, operand);
            return buf;
        case AdrMode::IX:
            snprintf(buf, sizeof(buf), "%s ($%02X,x)", name, operand);
            return buf;
        case AdrMode::IY:
            snprintf(buf, sizeof(buf), "%s ($%02X),y", name, operand);
            return buf;
        case AdrMode::REL:
            snprintf(buf, sizeof(buf), "%s $%04X", name, rel_addr(addr, operand));
            return buf;
        case AdrMode::IND:
            snprintf(buf, sizeof(buf), "%s ($%04X)", name, operand);
            return buf;
        case AdrMode::BRK:
            snprintf(buf, sizeof(buf), "%s #$%02X", name, operand);
            return buf;
        default:
            assert(false);
        }
    }

    string status_str(Cpu::Status p)
    {
        char buf[1024];
        snprintf(buf, sizeof(buf), "%c%c%c%c%c%c%c%c",
                 p.N  ? 'N' : '.',
                 p.V  ? 'V' : '.',
                 p.b5 ? '.' : '0', // 常に1になっているべきなので
                 p.b4 ? 'B' : '.',
                 p.D  ? 'D' : '.',
                 p.I  ? 'I' : '.',
                 p.Z  ? 'Z' : '.',
                 p.C  ? 'C' : '.');
        return buf;
    }

    void trace_one(const Cpu::State& state, uint8_t opcode, uint16_t operand)
    {
        INFO("%04X\t%02X%-6s\t%-11s\tA:%02X X:%02X Y:%02X S:%02X P:%s\n",
             state.PC, opcode, operand_str(opcode, operand).c_str(),
             dis_one(state.PC, opcode, operand).c_str(),
             state.A, state.X, state.Y, state.S, status_str(state.P).c_str());
    }
}

Mapper::CpuDebugDoor::CpuDebugDoor() {}

void Mapper::CpuDebugDoor::beforeExec(
    const Cpu::State& state, uint8_t opcode, uint16_t operand)
{
    //trace_one(state, opcode, operand);
}


Mapper::PpuDoor::PpuDoor(Mapper& mapper) : mapper_(mapper) {}

uint8_t Mapper::PpuDoor::readPpu(uint16_t addr)
{
    return mapper_.readPpu(addr);
}

void Mapper::PpuDoor::writePpu(uint16_t addr, uint8_t value)
{
    mapper_.writePpu(addr, value);
}

void Mapper::PpuDoor::triggerNmi()
{
    mapper_.triggerNmi();
}


Mapper::ApuDoor::ApuDoor(Mapper& mapper) : mapper_(mapper) {}

void Mapper::ApuDoor::triggerIrq()
{
    mapper_.triggerIrq();
}
