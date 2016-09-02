#pragma once



#include "mapper.hpp"

class Nes{
public:
    using Prg = Mapper::Prg;
    using Chr = Mapper::Chr;
    using NtMirror = Mapper::NtMirror;

    using InputPort = Mapper::InputPort;
    static constexpr unsigned int BUTTON_A = Mapper::BUTTON_A;
    static constexpr unsigned int BUTTON_B = Mapper::BUTTON_B;
    static constexpr unsigned int BUTTON_S = Mapper::BUTTON_S;
    static constexpr unsigned int BUTTON_T = Mapper::BUTTON_T;
    static constexpr unsigned int BUTTON_U = Mapper::BUTTON_U;
    static constexpr unsigned int BUTTON_D = Mapper::BUTTON_D;
    static constexpr unsigned int BUTTON_L = Mapper::BUTTON_L;
    static constexpr unsigned int BUTTON_R = Mapper::BUTTON_R;

    using Screen = Mapper::Screen;

    Nes(const Prg& prg, const Chr& chr, NtMirror ntMirror);

    void hardReset();
    void softReset();

    void setInput(InputPort port, unsigned int value);

    void emulateFrame();

    const Screen& screen() const;

    using SoundSq  = Mapper::SoundSq;
    using SoundTri = Mapper::SoundTri;
    SoundSq  soundSq1() const;
    SoundSq  soundSq2() const;
    SoundTri soundTri() const;

private:
    Mapper mapper_;
};
