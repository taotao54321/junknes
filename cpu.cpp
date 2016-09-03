#include <memory>
#include <cstdint>
#include <cassert>

#include "cpu.hpp"
#include "util.hpp"

using namespace std;

namespace{
    constexpr uint16_t VEC_NMI   = 0xFFFA;
    constexpr uint16_t VEC_RESET = 0xFFFC;
    constexpr uint16_t VEC_IRQ   = 0xFFFE;

    // copied from FCEUX
    constexpr int OP_CYCLE[0x100] = {
        /*0x00*/ 7,6,2,8, 3,3,5,5, 3,2,2,2, 4,4,6,6,
        /*0x10*/ 2,5,2,8, 4,4,6,6, 2,4,2,7, 4,4,7,7,
        /*0x20*/ 6,6,2,8, 3,3,5,5, 4,2,2,2, 4,4,6,6,
        /*0x30*/ 2,5,2,8, 4,4,6,6, 2,4,2,7, 4,4,7,7,
        /*0x40*/ 6,6,2,8, 3,3,5,5, 3,2,2,2, 3,4,6,6,
        /*0x50*/ 2,5,2,8, 4,4,6,6, 2,4,2,7, 4,4,7,7,
        /*0x60*/ 6,6,2,8, 3,3,5,5, 4,2,2,2, 5,4,6,6,
        /*0x70*/ 2,5,2,8, 4,4,6,6, 2,4,2,7, 4,4,7,7,
        /*0x80*/ 2,6,2,6, 3,3,3,3, 2,2,2,2, 4,4,4,4,
        /*0x90*/ 2,6,2,6, 4,4,4,4, 2,5,2,5, 5,5,5,5,
        /*0xA0*/ 2,6,2,6, 3,3,3,3, 2,2,2,2, 4,4,4,4,
        /*0xB0*/ 2,5,2,5, 4,4,4,4, 2,4,2,4, 4,4,4,4,
        /*0xC0*/ 2,6,2,8, 3,3,5,5, 2,2,2,2, 4,4,6,6,
        /*0xD0*/ 2,5,2,8, 4,4,6,6, 2,4,2,7, 4,4,7,7,
        /*0xE0*/ 2,6,3,8, 3,3,5,5, 2,2,2,2, 4,4,6,6,
        /*0xF0*/ 2,5,2,8, 4,4,6,6, 2,4,2,7, 4,4,7,7
    };


    class NullDebugDoor : public Cpu::DebugDoor{
    public:
        void beforeExec(const Cpu::State&, uint8_t, uint16_t) override {}
    };
}


constexpr int Cpu::OP_ARGLEN[];


Cpu::Door::~Door() {}

Cpu::DebugDoor::~DebugDoor() {}


Cpu::Cpu(const shared_ptr<Door>& door)
    : door_(door), dbgDoor_(nullptr)
{
    
}

auto Cpu::state() const -> State
{
    State st;
    st.PC = PC_;
    st.A  = A_;
    st.X  = X_;
    st.Y  = Y_;
    st.S  = S_;
    st.P  = P_;
    return st;
}

void Cpu::setDebugDoor(const shared_ptr<DebugDoor>& dbgDoor)
{
    dbgDoor_ = dbgDoor ? dbgDoor : make_shared<NullDebugDoor>();
}

/**
 * http://wiki.nesdev.com/w/index.php/CPU_power_up_state
 *
 * FCEUXはRESET割り込みの処理を遅らせてるけど…
 */
void Cpu::hardReset()
{
    restCycle_ = 0;

    nmi_ = false;
    irq_ = false;

    PC_ = read16(VEC_RESET);

    A_ = 0;
    X_ = 0;
    Y_ = 0;

    S_ = 0xFD;

    P_.raw = 0;
    P_.I = 1;
    P_.b4 = 1;
    P_.b5 = 1;

    apuRestCycle_ = 0;
}

/**
 * http://wiki.nesdev.com/w/index.php/CPU_power_up_state
 *
 * FCEUXはRESET割り込みフラグを立てるだけなので結構違いが出るかも
 */
void Cpu::softReset()
{
    //restCycle_ = 0; // サイクル数は触らない方がいい?

    nmi_ = false;
    irq_ = false;

    PC_ = read16(VEC_RESET);

    S_ -= 3;

    P_.I = 1;

    //apuRestCycle_ = 0;
}

