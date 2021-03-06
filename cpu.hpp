#pragma once

#include <memory>
#include <cstdint>

#include "junknes.h"
#include "util.hpp"

class Cpu{
public:
    // CPUから外部へアクセスするためのインターフェース
    class Door{
    public:
        virtual ~Door();
        virtual std::uint8_t read(std::uint16_t addr) = 0;
        virtual void write(std::uint16_t addr, std::uint8_t value) = 0;

        // 1命令ごとにAPUを処理する。FCEUXのパクリ
        virtual void tickApu(int cycle /* CPU cycle */) = 0;
    };

    explicit Cpu(const std::shared_ptr<Door>& door);

    void hardReset();
    void softReset();

    void triggerNmi();
    void triggerIrq();

    void oamDmaDelay();
    void dmcDmaDelay(int cycle /* CPU cycle */);

    void exec(int cycle /* PPU cycle */);

    void beforeExec(JunknesCpuHook hook, void* userdata);

private:
    void doNmi();
    void doIrq();

    void delay(int cycle /* CPU cycle */);

    void fetchOp(std::uint8_t& opcode, std::uint16_t& operand);

    JunknesCpuState state() const;

    std::uint8_t read8(std::uint16_t addr);
    std::uint16_t read16(std::uint16_t addr);
    std::uint16_t read16_inpage(std::uint16_t addr);
    void write8(std::uint16_t addr, std::uint8_t value);
    void write16(std::uint16_t addr, std::uint16_t value);
    void write16_inpage(std::uint16_t addr, std::uint16_t value);

    std::uint8_t pop8();
    std::uint16_t pop16();
    void push8(std::uint8_t value);
    void push16(std::uint16_t value);

    std::uint16_t ADDR_ZPI(std::uint16_t arg, std::uint8_t idx);
    std::uint16_t ADDR_ZPX(std::uint16_t arg);
    std::uint16_t ADDR_ZPY(std::uint16_t arg);
    std::uint16_t ADDR_ABI_READ(std::uint16_t arg, std::uint8_t idx);
    std::uint16_t ADDR_ABX_READ(std::uint16_t arg);
    std::uint16_t ADDR_ABY_READ(std::uint16_t arg);
    std::uint16_t ADDR_ABI_WRITE(std::uint16_t arg, std::uint8_t idx);
    std::uint16_t ADDR_ABX_WRITE(std::uint16_t arg);
    std::uint16_t ADDR_ABY_WRITE(std::uint16_t arg);
    std::uint16_t ADDR_IX(std::uint16_t arg);
    std::uint16_t ADDR_IY_READ(std::uint16_t arg);
    std::uint16_t ADDR_IY_WRITE(std::uint16_t arg);

    std::uint8_t LD_ZP(std::uint16_t arg);
    std::uint8_t LD_ZPX(std::uint16_t arg);
    std::uint8_t LD_ZPY(std::uint16_t arg);
    std::uint8_t LD_AB(std::uint16_t arg);
    std::uint8_t LD_ABX(std::uint16_t arg);
    std::uint8_t LD_ABY(std::uint16_t arg);
    std::uint8_t LD_IX(std::uint16_t arg);
    std::uint8_t LD_IY(std::uint16_t arg);

    void ST_ZP(std::uint16_t arg, std::uint8_t value);
    void ST_ZPX(std::uint16_t arg, std::uint8_t value);
    void ST_ZPY(std::uint16_t arg, std::uint8_t value);
    void ST_AB(std::uint16_t arg, std::uint8_t value);
    void ST_ABX(std::uint16_t arg, std::uint8_t value);
    void ST_ABY(std::uint16_t arg, std::uint8_t value);
    void ST_IX(std::uint16_t arg, std::uint8_t value);
    void ST_IY(std::uint16_t arg, std::uint8_t value);

    struct AddrValue{
        const std::uint16_t addr;
        std::uint8_t value;
    };
    AddrValue AV_READ(std::uint16_t addr);
    void AV_WRITE(AddrValue av);

