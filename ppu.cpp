#include <algorithm>
#include <memory>
#include <cstdint>

#include "ppu.hpp"
#include "util.hpp"

using namespace std;


Ppu::Door::~Door() {}

Ppu::Ppu(const shared_ptr<Door>& door) : door_(door)
{
    
}

void Ppu::hardReset()
{
    oam_.fill(0);
    pltram_.fill(0);

    ctrl_.raw = 0;
    mask_.raw = 0;
    status_.raw = 0;
    oamAddr_ = 0;

    regV_.raw = 0;
    regT_.raw = 0;
    regX_ = 0;
    regW_ = false;

    readBuf_ = 0;

    genLatch_ = 0;
}

void Ppu::softReset()
{
    ctrl_.raw = 0;
    mask_.raw = 0;

    regV_.raw = 0;
    regT_.raw = 0;
    regX_ = 0;
    regW_ = false;

    readBuf_ = 0;

    genLatch_ = 0;
}

#if 0
void Ppu::startFrame()
{
    status_.vbl = 0;

    // NesDevWikiだと一部のbitはコピーしないような書き方だけど…
    if(isRenderingOn())
        regV_ = regT_;
}
#endif

bool Ppu::nmiEnabled() const { return ctrl_.nmi; }
void Ppu::setSprOver(bool b) { status_.spr_over = b; }
void Ppu::setSpr0Hit(bool b) { status_.spr0_hit = b; }
void Ppu::setVBlank(bool b)  { status_.vbl = b; }

void Ppu::resetOamAddr()
{
    oamAddr_ = oamAddrLo_ = 0;
}

void Ppu::reloadAddr()
{
    if(isRenderingOn()) regV_ = regT_;
}

void Ppu::startLine()
{
    if(isRenderingOn()){
        // x方向のスクロール初期化(nt の bit0, および x_coarse 復帰)
        regV_.x_coarse = regT_.x_coarse;
        regV_.nt = (regV_.nt&2) | (regT_.nt&1);
    }
}

void Ppu::doLine(int line, uint8_t* buf)
{
    renderLine(line, buf);

    if(checkSpr0(line))
        status_.spr0_hit = true;
}

void Ppu::endLine()
{
    if(!isRenderingOn()) return;

    // y increment
    if(regV_.y_fine != 7){
        ++regV_.y_fine;
    }
    else{
        regV_.y_fine = 0;
        if(regV_.y_coarse == 29){
            regV_.y_coarse = 0;
            regV_.nt ^= 2;
        }
        else if(regV_.y_coarse == 31){
            regV_.y_coarse = 0;
        }
        else{
            ++regV_.y_coarse;
        }
    }
}

#if 0
void Ppu::startVBlank()
{
    status_.vbl = 1;

    if(ctrl_.nmi)
        door_->triggerNmi();

    status_.spr0_hit = false; // 本来はpre-renderのdot1でクリアされるらしい
}
#endif

void Ppu::oamDma(const uint8_t buf[0x100])
{
    // 開始アドレスは OAMADDR に依存するのかもしれないが、とりあえず
    // FCEUXと同じ実装にしておく
    copy(buf, buf+0x100, oam_.begin());
}

uint8_t Ppu::read200x()
{
    return genLatch_;
}

uint8_t Ppu::read2002()
{
    Status ret = status_;
    ret.garbage = genLatch_ & 0x1F;

    status_.vbl = 0;
    regW_ = false;

    genLatch_ = ret.raw;
    return ret.raw;
}

uint8_t Ppu::read2004()
{
    return genLatch_; // FCEUX oldppuと同じ
}

uint8_t Ppu::read2007()
{
    uint8_t ret;

    if(regV_.addr < 0x3F00){
        ret = genLatch_ = readBuf_;
        readBuf_ = door_->readPpu(regV_.addr);
    }
    else{ // PLTRAM (not buffered)
        ret = readPltram(regV_.addr);
        readBuf_ = door_->readPpu(regV_.addr - 0x1000);
    }

    // VBLANK外で読み取った際の regV_ の挙動は省略
    regV_.addr += ctrl_.inc32 ? 32 : 1;

    return ret;
}

void Ppu::write2000(uint8_t value)
{
    genLatch_ = value;

    bool nmi_prev = ctrl_.nmi;
    ctrl_.raw = value;
    if(!nmi_prev && ctrl_.nmi && status_.vbl)
        door_->triggerNmi();

    regT_.nt = ctrl_.nt;
}

void Ppu::write2001(uint8_t value)
{
    genLatch_ = value;
    mask_.raw = value;
}

void Ppu::write2002(uint8_t value)
{
    genLatch_ = value;
}