void Cpu::triggerNmi()
{
    nmi_ = true;
}

void Cpu::triggerIrq()
{
    irq_ = true;
}

void Cpu::oamDmaDelay()
{
    delay(512); // FCEUXと同じ。正しくは513or514?
}

void Cpu::dmcDmaDelay(int cycle)
{
    delay(cycle);
}

void Cpu::exec(int cycle)
{
    restCycle_ += cycle;

    while(restCycle_ >= 3){
        if(nmi_){
            doNmi();
        }
        else if(irq_){
            if(!P_.I) doIrq();
        }

        uint8_t opcode;
        uint16_t arg;
        fetchOp(opcode, arg);

        dbgDoor_->beforeExec(state(), opcode, arg);

        delay(OP_CYCLE[opcode]);

        // 1命令ごとにAPUを処理。FCEUXと同じタイミングになってる…はず
        {
            int tmp = apuRestCycle_;
            apuRestCycle_ = 0;
            door_->tickApu(tmp);
        }

        // 非公式命令および BRK はとりあえず無視
        switch(opcode){
        case 0xA9: LDA(arg);         break;
        case 0xA5: LDA(LD_ZP(arg));  break;
        case 0xB5: LDA(LD_ZPX(arg)); break;
        case 0xAD: LDA(LD_AB(arg));  break;
        case 0xBD: LDA(LD_ABX(arg)); break;
        case 0xB9: LDA(LD_ABY(arg)); break;
        case 0xA1: LDA(LD_IX(arg));  break;
        case 0xB1: LDA(LD_IY(arg));  break;

        case 0xA2: LDX(arg);         break;
        case 0xA6: LDX(LD_ZP(arg));  break;
        case 0xB6: LDX(LD_ZPY(arg)); break;
        case 0xAE: LDX(LD_AB(arg));  break;
        case 0xBE: LDX(LD_ABY(arg)); break;

        case 0xA0: LDY(arg);         break;
        case 0xA4: LDY(LD_ZP(arg));  break;
        case 0xB4: LDY(LD_ZPX(arg)); break;
        case 0xAC: LDY(LD_AB(arg));  break;
        case 0xBC: LDY(LD_ABX(arg)); break;

        // STA
        case 0x85: ST_ZP(arg, A_);  break;
        case 0x95: ST_ZPX(arg, A_); break;
        case 0x8D: ST_AB(arg, A_);  break;
        case 0x9D: ST_ABX(arg, A_); break;
        case 0x99: ST_ABY(arg, A_); break;
        case 0x81: ST_IX(arg, A_);  break;
        case 0x91: ST_IY(arg, A_);  break;

        // STX
        case 0x86: ST_ZP(arg, X_);  break;
        case 0x96: ST_ZPY(arg, X_); break;
        case 0x8E: ST_AB(arg, X_);  break;

        // STY
        case 0x84: ST_ZP(arg, Y_);  break;
        case 0x94: ST_ZPX(arg, Y_); break;
        case 0x8C: ST_AB(arg, Y_);  break;

        case 0xAA: /* TAX */ ZN_UPDATE(X_ = A_); break;
        case 0x8A: /* TXA */ ZN_UPDATE(A_ = X_); break;
        case 0xA8: /* TAY */ ZN_UPDATE(Y_ = A_); break;
        case 0x98: /* TYA */ ZN_UPDATE(A_ = Y_); break;
        case 0xBA: /* TSX */ ZN_UPDATE(X_ = S_); break;
        case 0x9A: /* TXS */ S_ = X_;            break;

        case 0x69: ADC(arg);         break;
        case 0x65: ADC(LD_ZP(arg));  break;
        case 0x75: ADC(LD_ZPX(arg)); break;
        case 0x6D: ADC(LD_AB(arg));  break;
        case 0x7D: ADC(LD_ABX(arg)); break;
        case 0x79: ADC(LD_ABY(arg)); break;
        case 0x61: ADC(LD_IX(arg));  break;
        case 0x71: ADC(LD_IY(arg));  break;

        case 0xE9: SBC(arg);         break;
        case 0xE5: SBC(LD_ZP(arg));  break;
        case 0xF5: SBC(LD_ZPX(arg)); break;
        case 0xED: SBC(LD_AB(arg));  break;
        case 0xFD: SBC(LD_ABX(arg)); break;
        case 0xF9: SBC(LD_ABY(arg)); break;
        case 0xE1: SBC(LD_IX(arg));  break;
        case 0xF1: SBC(LD_IY(arg));  break;

        case 0x09: ORA(arg);         break;
        case 0x05: ORA(LD_ZP(arg));  break;
        case 0x15: ORA(LD_ZPX(arg)); break;
        case 0x0D: ORA(LD_AB(arg));  break;
        case 0x1D: ORA(LD_ABX(arg)); break;
        case 0x19: ORA(LD_ABY(arg)); break;
        case 0x01: ORA(LD_IX(arg));  break;
        case 0x11: ORA(LD_IY(arg));  break;

        case 0x29: AND(arg);         break;
        case 0x25: AND(LD_ZP(arg));  break;
        case 0x35: AND(LD_ZPX(arg)); break;
        case 0x2D: AND(LD_AB(arg));  break;
        case 0x3D: AND(LD_ABX(arg)); break;
        case 0x39: AND(LD_ABY(arg)); break;
        case 0x21: AND(LD_IX(arg));  break;
        case 0x31: AND(LD_IY(arg));  break;

        case 0x49: EOR(arg);         break;
        case 0x45: EOR(LD_ZP(arg));  break;
        case 0x55: EOR(LD_ZPX(arg)); break;
        case 0x4D: EOR(LD_AB(arg));  break;
        case 0x5D: EOR(LD_ABX(arg)); break;
        case 0x59: EOR(LD_ABY(arg)); break;
        case 0x41: EOR(LD_IX(arg));  break;
        case 0x51: EOR(LD_IY(arg));  break;

        case 0x0A: ASL();             break;
        case 0x06: ASL(RMW_ZP(arg));  break;
        case 0x16: ASL(RMW_ZPX(arg)); break;
        case 0x0E: ASL(RMW_AB(arg));  break;
        case 0x1E: ASL(RMW_ABX(arg)); break;

        case 0x4A: LSR();             break;
        case 0x46: LSR(RMW_ZP(arg));  break;
        case 0x56: LSR(RMW_ZPX(arg)); break;
        case 0x4E: LSR(RMW_AB(arg));  break;
        case 0x5E: LSR(RMW_ABX(arg)); break;

        case 0x2A: ROL();             break;
        case 0x26: ROL(RMW_ZP(arg));  break;
        case 0x36: ROL(RMW_ZPX(arg)); break;
        case 0x2E: ROL(RMW_AB(arg));  break;
        case 0x3E: ROL(RMW_ABX(arg)); break;

        case 0x6A: ROR();             break;
        case 0x66: ROR(RMW_ZP(arg));  break;
        case 0x76: ROR(RMW_ZPX(arg)); break;
        case 0x6E: ROR(RMW_AB(arg));  break;
        case 0x7E: ROR(RMW_ABX(arg)); break;

        case 0x24: BIT(LD_ZP(arg)); break;
        case 0x2C: BIT(LD_AB(arg)); break;

        case 0xE6: INC(RMW_ZP(arg));     break;
        case 0xF6: INC(RMW_ZPX(arg));    break;
        case 0xEE: INC(RMW_AB(arg));     break;
        case 0xFE: INC(RMW_ABX(arg));    break;
        case 0xE8: /* INX */ INC_DO(X_); break;
        case 0xC8: /* INY */ INC_DO(Y_); break;

        case 0xC6: DEC(RMW_ZP(arg));     break;
        case 0xD6: DEC(RMW_ZPX(arg));    break;
        case 0xCE: DEC(RMW_AB(arg));     break;
        case 0xDE: DEC(RMW_ABX(arg));    break;
        case 0xCA: /* DEX */ DEC_DO(X_); break;
        case 0x88: /* DEY */ DEC_DO(Y_); break;

        case 0xC9: CMP(arg);         break;
        case 0xC5: CMP(LD_ZP(arg));  break;
        case 0xD5: CMP(LD_ZPX(arg)); break;
        case 0xCD: CMP(LD_AB(arg));  break;
        case 0xDD: CMP(LD_ABX(arg)); break;
        case 0xD9: CMP(LD_ABY(arg)); break;
        case 0xC1: CMP(LD_IX(arg));  break;
        case 0xD1: CMP(LD_IY(arg));  break;

        case 0xE0: CPX(arg);        break;
        case 0xE4: CPX(LD_ZP(arg)); break;
        case 0xEC: CPX(LD_AB(arg)); break;

        case 0xC0: CPY(arg);        break;
        case 0xC4: CPY(LD_ZP(arg)); break;
        case 0xCC: CPY(LD_AB(arg)); break;

        case 0xB0: /* BCS */ BRANCH(arg, P_.C);  break;
        case 0x90: /* BCC */ BRANCH(arg, !P_.C); break;
        case 0xF0: /* BEQ */ BRANCH(arg, P_.Z);  break;
        case 0xD0: /* BNE */ BRANCH(arg, !P_.Z); break;
        case 0x70: /* BVS */ BRANCH(arg, P_.V);  break;
        case 0x50: /* BVC */ BRANCH(arg, !P_.V); break;
        case 0x30: /* BMI */ BRANCH(arg, P_.N);  break;
        case 0x10: /* BPL */ BRANCH(arg, !P_.N); break;

        case 0x38: /* SEC */ P_.C = 1; break;
        case 0x18: /* CLC */ P_.C = 0; break;
        case 0x78: /* SEI */ P_.I = 1; break;
        case 0x58: /* CLI */ P_.I = 0; break;
        case 0xF8: /* SED */ P_.D = 1; break;
        case 0xD8: /* CLD */ P_.D = 0; break;
        case 0xB8: /* CLV */ P_.V = 0; break;

        case 0x4C: JMP_AB(arg);  break;
        case 0x6C: JMP_IND(arg); break;

        case 0x20: JSR(arg); break;
        case 0x60: RTS();    break;
        case 0x40: RTI();    break;

        case 0x48: /* PHA */ push8(A_);              break;
        case 0x08: /* PHP */ PUSH_P(/* b4= */ true); break;

        case 0x68: /* PLA */ ZN_UPDATE(A_ = pop8()); break;
        case 0x28: /* PLP */ POP_P();                break;

        case 0xEA: break; // NOP

        default:
            break;
        }
    }
}


