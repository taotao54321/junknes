#include <string>
#include <array>
#include <tuple>
#include <algorithm>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cassert>

#include <alloca.h>

#include <boost/lockfree/spsc_queue.hpp>

#include <SDL.h>

#include "nes.hpp"
#include "ines.hpp"

using namespace std;

namespace{
    template<typename T>
    bool allequal(const T& x, const T& y) { return x == y; }
    template<typename T, typename... Args>
    bool allequal(const T& x, const T& y, Args... tail) { return x == y && allequal(y, tail...); }

    struct InputMap{
        int scancode;
        Nes::InputPort port;
        unsigned int button;
    };
    constexpr InputMap INPUT_MAP[] = {
        { SDL_SCANCODE_D, Nes::InputPort::PAD_1P, Nes::BUTTON_R },
        { SDL_SCANCODE_A, Nes::InputPort::PAD_1P, Nes::BUTTON_L },
        { SDL_SCANCODE_S, Nes::InputPort::PAD_1P, Nes::BUTTON_D },
        { SDL_SCANCODE_W, Nes::InputPort::PAD_1P, Nes::BUTTON_U },
        { SDL_SCANCODE_E, Nes::InputPort::PAD_1P, Nes::BUTTON_T },
        { SDL_SCANCODE_Q, Nes::InputPort::PAD_1P, Nes::BUTTON_S },
        { SDL_SCANCODE_X, Nes::InputPort::PAD_1P, Nes::BUTTON_B },
        { SDL_SCANCODE_Z, Nes::InputPort::PAD_1P, Nes::BUTTON_A },
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

    void audio_push(
        const Nes::SoundSq&  sound_sq1,
        const Nes::SoundSq&  sound_sq2,
        const Nes::SoundTri& sound_tri,
        const Nes::SoundNoi& sound_noi)
    {
        assert(allequal(
            get<1>(sound_sq1), get<1>(sound_sq2), get<1>(sound_tri), get<1>(sound_noi)));

        // CPU周波数とサンプリング周波数が異なるので、1F内では全ての
        // データを処理できず余りが出る。offset 変数でそのズレを管理
        static int offset = 0;

        //constexpr int16_t MUL = 1000;
        constexpr double CPU_FREQ = 6.0 * 39375000.0/11.0 / 12.0;
        constexpr double STEP = CPU_FREQ / AUDIO_FREQ;

        //INFO("%d\n", get<1>(sound_tri));
        int n_sample = static_cast<int>((get<1>(sound_tri)-offset) / STEP);
        //INFO("%d\n", n_sample);
        // TODO: 1ステップ未満の場合の offset 補正
        if(n_sample <= 0) return;

        double pos = offset;
        int n_written = 0;
        while(n_written < n_sample){
            int pos_int = static_cast<int>(pos);
            // TODO: まともなmixerを書く
            int data = 0;
            data += get<0>(sound_sq1)[pos_int] - 15;
            data += get<0>(sound_sq2)[pos_int] - 15;
            data += get<0>(sound_tri)[pos_int] - 15;
            data += get<0>(sound_noi)[pos_int] - 15;
            //INFO("%d\n", data);
            int16_t sample = 300 * data;
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
            offset = static_cast<int>(STEP - (get<1>(sound_tri)-pos));
        }
        else{
            // キューがオーバーフローしたらデータを捨てるので offset もリセット
            offset = 0;
        }
        //INFO("%d\n", offset);
    }

    void draw(SDL_Texture* tex, const Nes::Screen& screen)
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

    void usage()
    {
        error("Usage: junknes <INES>");
    }
}

int main(int argc, char** argv)
{
    if(argc != 2) usage();

    Nes::Prg prg;
    Nes::Chr chr;
    Nes::NtMirror mirror;
    if(!ines_split(argv[1], prg, chr, mirror)) error("Cannot load iNES ROM");

    Nes nes(prg, chr, mirror);

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
    puts("[Desired spec]");
    print_audio_spec(want, false);
    puts("");

    SDL_AudioSpec have;
    SDL_AudioDeviceID audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if(!audio) error("SDL_OpenAudioDevice() failed");

    if(want.freq     != have.freq ||
       want.format   != have.format ||
       want.channels != have.channels) error("spec changed");

    puts("[Obtained spec]");
    print_audio_spec(have, true);
    puts("");


    SDL_Window* win = SDL_CreateWindow(
        "junknes",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        512, 480,
        0);
    if(!win) error("SDL_CreateWindow() failed");

    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, 0);
    if(!ren) error("SDL_CreateRenderer() failed");

    SDL_Texture* tex = SDL_CreateTexture(
        ren,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        256, 240);
    if(!tex) error("SDL_CreateTexture() failed");


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
        nes.setInput(Nes::InputPort::PAD_1P, inputs[0]);
        nes.setInput(Nes::InputPort::PAD_2P, inputs[1]);

        nes.emulateFrame();

        audio_push(nes.soundSq1(), nes.soundSq2(), nes.soundTri(), nes.soundNoi());

        draw(tex, nes.screen());

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

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);

    SDL_CloseAudioDevice(audio);

    return 0;
}