void Ppu::write2003(uint8_t value)
{
    genLatch_ = value;

    oamAddr_   = value;
    oamAddrLo_ = value & 7;
}

void Ppu::write2004(uint8_t value)
{
    genLatch_ = value;

    if(oamAddrLo_ < 8){
        oam_[oamAddrLo_] = value;
    }
    else{
        if(oamAddr_ >= 8) oam_[oamAddr_] = value;
    }

    ++oamAddr_;
    ++oamAddrLo_;
}

void Ppu::write2005(uint8_t value)
{
    genLatch_ = value;

    if(!regW_){ // 1st
        regT_.x_coarse = value >> 3;
        regX_          = value & 7;
    }
    else{ // 2nd
        regT_.y_coarse = value >> 3;
        regT_.y_fine   = value & 7;
    }

    regW_ ^= 1;
}

void Ppu::write2006(uint8_t value)
{
    genLatch_ = value;

    if(!regW_){ // 1st
        regT_.raw &= ~(1<<14);
        regT_.addr_hi = value & 0x3F;
    }
    else{ // 2nd
        regT_.addr_lo = value;
        regV_ = regT_;
    }

    regW_ ^= 1;
}

// FCEUX oldppuと同じ実装
void Ppu::write2007(uint8_t value)
{
    genLatch_ = value;

    door_->writePpu(regV_.addr, value);

    regV_.addr += ctrl_.inc32 ? 32 : 1;
}

uint8_t Ppu::readPltram(uint16_t addr) const
{
    if(addr & 3){
        return READPLT(addr & 0x1F);
    }
    else{ // $3F00, $3F04, ...
        return READPLT(addr & 0xC);
    }
}

void Ppu::writePltram(uint16_t addr, uint8_t value)
{
    if(addr & 3){
        pltram_[addr & 0x1F] = value & 0x3F;
    }
    else{ // $3F00, $3F04, ...
        pltram_[addr & 0xC] = value & 0x3F;
    }
}

void Ppu::renderLine(int line, uint8_t* buf)
{
    /**
     * 処理の簡略化のため、前後8pxの余裕を設けたバッファを使い、後で結
     * 果をbuf へ書き戻す(renderLineBg() のコメントも参照)
     */
    uint8_t tmpbuf[8 + 0x100 + 8]; // 描画用バッファ
    bool opacity[8 + 0x100 + 8];   // 不透明BGフラグ

    if(mask_.bg_on){
        renderLineBg(line, tmpbuf+8, opacity+8);
    }
    else{
        // BGオフなら全て背景色とする
        fill_n(tmpbuf+8, 0x100, READPLT(0));
        fill_n(opacity+8, 0x100, false);
    }

    if(mask_.spr_on) renderLineSpr(line, tmpbuf+8, opacity+8);

    // 書き戻し
    copy_n(tmpbuf+8, 0x100, buf);
}

namespace{
    uint8_t PATTERN_PIXEL(uint8_t lo, uint8_t hi, int offset)
    {
        int shift = 7 - offset;
        lo >>= shift;
        hi >>= shift;
        return (lo&1) | ((hi&1)<<1);
    }
}

// buf, opacity は前後8pxの余裕あり
void Ppu::renderLineBg(int /*line*/, uint8_t* buf, bool* opacity)
{
    uint16_t pat_base = ctrl_.bg_pat ? 0x1000 : 0x0000;

    /**
     * スクロールオフセットはタイル境界に合っていないこともあるので、
     * 1ラインの描画には都合33個のタイルを処理する必要がある。そこで、
     * 前後8pxの余裕を設けたバッファに33タイル全体を書き込み、そこから
     * 中央256pxのみスクリーンバッファへ書き戻す。
     */
    uint8_t* p = buf - regX_;
    bool* op = opacity - regX_;
    for(int i = 0; i < 33; ++i, p+=8, op+=8){
        uint8_t tile = door_->readPpu(0x2000 + (regV_.addr&0xFFF));

        uint16_t pat_addr = pat_base + 16*tile + regV_.y_fine;
        uint8_t pat_lo = door_->readPpu(pat_addr);
        uint8_t pat_hi = door_->readPpu(pat_addr + 8);

        uint8_t attr_block = door_->readPpu(
            0x23C0 + 0x400*regV_.nt + 8*(regV_.y_coarse>>2) + (regV_.x_coarse>>2));
        uint8_t attr_shift = ((regV_.y_coarse&2) ? 4 : 0) + ((regV_.x_coarse&2) ? 2 : 0);
        uint8_t attr = (attr_block>>attr_shift) & 3;

        // タイル描画
        for(int j = 0; j < 8; ++j){
            uint8_t px = PATTERN_PIXEL(pat_lo, pat_hi, j);

            if(px){
                p[j] = READPLT((attr<<2) | px);
                op[j] = true;
            }
            else{
                p[j] = READPLT(0);
                op[j] = false;
            }
        }

        // x increment
        if(regV_.x_coarse == 31){
            regV_.x_coarse = 0;
            regV_.nt ^= 1;
        }
        else{
            ++regV_.x_coarse;
        }
    }
}