void Cpu::doNmi()
{
    delay(7);

    push16(PC_);
    PUSH_P(/* b4= */ false);

    PC_ = read16(VEC_NMI);

    P_.I = 1;

    nmi_ = false;
}

void Cpu::doIrq()
{
    delay(7);

    push16(PC_);
    PUSH_P(/* b4= */ false);

    PC_ = read16(VEC_IRQ);

    P_.I = 1;

    irq_ = false;
}

void Cpu::delay(int cycle)
{
    restCycle_ -= 3*cycle;

    apuRestCycle_ += cycle;
}

void Cpu::fetchOp(uint8_t& opcode, uint16_t& arg)
{
    opcode = read8(PC_++);

    switch(OP_ARGLEN[opcode]){
    case 0: arg = 0; break; // inline化時の未初期化警告抑止のため一応代入
    case 1: arg = read8(PC_++); break;
    case 2: arg = read16(PC_); PC_ += 2; break;
    default: assert(false);
    }
}


uint8_t Cpu::read8(uint16_t addr)
{
    return door_->read(addr);
}

/**
 * ページ境界を越えてもよい(引数が $FFFF の場合、2Byte目は $0000)
 */
uint16_t Cpu::read16(uint16_t addr)
{
    uint16_t addr_next = addr + 1;
    return read8(addr) | (read8(addr_next)<<8);
}

