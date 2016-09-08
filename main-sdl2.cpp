#include <string>
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cassert>

#include <alloca.h>

#include <boost/lockfree/spsc_queue.hpp>

#include <SDL.h>

#include "junknes.h"
#include "ines.hpp"
#include "util.hpp"

using namespace std;

namespace{
    template<typename T>
    bool allequal(const T& x, const T& y) { return x == y; }
    template<typename T, typename... Args>
    bool allequal(const T& x, const T& y, Args... tail) { return x == y && allequal(y, tail...); }

    struct InputMap{
        int scancode;
        int port;
        unsigned int button;
    };
    constexpr InputMap INPUT_MAP[] = {
        { SDL_SCANCODE_D, 0, JUNKNES_JOY_R },
        { SDL_SCANCODE_A, 0, JUNKNES_JOY_L },
        { SDL_SCANCODE_S, 0, JUNKNES_JOY_D },
        { SDL_SCANCODE_W, 0, JUNKNES_JOY_U },
        { SDL_SCANCODE_E, 0, JUNKNES_JOY_T },
        { SDL_SCANCODE_Q, 0, JUNKNES_JOY_S },
        { SDL_SCANCODE_X, 0, JUNKNES_JOY_B },
        { SDL_SCANCODE_Z, 0, JUNKNES_JOY_A },
    };

    constexpr int             AUDIO_FREQ     = 44100;
    constexpr SDL_AudioFormat AUDIO_FORMAT   = AUDIO_S16SYS;
    constexpr int             AUDIO_CHANNELS = 1;
    constexpr int             AUDIO_SAMPLES  = 4096; // これ実際にはバッファサイズだと思う

    constexpr uint32_t PALETTE_RGBA[0x40] = {
        0x747474FF, 0x24188CFF, 0x0000A8FF, 0x44009CFF,
        0x8C0074FF, 0xA80010FF, 0xA40000FF, 0x7C0800FF,
        0x402C00FF, 0x004400FF, 0x005000FF, 0x003C14FF,
        0x183C5CFF, 0x000000FF, 0x000000FF, 0x000000FF,

        0xBCBCBCFF, 0x0070ECFF, 0x2038ECFF, 0x8000F0FF,
        0xBC00BCFF, 0xE40058FF, 0xD82800FF, 0xC84C0CFF,
        0x887000FF, 0x009400FF, 0x00A800FF, 0x009038FF,
        0x008088FF, 0x000000FF, 0x000000FF, 0x000000FF,

        0xFCFCFCFF, 0x3CBCFCFF, 0x5C94FCFF, 0xCC88FCFF,
        0xF478FCFF, 0xFC74B4FF, 0xFC7460FF, 0xFC9838FF,
        0xF0BC3CFF, 0x80D010FF, 0x4CDC48FF, 0x58F898FF,
        0x00E8D8FF, 0x787878FF, 0x000000FF, 0x000000FF,

        0xFCFCFCFF, 0xA8E4FCFF, 0xC4D4FCFF, 0xD4C8FCFF,
        0xFCC4FCFF, 0xFCC4D8FF, 0xFCBCB0FF, 0xFCD8A8FF,
        0xFCE4A0FF, 0xE0FCA0FF, 0xA8F0BCFF, 0xB0FCCCFF,
        0x9CFCF0FF, 0xC4C4C4FF, 0x000000FF, 0x000000FF
    };

    void warn(const char* msg)
    {
        fputs(msg, stderr);
        putc('\n', stderr);
    }

    [[noreturn]] void error(const char* msg)
    {
        warn(msg);
        exit(1);
    }

    void print_window(SDL_Window* win)
    {
        int w, h;
        SDL_GetWindowSize(win, &w, &h);
        uint32_t fmt = SDL_GetWindowPixelFormat(win);
        printf("size   : %d x %d\n", w, h);
        printf("format : 0x%08X\n", fmt);
        printf("  PixelType     : %u\n", SDL_PIXELTYPE(fmt));
        printf("  PixelOrder    : %u\n", SDL_PIXELORDER(fmt));
        printf("  PixelLayout   : %u\n", SDL_PIXELLAYOUT(fmt));
        printf("  BitsPerPixel  : %u\n", SDL_BITSPERPIXEL(fmt));
        printf("  BytesPerPixel : %u\n", SDL_BYTESPERPIXEL(fmt));
        printf("  Indexed       : %u\n", SDL_ISPIXELFORMAT_INDEXED(fmt));
        printf("  Alpha         : %u\n", SDL_ISPIXELFORMAT_ALPHA(fmt));
        printf("  Fourcc        : %u\n", SDL_ISPIXELFORMAT_FOURCC(fmt));
    }