    AddrValue RMW_ZP(std::uint16_t arg);
    AddrValue RMW_ZPX(std::uint16_t arg);
    AddrValue RMW_AB(std::uint16_t arg);
    AddrValue RMW_ABX(std::uint16_t arg);
    AddrValue RMW_ABY(std::uint16_t arg);
    AddrValue RMW_IX(std::uint16_t arg);
    AddrValue RMW_IY(std::uint16_t arg);

    void ZN_UPDATE(std::uint8_t value);

    void LDA(std::uint8_t value);
    void LDX(std::uint8_t value);
    void LDY(std::uint8_t value);

    void ADC(std::uint8_t value);
    void SBC(std::uint8_t value);

    void ORA(std::uint8_t value);
    void AND(std::uint8_t value);
    void EOR(std::uint8_t value);

    void ASL_DO(std::uint8_t& value);
    void ASL();
    void ASL(AddrValue av);
    void LSR_DO(std::uint8_t& value);
    void LSR();
    void LSR(AddrValue av);
    void ROL_DO(std::uint8_t& value);
    void ROL();
    void ROL(AddrValue av);
    void ROR_DO(std::uint8_t& value);
    void ROR();
    void ROR(AddrValue av);

    void BIT(std::uint8_t value);

    void INC_DO(std::uint8_t& value);
    void INC(AddrValue av);
    void DEC_DO(std::uint8_t& value);
    void DEC(AddrValue av);

    void CMP_DO(std::uint8_t lhs, std::uint8_t rhs);
    void CMP(std::uint8_t value);
    void CPX(std::uint8_t value);
    void CPY(std::uint8_t value);

    void BRANCH(std::uint16_t arg, bool cond);

    void JMP_AB(std::uint16_t arg);
    void JMP_IND(std::uint16_t arg);

    void JSR(std::uint16_t arg);
    void RTS();
    void RTI();

    void BRK();

    void PUSH_P(bool b4);
    void POP_P();

    // unofficial
    void KIL();
    // http://wiki.nesdev.com/w/index.php/Programming_with_unofficial_opcodes
    void ALR(std::uint8_t value);
    void ANC(std::uint8_t value);
    void ARR(std::uint8_t value);
    void AXS(std::uint8_t value);
    void LAX(std::uint8_t value);
    void DCP(AddrValue av);
    void ISC(AddrValue av);
    void RLA(AddrValue av);
    void RRA(AddrValue av);
    void SLO(AddrValue av);
    void SRE(AddrValue av);
    // unofficial and unstable
    // http://www.oxyron.de/html/opcodes02.html
    // とりあえずFCEUXと同じ実装にしてある
    void LAS(std::uint16_t arg); // stable?
    uint8_t AHX_VALUE(std::uint16_t arg);
    void AHX_ABY(std::uint16_t arg);
    void AHX_IY(std::uint16_t arg);
    void TAS(std::uint16_t arg);
    void SHX(std::uint16_t arg);
    void SHY(std::uint16_t arg);
    void LAX_IM(std::uint16_t arg);
    void XAA(std::uint16_t arg);


    std::shared_ptr<Door> door_;

    JunknesCpuHook beforeExecHook_;
    void* beforeExecData_;

    int restCycle_; // PPU cycle

    bool nmi_;
    bool irq_;

    std::uint16_t PC_;
    std::uint8_t A_;
    std::uint8_t X_;
    std::uint8_t Y_;
    std::uint8_t S_;

    union Status{
        std::uint8_t raw;
        explicit Status(std::uint8_t value=0) : raw(value) {};
        Status& operator=(const Status& rhs) { raw = rhs.raw; return *this; }
        BitField8<0> C;
        BitField8<1> Z;
        BitField8<2> I;
        BitField8<3> D;
        BitField8<4> b4;
        BitField8<5> b5; // always 1
        BitField8<6> V;
        BitField8<7> N;
    };
    Status P_;

    bool jammed_;

    int apuRestCycle_; // CPU cycle
};