/**
 * ページ境界を越える場合、ページの先頭へループ
 */
uint16_t Cpu::read16_inpage(uint16_t addr)
{
    uint16_t addr_next = (addr&0xFF00) | ((addr+1)&0xFF);
    return read8(addr) | (read8(addr_next)<<8);
}

void Cpu::write8(uint16_t addr, uint8_t value)
{
    door_->write(addr, value);
}

/**
 * ページ境界を超えてもよい(引数が $FFFF の場合、2Byte目は $0000)
 */
void Cpu::write16(uint16_t addr, uint16_t value)
{
    uint16_t addr_next = addr + 1;
    write8(addr, value & 0xFF);
    write8(addr_next, value >> 8);
}

/**
 * ページ境界を越える場合、ページの先頭へループ
 */
void Cpu::write16_inpage(uint16_t addr, uint16_t value)
{
    uint16_t addr_next = (addr&0xFF00) | ((addr+1)&0xFF);
    write8(addr, value & 0xFF);
    write8(addr_next, value >> 8);
}

uint8_t Cpu::pop8()
{
    ++S_;
    return read8(0x100 | S_);
}

// S == 0xFE の場合、$01FF, $0100 を読み取る
uint16_t Cpu::pop16()
{
    ++S_;
    uint16_t addr = 0x100 | S_;
    ++S_;
    return read16_inpage(addr);
}