    void print_renderer(SDL_Renderer* ren)
    {
        SDL_RendererInfo info;
        if(SDL_GetRendererInfo(ren, &info) != 0)
            error("SDL_GetRendererinfo() failed");
        printf("name               : %s\n", info.name);
        printf("flags              : 0x%08X\n", info.flags);
        printf("texture formats    : %u\n", info.num_texture_formats);
        for(unsigned int i = 0; i < info.num_texture_formats; ++i)
            printf("  0x%08X\n", info.texture_formats[i]);
        printf("max texture width  : %d\n", info.max_texture_width);
        printf("max texture height : %d\n", info.max_texture_height);
    }

    void print_texture(SDL_Texture* tex)
    {
        int w, h;
        uint32_t fmt;
        int access;
        if(SDL_QueryTexture(tex, &fmt, &access, &w, &h) != 0)
            error("SDL_QueryTexture() failed");
        printf("size   : %d x %d\n", w, h);
        printf("format : 0x%08X\n", fmt);
        printf("  PixelType     : %u\n", SDL_PIXELTYPE(fmt));
        printf("  PixelOrder    : %u\n", SDL_PIXELORDER(fmt));
        printf("  PixelLayout   : %u\n", SDL_PIXELLAYOUT(fmt));
        printf("  BitsPerPixel  : %u\n", SDL_BITSPERPIXEL(fmt));
        printf("  BytesPerPixel : %u\n", SDL_BYTESPERPIXEL(fmt));
        printf("  Indexed       : %u\n", SDL_ISPIXELFORMAT_INDEXED(fmt));
        printf("  Alpha         : %u\n", SDL_ISPIXELFORMAT_ALPHA(fmt));
        printf("  Fourcc        : %u\n", SDL_ISPIXELFORMAT_FOURCC(fmt));
        printf("access : 0x%08X\n", access);
    }

    void print_audio_spec(const SDL_AudioSpec& spec, bool obtained)
    {
        printf("freq     : %d\n", spec.freq);
        printf("format   : %d\n", spec.format);
        printf("channels : %u\n", spec.channels);
        printf("samples  : %u\n", spec.samples);
        if(obtained){
            printf("silence  : %u\n", spec.silence);
            printf("size     : %u\n", spec.size);
        }
    }

    void input(unsigned int inputs[2])
    {
        SDL_PumpEvents();
        const uint8_t* keys = SDL_GetKeyboardState(nullptr);

        for(const auto& m : INPUT_MAP){
            if(keys[m.scancode])
                inputs[static_cast<int>(m.port)] |= m.button;
        }
    }

    constexpr int FPS = 60;
    boost::lockfree::spsc_queue<
        int16_t,
        boost::lockfree::capacity<16*(AUDIO_FREQ/FPS)> // とりあえず多めで
    > audio_queue;

    void audio_pull(void* /*userdata*/, uint8_t* stream, int len)
    {
        assert(!(len&1));
        //INFO("%d\n", len);

        unsigned int n_sample = len / 2;

        int16_t* p = reinterpret_cast<int16_t*>(stream);
        size_t n_pop = audio_queue.pop(p, n_sample);
        if(n_pop < n_sample){
            INFO("UNDERFLOW: %lu samples\n", n_sample - n_pop);
            fill_n(p+n_pop, 0, n_sample-n_pop);
        }
    }

