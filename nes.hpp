#pragma once

#include <array>
#include <cstdint>

#include "junknes.h"
#include "cpu.hpp"
#include "ppu.hpp"
#include "apu.hpp"

class Nes{
public:
    Nes(const std::uint8_t* prg, const std::uint8_t* chr, JunknesMirroring mirror);

    void hardReset();
    void softReset();

    void setInput(int port, unsigned int value);

    void emulateFrame();

    const std::uint8_t* screen() const;

    JunknesSound sound() const;

    void beforeExec(JunknesCpuHook hook, void* userdata);

private:
    void initRW();
    void initRWPpu();

    void triggerNmi();
    void triggerIrq();

    std::uint8_t read(std::uint16_t addr); // not const
    void write(std::uint16_t addr, std::uint8_t value);

    std::uint8_t readPpu(std::uint16_t addr); // こっちは const でもいいかもしれないけど…
    void writePpu(std::uint16_t addr, std::uint8_t value);


    std::uint8_t readNull(std::uint16_t);
    void writeNull(std::uint16_t, std::uint8_t);

    std::uint8_t readRam(std::uint16_t addr);
    void writeRam(std::uint16_t addr, std::uint8_t value);

    std::uint8_t readRamMirror(std::uint16_t addr);
    void writeRamMirror(std::uint16_t addr, std::uint8_t value);

    std::uint8_t readPrg(std::uint16_t addr);

    std::uint8_t read200x(std::uint16_t);
    std::uint8_t read2002(std::uint16_t);
    std::uint8_t read2004(std::uint16_t);
    std::uint8_t read2007(std::uint16_t);
    void write2000(std::uint16_t, std::uint8_t value);
    void write2001(std::uint16_t, std::uint8_t value);
    void write2002(std::uint16_t, std::uint8_t value);
    void write2003(std::uint16_t, std::uint8_t value);
    void write2004(std::uint16_t, std::uint8_t value);
    void write2005(std::uint16_t, std::uint8_t value);
    void write2006(std::uint16_t, std::uint8_t value);
    void write2007(std::uint16_t, std::uint8_t value);

    void write4014(std::uint16_t, std::uint8_t value);

    uint8_t read4015(std::uint16_t);
    void write4000(std::uint16_t, std::uint8_t value);
    void write4001(std::uint16_t, std::uint8_t value);
    void write4002(std::uint16_t, std::uint8_t value);
    void write4003(std::uint16_t, std::uint8_t value);
    void write4004(std::uint16_t, std::uint8_t value);
    void write4005(std::uint16_t, std::uint8_t value);
    void write4006(std::uint16_t, std::uint8_t value);
    void write4007(std::uint16_t, std::uint8_t value);
    void write4008(std::uint16_t, std::uint8_t value);
    void write400A(std::uint16_t, std::uint8_t value);
    void write400B(std::uint16_t, std::uint8_t value);
    void write400C(std::uint16_t, std::uint8_t value);
    void write400E(std::uint16_t, std::uint8_t value);
    void write400F(std::uint16_t, std::uint8_t value);
    void write4010(std::uint16_t, std::uint8_t value);
    void write4011(std::uint16_t, std::uint8_t value);
    void write4012(std::uint16_t, std::uint8_t value);
    void write4013(std::uint16_t, std::uint8_t value);
    void write4015(std::uint16_t, std::uint8_t value);
    void write4017(std::uint16_t, std::uint8_t value);

    uint8_t read4016(std::uint16_t);
    uint8_t read4017(std::uint16_t);
    void write4016(std::uint16_t, std::uint8_t value);


    std::uint8_t readPpuChr(std::uint16_t addr);

    std::uint8_t readPpuVramH(std::uint16_t addr);
    void writePpuVramH(std::uint16_t addr, std::uint8_t value);
    std::uint8_t readPpuVramV(std::uint16_t addr);
    void writePpuVramV(std::uint16_t addr, std::uint8_t value);

    std::uint8_t readPpuPltram(std::uint16_t addr);
    void writePpuPltram(std::uint16_t addr, std::uint8_t value);


    class CpuDoor : public Cpu::Door{
    public:
        explicit CpuDoor(Nes& nes);
        std::uint8_t read(std::uint16_t addr) override;
        void write(std::uint16_t addr, std::uint8_t value) override;

        void tickApu(int cycle) override;
    private:
        Nes& nes_;
    };

    class PpuDoor : public Ppu::Door{
    public:
        explicit PpuDoor(Nes& nes);
        std::uint8_t readPpu(std::uint16_t addr) override;
        void writePpu(std::uint16_t addr, std::uint8_t value) override;
        void triggerNmi() override;
    private:
        Nes& nes_;
    };

    class ApuDoor : public Apu::Door{
    public:
        explicit ApuDoor(Nes& nes);
        std::uint8_t readDmc(std::uint16_t addr) override;
        void triggerDmcIrq() override;
        void triggerFrameIrq() override;
    private:
        Nes& nes_;
    };


    const std::array<std::uint8_t, 0x8000> prg_;
    const std::array<std::uint8_t, 0x2000> chr_;
    const JunknesMirroring mirror_;

    std::array<std::uint8_t, 0x800> ram_;
    std::array<std::uint8_t, 0x800> vram_;

    Cpu cpu_;
    Ppu ppu_;
    Apu apu_;

    int ppuWarmup_;
    bool oddFrame_;

    using Reader = std::uint8_t (Nes::*)(std::uint16_t);
    using Writer = void (Nes::*)(std::uint16_t, std::uint8_t);
    std::array<Reader, 0x10000> readers_;
    std::array<Writer, 0x10000> writers_;
    std::array<Reader, 0x4000> readersPpu_;
    std::array<Writer, 0x4000> writersPpu_;

    std::array<unsigned int, 2> input_;
    std::array<unsigned int, 2> inputBit_;
    bool inputStrobe_;

    std::array<std::uint8_t, 256*240> screen_;
};