void Cpu::push8(uint8_t value)
{
    write8(0x100 | S_--, value);
}

// S == 0 の場合、$0100, $01FF へ書き込む
void Cpu::push16(uint16_t value)
{
    --S_;
    uint16_t addr = 0x100 | S_;
    --S_;
    write16_inpage(addr, value);
}

// ページ境界を越える場合、$00 へループ(サイクル数は同じ)
uint16_t Cpu::ADDR_ZPI(uint16_t arg, uint8_t idx)
{
    uint8_t addr = arg + idx;
    return addr;
}

uint16_t Cpu::ADDR_ZPX(uint16_t arg) { return ADDR_ZPI(arg, X_); }
uint16_t Cpu::ADDR_ZPY(uint16_t arg) { return ADDR_ZPI(arg, Y_); }

/**
 * readonlyな命令で使われる。FCEUXと同じ実装。
 */
uint16_t Cpu::ADDR_ABI_READ(uint16_t arg, uint8_t idx)
{
    uint16_t addr = arg + idx;

    // ページ境界を越えたら1サイクル余分に消費。また、投機実行による余
    // 分な読み取りをエミュレート
    if((arg ^ addr) & 0x100){
        delay(1);
        read8(addr ^ 0x100);
    }

    return addr;
}

uint16_t Cpu::ADDR_ABX_READ(uint16_t arg) { return ADDR_ABI_READ(arg, X_); }
uint16_t Cpu::ADDR_ABY_READ(uint16_t arg) { return ADDR_ABI_READ(arg, Y_); }

/**
 * 書き込みを含む命令で使われる。FCEUXと同じ実装。
 */
uint16_t Cpu::ADDR_ABI_WRITE(uint16_t arg, uint8_t idx)
{
    uint16_t addr = arg + idx;

    // ページ境界を越えてもサイクル数は変わらない(書き込みでは投機実行
    // ができないため)。ただし常にこの読み取りが発生
    read8((arg&0xFF00) | (addr&0xFF));

    return addr;
}

uint16_t Cpu::ADDR_ABX_WRITE(uint16_t arg) { return ADDR_ABI_WRITE(arg, X_); }
uint16_t Cpu::ADDR_ABY_WRITE(uint16_t arg) { return ADDR_ABI_WRITE(arg, Y_); }

uint16_t Cpu::ADDR_IX(uint16_t arg)
{
    uint16_t ptr = ADDR_ZPX(arg);
    return read16_inpage(ptr);
}

/**
 * readonlyな命令で使われる。FCEUXと同じ実装。
 */
uint16_t Cpu::ADDR_IY_READ(uint16_t arg)
{
    uint16_t addr_base = read16_inpage(arg);
    uint16_t addr = addr_base + Y_;

    // ページ境界を越えたら1サイクル余分に消費。また、投機実行による余
    // 分な読み取りをエミュレート
    if((addr_base ^ addr) & 0x100){
        delay(1);
        read8(addr ^ 0x100);
    }

    return addr;
}

/**
 * 書き込みを含む命令で使われる。FCEUXと同じ実装。
 */
uint16_t Cpu::ADDR_IY_WRITE(uint16_t arg)
{
    uint16_t addr_base = read16_inpage(arg);
    uint16_t addr = addr_base + Y_;

    // ページ境界を越えてもサイクル数は変わらない(書き込みでは投機実行
    // ができないため)。ただし常にこの読み取りが発生
    read8((addr_base&0xFF00) | (addr&0xFF));

    return addr;
}