    void audio_push(const JunknesSound& sound)
    {
        assert(allequal(sound.sq1.len,
                        sound.sq2.len,
                        sound.tri.len,
                        sound.noi.len,
                        sound.dmc.len));
        int len = sound.sq1.len;

        // CPU周波数とサンプリング周波数が異なるので、1F内では全ての
        // データを処理できず余りが出る。offset 変数でそのズレを管理
        static int offset = 0;

        //constexpr int16_t MUL = 1000;
        constexpr double CPU_FREQ = 6.0 * 39375000.0/11.0 / 12.0;
        constexpr double STEP = CPU_FREQ / AUDIO_FREQ;

        int n_sample = static_cast<int>((len-offset) / STEP);
        //INFO("%d\n", n_sample);
        // TODO: 1ステップ未満の場合の offset 補正
        if(n_sample <= 0) return;

        double pos = offset;
        int n_written = 0;
        while(n_written < n_sample){
            int pos_int = static_cast<int>(pos);
            uint8_t v_sq1 = sound.sq1.data[pos_int]; // [0, 15]
            uint8_t v_sq2 = sound.sq2.data[pos_int]; // [0, 15]
            uint8_t v_tri = sound.tri.data[pos_int]; // [0, 15]
            uint8_t v_noi = sound.noi.data[pos_int]; // [0, 15]
            uint8_t v_dmc = sound.dmc.data[pos_int]; // [0,127]

            // http://wiki.nesdev.com/w/index.php/APU_Mixer
            double mixed = 0.00752*(v_sq1+v_sq2) + 0.00851*v_tri + 0.00494*v_noi + 0.00335*v_dmc;
            int16_t sample = 20000 * (mixed-0.5);

            if(audio_queue.push(sample)){
                ++n_written;
                pos += STEP;
            }
            else{
                INFO("OVERFLOW: %lu samples\n", n_sample - n_written);
                break;
            }
        }

        if(n_written == n_sample){
            offset = static_cast<int>(STEP - (len-pos));
        }
        else{
            // キューがオーバーフローしたらデータを捨てるので offset もリセット
            offset = 0;
        }
        //INFO("%d\n", offset);
    }

    void draw(SDL_Texture* tex, const uint8_t* screen)
    {
        uint32_t* p = nullptr;
        int pitch = -1;
        if(SDL_LockTexture(tex, nullptr, reinterpret_cast<void**>(&p), &pitch) < 0)
            error("SDL_LockTexture() failed");
        assert(pitch == sizeof(p[0]) * 256);

        for(int i = 0; i < 256*240; ++i)
            *p++ = PALETTE_RGBA[screen[i]];

        SDL_UnlockTexture(tex);
    }

