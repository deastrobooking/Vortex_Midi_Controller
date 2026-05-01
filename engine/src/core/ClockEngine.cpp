#include "ClockEngine.h"
#include <algorithm>
#include <cmath>
#include <numeric>

ClockEngine::ClockEngine() = default;

ClockEngine::~ClockEngine() {
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
}

void ClockEngine::setTempo(double bpm) {
    m_bpm = std::clamp(bpm, 20.0, 300.0);
}

double ClockEngine::getTempo() const { return m_bpm; }

void ClockEngine::setSource(ClockSource src) { m_source = src; }
ClockSource ClockEngine::getSource() const   { return m_source; }

void ClockEngine::play() {
    if (m_state == TransportState::Playing) return;
    m_tick  = 0;
    m_state = TransportState::Playing;

    if (!m_running) {
        m_running = true;
        m_thread  = std::thread(&ClockEngine::runInternal, this);
    }
}

void ClockEngine::stop() {
    m_state = TransportState::Stopped;
    m_tick  = 0;
}

void ClockEngine::continueClock() {
    m_state = TransportState::Playing;
}

void ClockEngine::tapTempo() {
    auto now = std::chrono::steady_clock::now();

    if (m_tapCount > 0) {
        double ms = std::chrono::duration<double, std::milli>(
                        now - m_lastTap).count();

        // Ignore taps more than 2 seconds apart (restart counting).
        if (ms < 2000.0) {
            double bpm = 60000.0 / ms;
            // Smooth with previous tempo.
            setTempo((getTempo() + bpm) / 2.0);
        } else {
            m_tapCount = 0;
        }
    }

    m_lastTap = now;
    ++m_tapCount;
}

uint64_t    ClockEngine::currentTick()  const { return m_tick; }
uint32_t    ClockEngine::currentBeat()  const { return static_cast<uint32_t>(m_tick / kTicksPerBeat); }
uint32_t    ClockEngine::currentBar()   const { return currentBeat() / 4; }
TransportState ClockEngine::transportState() const { return m_state; }

void ClockEngine::onTick(TickCallback cb) { m_onTick = std::move(cb); }
void ClockEngine::onBeat(BeatCallback cb) { m_onBeat = std::move(cb); }
void ClockEngine::onBar(BarCallback  cb)  { m_onBar  = std::move(cb); }

// ─── External MIDI clock ─────────────────────────────────────────────────────

void ClockEngine::receiveMidiClock() {
    if (m_source != ClockSource::ExternalMidi) return;

    auto now = std::chrono::steady_clock::now();
    double intervalMs = std::chrono::duration<double, std::milli>(
                            now - m_lastExternalClock).count();
    m_lastExternalClock = now;

    // MIDI clock sends 24 pulses per quarter note.
    // Average the last N samples for stability.
    m_externalIntervals[m_externalClockIdx++ % kExternalClockSamples] = intervalMs;
    double avg = std::accumulate(m_externalIntervals,
                                 m_externalIntervals + kExternalClockSamples,
                                 0.0) / kExternalClockSamples;
    if (avg > 0) setTempo(60000.0 / (avg * 24.0));
}

void ClockEngine::receiveMidiStart()    { m_tick = 0; continueClock(); }
void ClockEngine::receiveMidiStop()     { stop(); }
void ClockEngine::receiveMidiContinue() { continueClock(); }

// ─── Quantize ────────────────────────────────────────────────────────────────

uint64_t ClockEngine::quantizeTick(uint64_t tick, uint32_t gridTicks) const {
    if (gridTicks == 0) return tick;
    return ((tick + gridTicks - 1) / gridTicks) * gridTicks;
}

// ─── Internal clock thread ───────────────────────────────────────────────────

void ClockEngine::runInternal() {
    uint32_t prevBeat = 0;
    uint32_t prevBar  = 0;

    while (m_running) {
        if (m_state != TransportState::Playing ||
            m_source != ClockSource::Internal) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // Duration of one tick in microseconds: 60_000_000 / (bpm * ppqn)
        double tickUs = 60'000'000.0 / (m_bpm * kTicksPerBeat);
        auto   wake   = std::chrono::steady_clock::now() +
                        std::chrono::duration<double, std::micro>(tickUs);

        uint64_t tick = ++m_tick;

        if (m_onTick) m_onTick(tick);

        uint32_t beat = static_cast<uint32_t>(tick / kTicksPerBeat);
        if (beat != prevBeat) {
            prevBeat = beat;
            if (m_onBeat) m_onBeat(beat);
        }

        uint32_t bar = beat / 4;
        if (bar != prevBar) {
            prevBar = bar;
            if (m_onBar) m_onBar(bar);
        }

        std::this_thread::sleep_until(wake);
    }
}
