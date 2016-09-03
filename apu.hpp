#pragma once

#include <array>
#include <tuple>
#include <memory>
#include <cstdint>

#include "util.hpp"

class Apu{
public:
    class Door{
    public:
        virtual ~Door();
        virtual std::uint8_t readDmc(std::uint16_t addr) = 0;
        virtual void triggerDmcIrq() = 0;
        virtual void triggerFrameIrq() = 0;
    };

    explicit Apu(const std::shared_ptr<Door>& door);

    void hardReset();
    void softReset();

    void tick(int cycle /* CPU cycle */);

    std::uint8_t read4015();
    void write4000(std::uint8_t value);
    void write4001(std::uint8_t value);
    void write4002(std::uint8_t value);
    void write4003(std::uint8_t value);
    void write4004(std::uint8_t value);
    void write4005(std::uint8_t value);
    void write4006(std::uint8_t value);
    void write4007(std::uint8_t value);
    void write4008(std::uint8_t value);
    void write400A(std::uint8_t value);
    void write400B(std::uint8_t value);
    void write400C(std::uint8_t value);
    void write400E(std::uint8_t value);
    void write400F(std::uint8_t value);
    void write4010(std::uint8_t value);
    void write4011(std::uint8_t value);
    void write4012(std::uint8_t value);
    void write4013(std::uint8_t value);
    void write4015(std::uint8_t value);
    void write4017(std::uint8_t value);

    using SoundSq  = std::tuple<const std::uint8_t*, int>;
    using SoundTri = std::tuple<const std::uint8_t*, int>;
    using SoundNoi = std::tuple<const std::uint8_t*, int>;
    using SoundDmc = std::tuple<const std::uint8_t*, int>;
    void startFrame();
    void endFrame();
    SoundSq  soundSq1() const;
    SoundSq  soundSq2() const;
    SoundTri soundTri() const;
    SoundNoi soundNoi() const;
    SoundDmc soundDmc() const;

private:
    void updateStep();
    void frameQuarter();
    void frameHalf();

    std::shared_ptr<Door> door_;

    class Square{
    public:
        enum class Ch { SQ1, SQ2 };
        Square(Ch which);

        void hardReset();
        void softReset();
        void enable(bool enabled, int timestamp);
        bool isActive() const;
        void frameQuarter();
        void frameHalf();
        void write4000or4004(std::uint8_t value, int timestamp);
        void write4001or4005(std::uint8_t value, int timestamp);
        void write4002or4006(std::uint8_t value, int timestamp);
        void write4003or4007(std::uint8_t value, int timestamp);

        void startFrame();
        void genSound(int timestamp);
        SoundSq sound() const;
    private:
        bool checkFreq();

        const Ch which_;
        bool enabled_;
        uint8_t duty_; // [0,3]
        unsigned int timer_; // CPU cycle

        // $4002-$4003 または $4006-$4007 で設定されるタイマ値
        // sweepによっても変化する
        union TimerReg{
            std::uint16_t raw;
            explicit TimerReg(std::uint16_t value=0) : raw(value) {}
            TimerReg& operator=(const TimerReg& rhs) { raw = rhs.raw; return *this; }
            BitField16<0,8> lo;
            BitField16<8,3> hi;
        };
        TimerReg timerReg_;

        std::uint8_t length_;
        bool lengthHalt_;
        struct Envelope{
            bool start;
            bool loop;
            bool constant;
            std::uint8_t divider;
            std::uint8_t divider_reg;
            std::uint8_t volume; // constant volume
            std::uint8_t decay_level;
        };
        Envelope envelope_;
        struct Sweep{
            bool enabled;
            std::uint8_t divider;
            std::uint8_t divider_reg;
            bool nega;
            std::uint8_t shift;
        };
        Sweep sweep_;
        unsigned int step_; // シーケンサのステップ(下位3bitを見る。[0,7], increment)

        std::array<std::uint8_t, 40000> sound_; // 各要素は [0,15]
        int soundPos_; // CPU cycle
    };
    Square sq1_, sq2_;

    class Triangle{
    public:
        void hardReset();
        void softReset();
        void enable(bool enabled, int timestamp);
        bool isActive() const;
        void frameQuarter();
        void frameHalf();
        void write4008(std::uint8_t value, int timestamp);
        void write400A(std::uint8_t value, int timestamp);
        void write400B(std::uint8_t value, int timestamp);

        // フレームごとに出力を取得するための適当インターフェース
        void startFrame();
        void genSound(int timestamp);
        SoundTri sound() const;
    private:
        bool enabled_;
        unsigned int timer_; // CPU cycle

        // $400A-$400B で設定されるタイマ値
        union TimerReg{
            std::uint16_t raw;
            explicit TimerReg(std::uint16_t value=0) : raw(value) {}
            TimerReg& operator=(const TimerReg& rhs) { raw = rhs.raw; return *this; }
            BitField16<0,8> lo;
            BitField16<8,3> hi;
        };
        TimerReg timerReg_;

