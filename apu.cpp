#include <algorithm>
#include <memory>
#include <cstdint>
#include <cassert>

#include "apu.hpp"
#include "util.hpp"

using namespace std;

namespace{
    constexpr uint8_t LENGTH_TABLE[0x20] = {
        10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
        12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
    };
}


Apu::Door::~Door() {}

Apu::Apu(const shared_ptr<Door>& door)
    : door_(door),
      sq1_(Square(Square::Ch::SQ1)), sq2_(Square(Square::Ch::SQ2)), dmc_(door_)
{
    
}

void Apu::hardReset()
{
    sq1_.hardReset();
    sq2_.hardReset();
    tri_.hardReset();
    noi_.hardReset();
    dmc_.hardReset();

    nextStep_   = 0;          // FCEUXに合わせたが正しいのだろうか(初期stepは3になる)
    restCycle_  = STEP_CYCLE;
    frameIrqOn_ = true;       // FCEUXに合わせた
    step5_      = false;
}

void Apu::softReset()
{
    sq1_.softReset();
    sq2_.softReset();
    tri_.softReset();
    noi_.softReset();
    dmc_.softReset();

    nextStep_   = 0;
    restCycle_  = STEP_CYCLE;
    frameIrqOn_ = true;
    step5_      = false;
}

void Apu::tick(int cycle)
{
    restCycle_ -= 48 * cycle;
    if(restCycle_ <= 0) updateStep();

    dmc_.tick(cycle, soundTimestamp_);

    soundTimestamp_ += cycle;
}


void Apu::updateStep()
{
    sq1_.genSound(soundTimestamp_);
    sq2_.genSound(soundTimestamp_);
    tri_.genSound(soundTimestamp_);
    noi_.genSound(soundTimestamp_);

    if(frameIrqOn_ && nextStep_ == 0 && !step5_){
        frameIrqOn_ = false;
        door_->triggerFrameIrq();
    }

    frameQuarter();
    if(nextStep_ == 0 || nextStep_ == 2) frameHalf();

    // サイクル、step更新
    // 5-stepの場合、step3 -> step0 のサイクルを倍にして対処
    restCycle_ += STEP_CYCLE;
    if(step5_ && nextStep_ == 3) restCycle_ += STEP_CYCLE;
    nextStep_ = (nextStep_ + 1) & 3;
}

// envelope, linear counter
void Apu::frameQuarter()
{
    sq1_.frameQuarter();
    sq2_.frameQuarter();
    tri_.frameQuarter();
    noi_.frameQuarter();
}

// length counter, sweep
void Apu::frameHalf()
{
    sq1_.frameHalf();
    sq2_.frameHalf();
    tri_.frameHalf();
    noi_.frameHalf();
}


Apu::Square::Square(Ch which) : which_(which) {}

void Apu::Square::hardReset()
{
    duty_ = 0;
    lengthHalt_ = false;

    envelope_.start       = false;
    envelope_.loop        = false;
    envelope_.constant    = false;
    envelope_.divider     = 0;
    envelope_.divider_reg = 0;
    envelope_.volume      = 0;
    envelope_.decay_level = 0;

    sweep_.divider     = 0; // FCEUXは static 初期化に頼ってる
    sweep_.divider_reg = 0;
    sweep_.nega        = 0;
    sweep_.shift       = 0;

    softReset();
}

void Apu::Square::softReset()
{
    enabled_ = false;

    // FCEUXの真似だがソース不明
    // なお、FCEUXはタイマ値が1から0になるときシーケンサのステップを進
    // めるので値が1ずれて 0x800 になっている
    timer_ = 0x7FF;

    timerReg_.raw = 0;
    length_ = 0;

    sweep_.enabled = false;

    // FCEUXはハードリセットでもシーケンサのステップ数を初期化していな
    // い(static 変数なのでエミュレータ起動時の初期値は0)が、少なくと
    // もハードリセット時の値は固定にしたい
    step_ = 0;
}

void Apu::Square::enable(bool enabled, int timestamp)
{
    genSound(timestamp);

    if(!enabled)
        length_ = 0;

    enabled_ = enabled;
}