uint8_t Cpu::LD_ZP(uint16_t arg)  { return read8(arg); }
uint8_t Cpu::LD_ZPX(uint16_t arg) { return read8(ADDR_ZPX(arg)); }
uint8_t Cpu::LD_ZPY(uint16_t arg) { return read8(ADDR_ZPY(arg)); }
uint8_t Cpu::LD_AB(uint16_t arg)  { return read8(arg); }
uint8_t Cpu::LD_ABX(uint16_t arg) { return read8(ADDR_ABX_READ(arg)); }
uint8_t Cpu::LD_ABY(uint16_t arg) { return read8(ADDR_ABY_READ(arg)); }
uint8_t Cpu::LD_IX(uint16_t arg)  { return read8(ADDR_IX(arg)); }
uint8_t Cpu::LD_IY(uint16_t arg)  { return read8(ADDR_IY_READ(arg)); }

void Cpu::ST_ZP(uint16_t arg, uint8_t value)  { write8(arg, value); }
void Cpu::ST_ZPX(uint16_t arg, uint8_t value) { write8(ADDR_ZPX(arg), value); }
void Cpu::ST_ZPY(uint16_t arg, uint8_t value) { write8(ADDR_ZPY(arg), value); }
void Cpu::ST_AB(uint16_t arg, uint8_t value)  { write8(arg, value); }
void Cpu::ST_ABX(uint16_t arg, uint8_t value) { write8(ADDR_ABX_WRITE(arg), value); }
void Cpu::ST_ABY(uint16_t arg, uint8_t value) { write8(ADDR_ABY_WRITE(arg), value); }
void Cpu::ST_IX(uint16_t arg, uint8_t value)  { write8(ADDR_IX(arg), value); }
void Cpu::ST_IY(uint16_t arg, uint8_t value)  { write8(ADDR_IY_WRITE(arg), value); }

auto Cpu::AV_READ(uint16_t addr) -> AddrValue
{
    return AddrValue{ addr, read8(addr) };
}

void Cpu::AV_WRITE(AddrValue av)
{
    write8(av.addr, av.value);
}

auto Cpu::RMW_ZP(uint16_t arg) -> AddrValue
{
    AddrValue av = AV_READ(arg);
    //write8(av.addr, av.value); // zeropageなのでRMWW動作は省略
    return av;
}

auto Cpu::RMW_ZPX(uint16_t arg) -> AddrValue
{
    AddrValue av = AV_READ(ADDR_ZPX(arg));
    //write8(av.addr, av.value); // zeropageなのでRMWW動作は省略
    return av;
}

auto Cpu::RMW_AB(uint16_t arg) -> AddrValue
{
    AddrValue av = AV_READ(arg);
    write8(av.addr, av.value); // RMWW
    return av;
}

auto Cpu::RMW_ABX(uint16_t arg) -> AddrValue
{
    AddrValue av = AV_READ(ADDR_ABX_WRITE(arg));
    write8(av.addr, av.value); // RMWW
    return av;
}

// 一部の非公式命令のみで使われる
auto Cpu::RMW_ABY(uint16_t arg) -> AddrValue
{
    AddrValue av = AV_READ(ADDR_ABY_WRITE(arg));
    write8(av.addr, av.value); // RMWW
    return av;
}

auto Cpu::RMW_IX(uint16_t arg) -> AddrValue
{
    AddrValue av = AV_READ(ADDR_IX(arg));
    write8(av.addr, av.value); // RMWW
    return av;
}

auto Cpu::RMW_IY(uint16_t arg) -> AddrValue
{
    AddrValue av = AV_READ(ADDR_IY_WRITE(arg));
    write8(av.addr, av.value); // RMWW
    return av;
}

void Cpu::ZN_UPDATE(uint8_t value)
{
    P_.Z = value == 0;
    P_.N = (value&0x80) != 0;
}

void Cpu::LDA(uint8_t value)
{
    A_ = value;
    ZN_UPDATE(A_);
}

void Cpu::LDX(uint8_t value)
{
    X_ = value;
    ZN_UPDATE(X_);
}

void Cpu::LDY(uint8_t value)
{
    Y_ = value;
    ZN_UPDATE(Y_);
}

void Cpu::ADC(uint8_t value)
{
    unsigned int result = A_ + value + P_.C;

    P_.C = (result&0x100) != 0;
    P_.V = (((A_^value)&0x80)^0x80) && ((A_^result)&0x80);

    A_ = result & 0xFF;
    ZN_UPDATE(A_);
}

void Cpu::SBC(uint8_t value)
{
    unsigned int result = A_ - value - !P_.C;

    P_.C = (result&0x100) == 0;
    P_.V = ((A_^value)&(A_^result)&0x80) != 0;

    A_ = result & 0xFF;
    ZN_UPDATE(A_);
}