        std::uint8_t length_;
        bool lengthHalt_; // $4008 bit7
        std::uint8_t linear_;
        std::uint8_t linearReg_; // $4008 bit6-0
        bool linearReload_;
        bool control_; // $4008 bit7
        unsigned int step_; // シーケンサのステップ(下位5bitを見る。[0,31], increment)

        // やっつけ音声出力用バッファ
        // CPUサイクルごとに生の出力データを記録するだけ。1FはCPUサイ
        // クルに換算すると30000弱だから、40000あればDMAなどで多少ずれ
        // ても絶対足りるはず(数値自体はFCEUXのパクリ)。
        std::array<std::uint8_t, 40000> sound_; // 各要素は [0,15]
        int soundPos_; // CPU cycle
    };
    Triangle tri_;

    class Noise{
    public:
        void hardReset();
        void softReset();
        void enable(bool enabled, int timestamp);
        bool isActive() const;
        void frameQuarter();
        void frameHalf();
        void write400C(std::uint8_t value, int timestamp);
        void write400E(std::uint8_t value, int timestamp);
        void write400F(std::uint8_t value, int timestamp);

        void startFrame();
        void genSound(int timestamp);
        SoundNoi sound() const;
    private:
        bool enabled_;
        unsigned int timer_;
        unsigned int timerReg_; // $400E bit3-0
        std::uint8_t length_;
        bool lengthHalt_;
        struct Envelope{
            bool start;
            bool loop;
            bool constant;
            std::uint8_t divider;
            std::uint8_t divider_reg;
            std::uint8_t volume; // constant volume
            std::uint8_t decay_level;
        };
        Envelope envelope_;

        struct Lfsr{
            bool mode_short;
            unsigned int reg;
            void shift();
        };
        Lfsr lfsr_;

        std::array<std::uint8_t, 40000> sound_; // 各要素は [0,15]
        int soundPos_; // CPU cycle
    };
    Noise noi_;

    class Dmc{
    public:
        explicit Dmc(const std::shared_ptr<Door>& door);
        void hardReset();
        void softReset();
        void enable(bool enabled, int timestamp);
        bool isActive() const;
        bool irqEnabled() const;
        void tick(int cycle, int sound_timestamp);
        void write4010(std::uint8_t value, int timestamp);
        void write4011(std::uint8_t value, int timestamp);
        void write4012(std::uint8_t value, int timestamp);
        void write4013(std::uint8_t value, int timestamp);

        void startFrame();
        void genSound(int timestamp);
        SoundDmc sound() const;
    private:
        const std::shared_ptr<Door>& door_;

        bool irq_;
        bool loop_;
        unsigned int periodReg_; // $4010 bit3-0
        struct Reader{
            std::uint8_t addr_reg; // $4012
            std::uint8_t size_reg; // $4013
            unsigned int addr; // [0x8000,0xFFFF]
            unsigned int size; // [0,0xFF1]
            std::uint8_t sample;
            bool has_sample;
        } reader_;
        struct Output{
            std::uint8_t level;
            std::uint8_t reg; // シフトレジスタ
            unsigned int rest_bits; // 残りbit数 [0,8]
        } out_;

        // CPUとの同期管理用。単位はCPUサイクル
        // 0:CPUと同期(何もしない)
        // 正:CPUより進んでいる(追いつかれるまで何もしない)
        // 負:CPUより遅れている(追いつくまでDMCを回す)
        int timestamp_;

        std::array<std::uint8_t, 40000> sound_; // 各要素は [0,127]
        int soundPos_; // CPU cycle
    };
    Dmc dmc_;

    /**
     * フレームカウンタ
     * step間のサイクル数は一定と近似している(FCEUXと同じ)
     * サイクル数をCPUサイクルの48倍にしてるのはFCEUXに合わせたものだ
     * が、理由はまだわかってない
     */
    static constexpr int STEP_CYCLE = 48 * 29830 / 4; // 1ステップ当たりのCPUサイクル(48倍)
    int nextStep_;    // [0,3] - 0:step0(Frame IRQ発生), 3:step3(5-stepの場合倍の長さ)
    int restCycle_;   // 次ステップまでのCPUサイクル(48倍)
    bool frameIrqOn_;
    bool step5_;

    union Status{
        std::uint8_t raw;
        explicit Status(std::uint8_t value=0) : raw(value) {}
        Status& operator=(const Status& rhs) { raw = rhs.raw; return *this; }
        BitField8<0> sq1;
        BitField8<1> sq2;
        BitField8<2> tri;
        BitField8<3> noi;
        BitField8<4> dmc;
        BitField8<6> frame_irq;
        BitField8<7> dmc_irq;
    };

    // CPUとの同期管理用
    // 1F内でのCPUサイクル
    int soundTimestamp_;
};