bool Apu::Square::isActive() const
{
    return length_;
}

// envelope
void Apu::Square::frameQuarter()
{
    if(envelope_.start){
        envelope_.start = false;
        envelope_.decay_level = 0xF;
        envelope_.divider = envelope_.divider_reg;
    }
    else{
        if(!envelope_.divider){
            envelope_.divider = envelope_.divider_reg;
            if(envelope_.decay_level){
                --envelope_.decay_level;
            }
            else if(envelope_.loop){
                envelope_.decay_level = 0xF;
            }
        }
        else{
            --envelope_.divider;
        }
    }
}

// length counter, sweep
void Apu::Square::frameHalf()
{
    if(!lengthHalt_ && length_)
        --length_;

    // FCEUXの真似。NesDevWikiの記述とは一致してないかも
    if(sweep_.enabled){
        if(!sweep_.divider){
            sweep_.divider = sweep_.divider_reg;
            if(sweep_.nega){
                if(sweep_.shift){
                    unsigned int sub = (timerReg_.raw>>sweep_.shift) + (which_==Ch::SQ1 ? 1 : 0);
                    timerReg_.raw = timerReg_.raw > sub ? timerReg_.raw - sub : 0;
                }
            }
            else{
                unsigned int add = timerReg_.raw>>sweep_.shift;
                if(timerReg_.raw + add > 0x7FF){
                    timerReg_.raw = 0;
                    sweep_.enabled = false;
                }
                else if(sweep_.shift){
                    timerReg_.raw += add;
                }
            }
        }
        else{
            --sweep_.divider;
        }
    }
}

void Apu::Square::write4000or4004(uint8_t value, int timestamp)
{
    // パラメータが変わるのでここまでの音声データを出力
    genSound(timestamp);

    duty_                                    = value >> 6;
    lengthHalt_ = envelope_.loop             = value & 0x20;
    envelope_.constant                       = value & 0x10;
    envelope_.volume = envelope_.divider_reg = value & 0x0F;
}

void Apu::Square::write4001or4005(uint8_t value, int /*timestamp*/)
{
    // TODO: FCEUXではこうなってるが、genSound() が不要な理由は?
    sweep_.enabled     = value & 0x80;
    sweep_.divider_reg = (value>>4) & 7;
    sweep_.nega        = value & 0x08;
    sweep_.shift       = value & 0x07;
}

void Apu::Square::write4002or4006(uint8_t value, int timestamp)
{
    // 周波数が変わるのでここまでの音声データを出力
    genSound(timestamp);

    timerReg_.lo = value;
}

void Apu::Square::write4003or4007(uint8_t value, int timestamp)
{
    // FCEUXの真似だが正しいかどうか確信なし
    // なぜ genSound() が必須でないのか?
    if(enabled_){
        genSound(timestamp);
        length_ = LENGTH_TABLE[value>>3];
    }

    timerReg_.hi = value & 7;

    // TODO: FCEUXだと sweep_.enabled と sweep_.divider をバックアップ
    //       から復帰するようになってるが、NesDevWikiにそういう記述っ
    //       てあるか?
    

    envelope_.start = true;

    step_= 7;
}


void Apu::Triangle::hardReset()
{
    timerReg_.raw = 0;
    lengthHalt_ = false;
    linearReg_ = 0;
    control_ = false;

    softReset();
}

void Apu::Triangle::softReset()
{
    enabled_ = false;
    timer_ = 0; // FCEUXの真似だがソース不明
    length_ = 0;
    linear_ = 0;
    linearReload_ = false;
    step_ = 0;
}

void Apu::Triangle::enable(bool enabled, int timestamp)
{
    genSound(timestamp);

    if(!enabled)
        length_ = 0;

    enabled_ = enabled;
}

bool Apu::Triangle::isActive() const
{
    return length_;
}

// linear counter
void Apu::Triangle::frameQuarter()
{
    if(linearReload_)
        linear_ = linearReg_;
    else if(linear_)
        --linear_;

    if(!control_)
        linearReload_ = false;
}

