

#include "nes.hpp"
#include "mapper.hpp"
#include "util.hpp"

using namespace std;

Nes::Nes(const Prg& prg, const Chr& chr, NtMirror ntMirror)
    : mapper_(prg, chr, ntMirror)
{
    hardReset();
}

void Nes::hardReset()
{
    mapper_.hardReset();
}

void Nes::softReset()
{
    mapper_.softReset();
}

void Nes::setInput(InputPort port, unsigned int value)
{
    mapper_.setInput(port, value);
}

void Nes::emulateFrame()
{
    mapper_.emulateFrame();
}

auto Nes::screen() const -> const Screen& { return mapper_.screen(); }


auto Nes::soundSq1() const -> SoundSq  { return mapper_.soundSq1(); }
auto Nes::soundSq2() const -> SoundSq  { return mapper_.soundSq2(); }
auto Nes::soundTri() const -> SoundTri { return mapper_.soundTri(); }