    constexpr int OP_ARGLEN[0x100] = {
        /*0x00*/ 1,1,0,1, 1,1,1,1, 0,1,0,1, 2,2,2,2,
        /*0x10*/ 1,1,0,1, 1,1,1,1, 0,2,0,2, 2,2,2,2,
        /*0x20*/ 2,1,0,1, 1,1,1,1, 0,1,0,1, 2,2,2,2,
        /*0x30*/ 1,1,0,1, 1,1,1,1, 0,2,0,2, 2,2,2,2,
        /*0x40*/ 0,1,0,1, 1,1,1,1, 0,1,0,1, 2,2,2,2,
        /*0x50*/ 1,1,0,1, 1,1,1,1, 0,2,0,2, 2,2,2,2,
        /*0x60*/ 0,1,0,1, 1,1,1,1, 0,1,0,1, 2,2,2,2,
        /*0x70*/ 1,1,0,1, 1,1,1,1, 0,2,0,2, 2,2,2,2,
        /*0x80*/ 1,1,1,1, 1,1,1,1, 0,1,0,1, 2,2,2,2,
        /*0x90*/ 1,1,0,1, 1,1,1,1, 0,2,0,2, 2,2,2,2,
        /*0xA0*/ 1,1,1,1, 1,1,1,1, 0,1,0,1, 2,2,2,2,
        /*0xB0*/ 1,1,0,1, 1,1,1,1, 0,2,0,2, 2,2,2,2,
        /*0xC0*/ 1,1,1,1, 1,1,1,1, 0,1,0,1, 2,2,2,2,
        /*0xD0*/ 1,1,0,1, 1,1,1,1, 0,2,0,2, 2,2,2,2,
        /*0xE0*/ 1,1,1,1, 1,1,1,1, 0,1,0,1, 2,2,2,2,
        /*0xF0*/ 1,1,0,1, 1,1,1,1, 0,2,0,2, 2,2,2,2
    };

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
        switch(OP_ARGLEN[opcode]){
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

    string status_str(const JunknesCpuState& st)
    {
        char buf[1024];
        snprintf(buf, sizeof(buf), "%c%c..%c%c%c%c",
                 st.P.N  ? 'N' : '.',
                 st.P.V  ? 'V' : '.',
                 st.P.D  ? 'D' : '.',
                 st.P.I  ? 'I' : '.',
                 st.P.Z  ? 'Z' : '.',
                 st.P.C  ? 'C' : '.');
        return buf;
    }

    void trace_one(const JunknesCpuState* state, uint8_t opcode, uint16_t operand, void*)
    {
        INFO("%04X\t%02X%-6s\t%-11s\tA:%02X X:%02X Y:%02X S:%02X P:%s\n",
             state->PC, opcode, operand_str(opcode, operand).c_str(),
             dis_one(state->PC, opcode, operand).c_str(),
             state->A, state->X, state->Y, state->S, status_str(*state).c_str());
    }

    void usage()
    {
        error("Usage: junknes <INES> [dbg]");
    }
}

int main(int argc, char** argv)
{
    if(argc < 2) usage();

    array<uint8_t, 0x8000> prg;
    array<uint8_t, 0x2000> chr;
    JunknesMirroring mirror;
    if(!ines_split(argv[1], prg, chr, mirror)) error("Cannot load iNES ROM");
    bool dbg = argc == 3;

    if(SDL_Init(
        SDL_INIT_VIDEO |
        SDL_INIT_AUDIO |
        SDL_INIT_TIMER) != 0) error("SDL_Init() failed");
    atexit(SDL_Quit);

    {
        const char* drv_video = SDL_GetCurrentVideoDriver();
        const char* drv_audio = SDL_GetCurrentAudioDriver();
        printf("Video driver: %s\n", drv_video ? drv_video : "(null!?)");
        printf("Audio driver: %s\n", drv_audio ? drv_audio : "(null!?)");
        puts("");
    }


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    SDL_AudioSpec want = {};
#pragma GCC diagnostic pop
    want.freq     = AUDIO_FREQ;
    want.format   = AUDIO_FORMAT;
    want.channels = AUDIO_CHANNELS;
    want.samples  = AUDIO_SAMPLES;
    want.callback = audio_pull;
    puts("[Desired audio spec]");
    print_audio_spec(want, false);
    puts("");

    SDL_AudioSpec have;
    SDL_AudioDeviceID audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if(!audio) error("SDL_OpenAudioDevice() failed");

    if(want.freq     != have.freq ||
       want.format   != have.format ||
       want.channels != have.channels) error("spec changed");

    puts("[Obtained audio spec]");
    print_audio_spec(have, true);
    puts("");


    SDL_Window* win = SDL_CreateWindow(
        "junknes",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        512, 480,
        0);
    if(!win) error("SDL_CreateWindow() failed");
    puts("[Window]");
    print_window(win);
    puts("");

    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, 0);
    if(!ren) error("SDL_CreateRenderer() failed");
    puts("[Renderer]");
    print_renderer(ren);
    puts("");

    SDL_Texture* tex = SDL_CreateTexture(
        ren,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        256, 240);
    if(!tex) error("SDL_CreateTexture() failed");
    puts("[Texture]");
    print_texture(tex);
    puts("");


    Junknes* nes = junknes_create(prg.data(), chr.data(), mirror);
    if(!nes) error("junknes_create() failed");
    if(dbg) junknes_before_exec(nes, trace_one, nullptr);

    SDL_PauseAudioDevice(audio, 0);

    uint32_t start_ms = SDL_GetTicks();
    int frame = 0;
    bool running = true;
    while(running){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(ev.type == SDL_QUIT ||
               (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE))
                running = false;
        }

        unsigned int inputs[2] = {};
        input(inputs);
        junknes_set_input(nes, 0, inputs[0]);
        junknes_set_input(nes, 1, inputs[1]);

        junknes_emulate_frame(nes);

        JunknesSound sound;
        junknes_sound(nes, &sound);
        audio_push(sound);

        draw(tex, junknes_screen(nes));

        //SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);
        SDL_RenderPresent(ren);

        // TODO: まともなウェイト処理
        SDL_Delay(7);

        ++frame;
        if(frame == 1000){
            uint32_t end_ms = SDL_GetTicks();
            printf("fps: %.2f\n", 1000.0 * frame / (end_ms-start_ms));
            start_ms = end_ms;
            frame = 0;
        }
    }

    junknes_destroy(nes);

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);

    SDL_CloseAudioDevice(audio);

    return 0;
}