// length counter
void Apu::Triangle::frameHalf()
{
    if(!lengthHalt_ && length_)
        --length_;
}

void Apu::Triangle::write4008(uint8_t value, int /*timestamp*/)
{
    linearReg_             = value & 0x7F;
    lengthHalt_ = control_ = value & 0x80;
}

void Apu::Triangle::write400A(uint8_t value, int timestamp)
{
    // 周波数が変わるのでここまでの音声データを出力
    genSound(timestamp);

    timerReg_.lo = value;
}

void Apu::Triangle::write400B(uint8_t value, int timestamp)
{
    // 周波数が変わるのでここまでの音声データを出力
    genSound(timestamp);

    if(enabled_)
        length_ = LENGTH_TABLE[value>>3];

    timerReg_.hi = value & 7;

    linearReload_ = true;
}


void Apu::Noise::hardReset()
{
    timerReg_ = 0;
    lengthHalt_ = false;

    envelope_.start       = false;
    envelope_.loop        = false;
    envelope_.constant    = false;
    envelope_.divider     = 0;
    envelope_.divider_reg = 0;
    envelope_.volume      = 0;
    envelope_.decay_level = 0;

    lfsr_.mode_short = false;

    softReset();
}

void Apu::Noise::softReset()
{
    enabled_ = false;

    // FCEUXの真似だがソース不明
    // なお、FCEUXはタイマ値が1から0になるときシーケンサのステップを進
    // めるので値が1ずれて 0x800 になっている
    timer_ = 0x7FF;

    length_ = 0;

    lfsr_.reg = 0x4000; // FCEUXに合わせた(FCEUXはビット逆順)。NesDevWikiでは起動時1となってる
}

void Apu::Noise::enable(bool enabled, int timestamp)
{
    genSound(timestamp);

    if(!enabled)
        length_ = 0;

    enabled_ = enabled;
}

bool Apu::Noise::isActive() const
{
    return length_;
}

// envelope
void Apu::Noise::frameQuarter()
{
    if(envelope_.start){
        envelope_.start = false;
        envelope_.decay_level = 0xF;
        envelope_.divider = envelope_.divider_reg;
    }
    else{
        if(!envelope_.divider){
            envelope_.divider = envelope_.divider_reg;
            if(envelope_.decay_level){
                --envelope_.decay_level;
            }
            else if(envelope_.loop){
                envelope_.decay_level = 0xF;
            }
        }
        else{
            --envelope_.divider;
        }
    }
}

// length counter
void Apu::Noise::frameHalf()
{
    if(!lengthHalt_ && length_)
        --length_;
}

void Apu::Noise::write400C(uint8_t value, int timestamp)
{
    // パラメータが変わるのでここまでの音声データを出力
    genSound(timestamp);

    lengthHalt_ = envelope_.loop             = value & 0x20;
    envelope_.constant                       = value & 0x10;
    envelope_.volume = envelope_.divider_reg = value & 0x0F;
}

void Apu::Noise::write400E(uint8_t value, int timestamp)
{
    genSound(timestamp);

    lfsr_.mode_short = value & 0x80;
    timerReg_        = value & 0x0F;
}

void Apu::Noise::write400F(uint8_t value, int timestamp)
{
    genSound(timestamp);

    if(enabled_)
        length_ = LENGTH_TABLE[value>>3];

    envelope_.start = true;
}


Apu::Dmc::Dmc(const shared_ptr<Door>& door) : door_(door) {}

void Apu::Dmc::hardReset()
{
    

    softReset();
}

void Apu::Dmc::softReset()
{
    irq_ = false;
    loop_ = false;
    periodReg_ = 0;

    reader_.addr_reg = 0;
    reader_.size_reg = 0;
    reader_.addr = 0;
    reader_.size = 0;
    reader_.has_sample = false;

    out_.level = 0;
    out_.reg = 0;
    out_.rest_bits = 0;

    timestamp_ = 0;
}

