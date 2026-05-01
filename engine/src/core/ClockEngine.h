#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <thread>

enum class ClockSource { Internal, ExternalMidi, AbletonLink };
enum class TransportState { Stopped, Playing, Paused };

// Callback signatures
using TickCallback  = std::function<void(uint64_t tick)>;
using BeatCallback  = std::function<void(uint32_t beat)>;
using BarCallback   = std::function<void(uint32_t bar)>;

class ClockEngine {
public:
    ClockEngine();
    ~ClockEngine();

    void setTempo(double bpm);
    double getTempo() const;

    void setSource(ClockSource src);
    ClockSource getSource() const;

    void play();
    void stop();
    void continueClock();
    void tapTempo();

    uint64_t    currentTick() const;
    uint32_t    currentBeat() const;
    uint32_t    currentBar()  const;
    TransportState transportState() const;

    // Fire once per tick (960 ppqn).
    void onTick(TickCallback cb);
    // Fire once per quarter-note beat.
    void onBeat(BeatCallback cb);
    // Fire once per 4/4 bar.
    void onBar(BarCallback cb);

    // Called by the MIDI input thread when an external clock tick arrives.
    void receiveMidiClock();
    void receiveMidiStart();
    void receiveMidiStop();
    void receiveMidiContinue();

    // Quantise a future tick to the nearest bar/beat/16th boundary.
    uint64_t quantizeTick(uint64_t tick, uint32_t gridTicks) const;

private:
    void runInternal();

    std::atomic<double>         m_bpm{120.0};
    std::atomic<uint64_t>       m_tick{0};
    std::atomic<TransportState> m_state{TransportState::Stopped};
    std::atomic<ClockSource>    m_source{ClockSource::Internal};

    TickCallback  m_onTick;
    BeatCallback  m_onBeat;
    BarCallback   m_onBar;

    std::thread   m_thread;
    std::atomic<bool> m_running{false};

    // For tap tempo
    std::chrono::steady_clock::time_point m_lastTap{};
    uint8_t m_tapCount{0};

    // For external MIDI clock averaging
    static constexpr int kExternalClockSamples = 24;
    double m_externalIntervals[kExternalClockSamples]{};
    int    m_externalClockIdx{0};
    std::chrono::steady_clock::time_point m_lastExternalClock{};
};
