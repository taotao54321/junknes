#pragma once

#include <array>
#include <memory>
#include <cstdint>

#include "util.hpp"

class Ppu{
public:
    class Door{
    public:
        virtual ~Door();
        virtual std::uint8_t readPpu(std::uint16_t addr) = 0;
        virtual void writePpu(std::uint16_t addr, std::uint8_t value) = 0;
        virtual void triggerNmi() = 0;
    };

    explicit Ppu(const std::shared_ptr<Door>& door);

    void hardReset();
    void softReset();

    void startFrame();
    void startLine();
    void doLine(int line, std::uint8_t* buf);
    void endLine();
    void startVBlank();

    void oamDma(const std::uint8_t buf[0x100]);

    std::uint8_t read200x();
    std::uint8_t read2002();
    std::uint8_t read2004();
    std::uint8_t read2007();
    void write2000(std::uint8_t value);
    void write2001(std::uint8_t value);
    void write2002(std::uint8_t value);
    void write2003(std::uint8_t value);
    void write2004(std::uint8_t value);
    void write2005(std::uint8_t value);
    void write2006(std::uint8_t value);
    void write2007(std::uint8_t value);

    std::uint8_t readPltram(std::uint16_t addr) const;
    void writePltram(std::uint16_t addr, std::uint8_t value);

private:
    std::shared_ptr<Door> door_;

    std::array<std::uint8_t, 0x100> oam_;
    std::array<std::uint8_t, 0x20> pltram_;

    union Ctrl{
        std::uint8_t raw;
        explicit Ctrl(std::uint8_t value=0) : raw(value) {}
        Ctrl& operator=(const Ctrl& rhs) { raw = rhs.raw; return *this; }
        BitField8<0,2> nt;
        BitField8<2>   inc32;
        BitField8<3>   spr_pat;
        BitField8<4>   bg_pat;
        BitField8<5>   spr16;
        BitField8<6>   slave;
        BitField8<7>   nmi;
    };
    Ctrl ctrl_;

    union Mask{
        std::uint8_t raw;
        explicit Mask(std::uint8_t value=0) : raw(value) {}
        Mask& operator=(const Mask& rhs) { raw = rhs.raw; return *this; }
        BitField8<0> gray;
        BitField8<1> bg_noclip;
        BitField8<2> spr_noclip;
        BitField8<3> bg_on;
        BitField8<4> spr_on;
        BitField8<5> emp_r;
        BitField8<6> emp_g;
        BitField8<7> emp_b;
    };
    Mask mask_;

    union Status{
        std::uint8_t raw;
        explicit Status(std::uint8_t value=0) : raw(value) {}
        Status& operator=(const Status& rhs) { raw = rhs.raw; return *this; }
        BitField8<0,5> garbage;
        BitField8<5> spr_over;
        BitField8<6> spr0_hit;
        BitField8<7> vbl;
    };
    Status status_;

    std::uint8_t oamAddr_;

    // http://wiki.nesdev.com/w/index.php/PPU_scrolling
    union Addr{
        std::uint16_t raw;
        explicit Addr(std::uint16_t value=0) : raw(value) {}
        Addr& operator=(const Addr& rhs) { raw = rhs.raw; return *this; }
        BitField16<0,14> addr;
        //-----
        BitField16<0,8>  addr_lo;
        BitField16<8,6>  addr_hi;
        BitField16<0,5>  x_coarse; // タイル単位
        BitField16<5,5>  y_coarse; // タイル単位
        BitField16<10,2> nt;
        BitField16<12,3> y_fine; // ピクセル単位
    };
    Addr regV_;
    Addr regT_;
    std::uint8_t regX_; // x_fine (ピクセル単位)
    bool regW_;

    std::uint8_t readBuf_;

    std::uint8_t genLatch_;


    

    void renderLine(int line, std::uint8_t* buf);
    void renderLineBg(int line, std::uint8_t* buf, bool* opacity);
    void renderLineSpr(int line, std::uint8_t* buf, const bool* opacity);
    bool checkSpr0(int line);

    bool isRenderingOn() const;

    std::uint8_t READPLT(int offset) const;
};