void Apu::Dmc::enable(bool enabled, int timestamp)
{
    genSound(timestamp);

    if(enabled){
        if(!reader_.size){
            // TODO: ここは tick() と共通のコードなので関数化
            reader_.addr = 0xC000 + (reader_.addr_reg<<6);
            reader_.size = (reader_.size_reg<<4) + 1;
        }
    }
    else{
        reader_.size = 0;
    }

    irq_ = false;
}

bool Apu::Dmc::isActive() const
{
    return reader_.size;
}

bool Apu::Dmc::irqEnabled() const
{
    return irq_;
}

namespace{
    constexpr unsigned int DMC_PERIOD_TABLE[0x10] = {
        428, 380, 340, 320, 286, 254, 226, 214,
        190, 160, 142, 128, 106,  84,  72,  54
    };
}

void Apu::Dmc::tick(int cycle, int sound_timestamp)
{
    // DMA
    if(reader_.size && !reader_.has_sample){
        reader_.sample = door_->readDmc(reader_.addr);
        reader_.has_sample = true;
        reader_.addr = reader_.addr==0xFFFF ? 0x8000 : reader_.addr+1;
        --reader_.size;
        if(!reader_.size){
            if(loop_){
                // TODO: この部分は関数化
                reader_.addr = 0xC000 + (reader_.addr_reg<<6);
                reader_.size = (reader_.size_reg<<4) + 1;
            }
            else{
                if(irq_) door_->triggerDmcIrq();
            }
        }
    }

    timestamp_ -= cycle;
    while(timestamp_ < 0){
        if(out_.rest_bits){
            genSound(sound_timestamp + timestamp_); // CPUに対する遅れを補正

            bool bit = out_.reg & 1;
            if(bit && out_.level <= 0x7D)
                out_.level += 2;
            else if(!bit && 2 <= out_.level)
                out_.level -= 2;

            --out_.rest_bits;
            out_.reg >>= 1;
        }
        else{
            if(reader_.has_sample){
                out_.reg = reader_.sample;
                out_.rest_bits = 8;
                reader_.has_sample = false;
            }
        }

        timestamp_ += DMC_PERIOD_TABLE[periodReg_];
    }
}

void Apu::Dmc::write4010(uint8_t value, int timestamp)
{
    genSound(timestamp);

    if(irq_){
        if(value & 0x80)
            door_->triggerDmcIrq();
        else
            irq_ = false;
    }

    irq_       = (value & 0x80) == 1;
    loop_      = (value & 0x40) == 1;
    periodReg_ = value & 0x0F;
}

void Apu::Dmc::write4011(uint8_t value, int timestamp)
{
    genSound(timestamp);

    out_.level = value & 0x7F;
}

void Apu::Dmc::write4012(uint8_t value, int /*timestamp*/)
{
    reader_.addr_reg = value;
}

void Apu::Dmc::write4013(uint8_t value, int /*timestamp*/)
{
    reader_.size_reg = value;
}


uint8_t Apu::read4015()
{
    Status st;
    st.sq1       = sq1_.isActive();
    st.sq2       = sq2_.isActive();
    st.tri       = tri_.isActive();
    st.noi       = noi_.isActive();
    st.dmc       = dmc_.isActive();
    st.frame_irq = frameIrqOn_;
    st.dmc_irq   = dmc_.irqEnabled();

    frameIrqOn_ = false;
    // TODO: FCEUXみたいに実際のIRQ解除もすべき?

    return st.raw;
}


void Apu::write4000(uint8_t value) { sq1_.write4000or4004(value, soundTimestamp_); }
void Apu::write4001(uint8_t value) { sq1_.write4001or4005(value, soundTimestamp_); }
void Apu::write4002(uint8_t value) { sq1_.write4002or4006(value, soundTimestamp_); }
void Apu::write4003(uint8_t value) { sq1_.write4003or4007(value, soundTimestamp_); }

void Apu::write4004(uint8_t value) { sq2_.write4000or4004(value, soundTimestamp_); }
void Apu::write4005(uint8_t value) { sq2_.write4001or4005(value, soundTimestamp_); }
void Apu::write4006(uint8_t value) { sq2_.write4002or4006(value, soundTimestamp_); }
void Apu::write4007(uint8_t value) { sq2_.write4003or4007(value, soundTimestamp_); }