void Cpu::ORA(uint8_t value)
{
    A_ |= value;
    ZN_UPDATE(A_);
}

void Cpu::AND(uint8_t value)
{
    A_ &= value;
    ZN_UPDATE(A_);
}

void Cpu::EOR(uint8_t value)
{
    A_ ^= value;
    ZN_UPDATE(A_);
}

void Cpu::ASL_DO(uint8_t& value)
{
    P_.C = (value&0x80) != 0;
    value <<= 1;
    ZN_UPDATE(value);
}

void Cpu::ASL()
{
    ASL_DO(A_);
}

void Cpu::ASL(AddrValue av)
{
    ASL_DO(av.value);
    AV_WRITE(av);
}

void Cpu::LSR_DO(uint8_t& value)
{
    P_.C = value & 1;
    value >>= 1;
    ZN_UPDATE(value);
}

void Cpu::LSR()
{
    LSR_DO(A_);
}

void Cpu::LSR(AddrValue av)
{
    LSR_DO(av.value);
    AV_WRITE(av);
}

void Cpu::ROL_DO(uint8_t& value)
{
    bool c_result = value & 0x80;
    value <<= 1;
    value |= P_.C;
    P_.C = c_result;
    ZN_UPDATE(value);
}

void Cpu::ROL()
{
    ROL_DO(A_);
}

void Cpu::ROL(AddrValue av)
{
    ROL_DO(av.value);
    AV_WRITE(av);
}

void Cpu::ROR_DO(uint8_t& value)
{
    bool c_result = value & 1;
    value >>= 1;
    value |= P_.C << 7;
    P_.C = c_result;
    ZN_UPDATE(value);
}

void Cpu::ROR()
{
    ROR_DO(A_);
}

void Cpu::ROR(AddrValue av)
{
    ROR_DO(av.value);
    AV_WRITE(av);
}

void Cpu::BIT(uint8_t value)
{
    P_.Z = (A_&value) == 0;

    Status p(value);
    P_.V = p.V;
    P_.N = p.N;
}

void Cpu::INC_DO(uint8_t& value)
{
    ++value;
    ZN_UPDATE(value);
}

void Cpu::INC(AddrValue av)
{
    INC_DO(av.value);
    AV_WRITE(av);
}

void Cpu::DEC_DO(uint8_t& value)
{
    --value;
    ZN_UPDATE(value);
}

void Cpu::DEC(AddrValue av)
{
    DEC_DO(av.value);
    AV_WRITE(av);
}

void Cpu::CMP_DO(uint8_t lhs, uint8_t rhs)
{
    unsigned int result = lhs - rhs;
    P_.C = (result&0x100) == 0;
    ZN_UPDATE(result & 0xFF);
}

void Cpu::CMP(uint8_t value)
{
    CMP_DO(A_, value);
}

void Cpu::CPX(uint8_t value)
{
    CMP_DO(X_, value);
}

void Cpu::CPY(uint8_t value)
{
    CMP_DO(Y_, value);
}

void Cpu::BRANCH(uint16_t arg, bool cond)
{
    if(cond){
        delay(1);
        int8_t disp = static_cast<int8_t>(arg);
        uint16_t dst = PC_ + disp;
        if((PC_ ^ dst) & 0x100)
            delay(1);
        PC_ = dst;
    }
}

void Cpu::JMP_AB(uint16_t arg)
{
    PC_ = arg;
}

void Cpu::JMP_IND(uint16_t arg)
{
    PC_ = read16_inpage(arg);
}

void Cpu::JSR(uint16_t arg)
{
    push16((PC_-1) & 0xFFFF);
    PC_ = arg;
}

void Cpu::RTS()
{
    PC_ = pop16();
    ++PC_;
}

void Cpu::RTI()
{
    POP_P();
    PC_ = pop16();
}

void Cpu::POP_P()
{
    // ignore bit5-4
    Status p(pop8());
    P_.C = p.C;
    P_.Z = p.Z;
    P_.I = p.I;
    P_.D = p.D;
    P_.V = p.V;
    P_.N = p.N;
}

void Cpu::PUSH_P(bool b4)
{
    Status p = P_;
    p.b4 = b4;
    push8(p.raw);
}