namespace{
    union SpriteAttr{
        uint8_t raw;
        explicit SpriteAttr(uint8_t value=0) : raw(value) {}
        SpriteAttr& operator=(const SpriteAttr& rhs) { raw = rhs.raw; return *this; }
        BitField8<0,2> plt;
        BitField8<5>   bg;
        BitField8<6>   flip_h;
        BitField8<7>   flip_v;
    };

    struct Sprite{
        uint8_t x;
        uint8_t y;
        SpriteAttr attr;
        uint8_t h;
        uint8_t tile;
        uint16_t pat_base;

        Sprite(uint8_t oam[4], bool spr16, bool spr_pat)
        {
            x = oam[3];
            y = oam[0] + 1;
            attr.raw = oam[2];

            if(spr16){
                h = 16;
                tile = oam[1] & ~1;
                pat_base = (oam[1]&1) ? 0x1000 : 0x0000;
            }
            else{
                h = 8;
                tile = oam[1];
                pat_base = spr_pat ? 0x1000 : 0x0000;
            }
        }
    };
}

// スプライトオーバーはとりあえず無視
// buf は前後8pxの余裕あり
void Ppu::renderLineSpr(int line, uint8_t* buf, const bool* opacity)
{
    // line 0ではスプライトは決して表示されない
    if(line == 0) return;

    // スプライト描画済フラグ
    bool spr_drawn[0x100] = {};

    // 全スプライト64個に対して処理
    for(int i = 0; i < 64; ++i){
        Sprite spr(oam_.data() + 4*i, ctrl_.spr16, ctrl_.spr_pat);

        // y==0 (oam[0]==0xFF) なスプライトは表示されない
        if(spr.y == 0) continue;
        // スプライトがライン上になければ次のスプライトへ
        if(!(spr.y <= line && line < spr.y + spr.h)) continue;

        int y_offset = line - spr.y;
        if(spr.attr.flip_v)
            y_offset = spr.h-1 - y_offset;

        uint8_t tile = spr.tile + (y_offset >= 8 ? 1 : 0);

        uint16_t pat_addr = spr.pat_base + 16*tile + (y_offset&7);
        uint8_t pat_lo = door_->readPpu(pat_addr);
        uint8_t pat_hi = door_->readPpu(pat_addr + 8);

        for(int j = 0; j < 8; ++j){
            int x_offset = spr.attr.flip_h ? 7-j : j;
            uint8_t px = PATTERN_PIXEL(pat_lo, pat_hi, x_offset);

            int x = spr.x + j;
            if(px && !spr_drawn[x]){
                if(!spr.attr.bg || !opacity[x])
                    buf[x] = READPLT(0x10 | (spr.attr.plt<<2) | px);
                // 背面スプライトであっても優先度には影響しない
                spr_drawn[x] = true;
            }
        }
    }
}

// bjneからパクったけど正しくない
//   * BGも見なければならない
//   * クリッピングが有効な場合、x=0 から x=7 では起こらない
//   * x=255 では起こらない
//   * 1フレームに1回しか起こらない
bool Ppu::checkSpr0(int line)
{
    if(!mask_.bg_on || !mask_.spr_on) return false;

    Sprite spr(oam_.data(), ctrl_.spr16, ctrl_.spr_pat);
    if(spr.y == 0) return false;
    if(!(spr.y <= line && line < spr.y + spr.h)) return false;

    int y_offset = line - spr.y;
    if(spr.attr.flip_v)
        y_offset = spr.h-1 - y_offset;

    uint8_t tile = spr.tile + (y_offset >= 8 ? 1 : 0);

    uint16_t pat_addr = spr.pat_base + 16*tile + (y_offset&7);
    uint8_t pat_lo = door_->readPpu(pat_addr);
    uint8_t pat_hi = door_->readPpu(pat_addr + 8);

    return pat_lo || pat_hi;
}

bool Ppu::isRenderingOn() const
{
    return mask_.bg_on || mask_.spr_on;
}

uint8_t Ppu::READPLT(int offset) const
{
    // グレースケールはよくわからないので省略
    return pltram_[offset];
}