void Apu::write4008(uint8_t value) { tri_.write4008(value, soundTimestamp_); }
void Apu::write400A(uint8_t value) { tri_.write400A(value, soundTimestamp_); }
void Apu::write400B(uint8_t value) { tri_.write400B(value, soundTimestamp_); }

void Apu::write400C(uint8_t value) { noi_.write400C(value, soundTimestamp_); }
void Apu::write400E(uint8_t value) { noi_.write400E(value, soundTimestamp_); }
void Apu::write400F(uint8_t value) { noi_.write400F(value, soundTimestamp_); }

void Apu::write4010(uint8_t value) { dmc_.write4010(value, soundTimestamp_); }
void Apu::write4011(uint8_t value) { dmc_.write4011(value, soundTimestamp_); }
void Apu::write4012(uint8_t value) { dmc_.write4012(value, soundTimestamp_); }
void Apu::write4013(uint8_t value) { dmc_.write4013(value, soundTimestamp_); }


void Apu::write4015(uint8_t value)
{
    Status st(value);
    sq1_.enable(st.sq1, soundTimestamp_);
    sq2_.enable(st.sq2, soundTimestamp_);
    tri_.enable(st.tri, soundTimestamp_);
    noi_.enable(st.noi, soundTimestamp_);
    dmc_.enable(st.dmc, soundTimestamp_);
}

void Apu::write4017(uint8_t value)
{
    // 5-mode bitが立っている場合、quarter/halfシグナルを生成
    // FCEUXでは一見IRQも生成してるように見えるが、後からなかったこと
    // にしてる
    if(value & 0x80){
        frameQuarter();
        frameHalf();
    }

    nextStep_  = 1;
    restCycle_ = STEP_CYCLE;

    frameIrqOn_ = (value&0x40) == 0;
    step5_      = (value&0x80) == 1;
}


//---------------------------------------------------------------------
// 以下、音声出力の実装(超適当)
//---------------------------------------------------------------------

void Apu::startFrame()
{
    soundTimestamp_ = 0;
    sq1_.startFrame();
    sq2_.startFrame();
    tri_.startFrame();
    noi_.startFrame();
    dmc_.startFrame();
}

void Apu::endFrame()
{
    sq1_.genSound(soundTimestamp_);
    sq2_.genSound(soundTimestamp_);
    tri_.genSound(soundTimestamp_);
    noi_.genSound(soundTimestamp_);
    dmc_.genSound(soundTimestamp_);
}

auto Apu::soundSq1() const -> Apu::SoundSq
{
    return sq1_.sound();
}

auto Apu::soundSq2() const -> Apu::SoundSq
{
    return sq2_.sound();
}

auto Apu::soundTri() const -> Apu::SoundTri
{
    return tri_.sound();
}

auto Apu::soundNoi() const -> Apu::SoundNoi
{
    return noi_.sound();
}

auto Apu::soundDmc() const -> Apu::SoundDmc
{
    return dmc_.sound();
}


void Apu::Square::startFrame()
{
    soundPos_ = 0;
}

auto Apu::Square::sound() const -> Apu::SoundSq
{
    return SoundSq(sound_.data(), soundPos_);
}

namespace{
    // 添字7から開始することを想定している
    constexpr uint8_t SQ_DUTIES[4][8] = {
        { 1, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 1, 0, 0, 0, 0, 0, 0 },
        { 1, 1, 1, 1, 0, 0, 0, 0 },
        { 1, 1, 1, 1, 1, 1, 0, 0 }  // FCEUXに合わせたけど、NesDevWikiだと位相がズレてる
    };

}

bool Apu::Square::checkFreq()
{
    if(!sweep_.nega){
        unsigned int add = timerReg_.raw>>sweep_.shift;
        return timerReg_.raw + add <= 0x7FF;
    }
    else{
        return true;
    }
}

void Apu::Square::genSound(int timestamp)
{
    assert(soundPos_ <= timestamp);

    if(!(8 <= timerReg_.raw && timerReg_.raw <= 0x7FF) ||
       !checkFreq() ||
       !length_){ // silenced
        fill(sound_.begin()+soundPos_, sound_.begin()+timestamp, 0);
        soundPos_ = timestamp;
    }
    else{
        uint8_t amp = envelope_.constant ? envelope_.volume : envelope_.decay_level;
        for(; soundPos_ < timestamp; ++soundPos_){
            sound_[soundPos_] = amp * SQ_DUTIES[duty_][step_];
            if(!timer_){
                timer_ = 2*timerReg_.raw + 1; // 周期はCPUサイクル単位で 2*(t+1) だから…
                step_ = (step_+1) & 7;
            }
            else{
                --timer_;
            }
        }
    }
}


void Apu::Triangle::startFrame()
{
    soundPos_ = 0;
}

auto Apu::Triangle::sound() const -> Apu::SoundTri
{
    return SoundTri(sound_.data(), soundPos_);
}

namespace{
    uint8_t TRI_OUTPUT(unsigned int step)
    {
        // FCEUXの真似
        uint8_t sample = step & 0xF;
        if(!(step & 0x10)) sample ^= 0xF;
        return sample;
    }
}

void Apu::Triangle::genSound(int timestamp)
{
    assert(soundPos_ <= timestamp);

    if(length_ && linear_){
        uint8_t output = TRI_OUTPUT(step_);
        for(; soundPos_ < timestamp; ++soundPos_){
            sound_[soundPos_] = output;
            if(!timer_){
                // step_ はどうせ下位5bitしか見ないので、ここでのマス
                // クは必要ない。オーバーフローしても問題なく動くはず
                timer_ = timerReg_.raw; // 三角波タイマは1CPUサイクルごとに動作
                ++step_;
                output = TRI_OUTPUT(step_);
            }
            else{
                --timer_;
            }
        }
    }
    else{ // silenced
        fill(sound_.begin()+soundPos_, sound_.begin()+timestamp, 0);
        soundPos_ = timestamp;
    }
}


void Apu::Noise::startFrame()
{
    soundPos_ = 0;
}

auto Apu::Noise::sound() const -> SoundNoi
{
    return SoundNoi(sound_.data(), soundPos_);
}

void Apu::Noise::Lfsr::shift()
{
    int shift = mode_short ? 6 : 1;
    unsigned int bit = (reg ^ (reg>>shift)) & 1;
    reg >>= 1;
    reg |= bit << 14;
}

namespace{
    constexpr unsigned int NOI_P_TABLE[0x10] = {
          4,   8,  16,  32,  64,   96,  128,  160,
        202, 254, 380, 508, 762, 1016, 2034, 4068
    };
}

void Apu::Noise::genSound(int timestamp)
{
    assert(soundPos_ <= timestamp);

    uint8_t amp = envelope_.constant ? envelope_.volume : envelope_.decay_level;

    // length counterが0であってもタイマは回さなければならないらしい
    // (FCEUXを読む限りでは)
    uint8_t out = (lfsr_.reg&1) ? 0 : amp;
    for(; soundPos_ < timestamp; ++soundPos_){
        sound_[soundPos_] = length_ ? out : 0;
        // FCEUXではタイマが1->0のときにLFSRを更新してるけど、これだと
        // 周期が1ずれるんじゃないかな…
        if(!timer_){
            timer_ = NOI_P_TABLE[timerReg_];
            lfsr_.shift();
            out = (lfsr_.reg&1) ? 0 : amp;
        }
        else{
            --timer_;
        }
    }
}


void Apu::Dmc::startFrame()
{
    soundPos_ = 0;
}

auto Apu::Dmc::sound() const -> SoundDmc
{
    return SoundDmc(sound_.data(), soundPos_);
}

void Apu::Dmc::genSound(int timestamp)
{
    for(; soundPos_ < timestamp; ++soundPos_){
        sound_[soundPos_] = out_.level;
    }
}
