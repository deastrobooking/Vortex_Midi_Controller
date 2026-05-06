#pragma once
#include "Types.h"
#include <array>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

class LfoEngine;
class SequencerEngine;
class MidiRouter;

// Modulation routing matrix.
// Sources: LFOs, MIDI CC, velocity, aftertouch, mod wheel, pitch bend.
// Destinations: MIDI CC (additive), sequencer pitch/velocity/gate/rate,
//               LFO rate/depth, snapshot morph.
//
// process() is called once per clock tick; all ModRoute entries are evaluated
// and their results dispatched to the appropriate subsystem.
class ModMatrix {
public:
    ModMatrix(LfoEngine& lfos, SequencerEngine& seq, MidiRouter& router);

    // ── Route management ──────────────────────────────────────────────────
    void addRoute(ModRoute route);
    void removeRoute(const std::string& routeId);
    void setRouteAmount(const std::string& routeId, float amount);
    void setRouteEnabled(const std::string& routeId, bool enabled);
    void updateRoute(const ModRoute& route);

    std::vector<ModRoute> allRoutes() const;

    // ── Incoming MIDI source feeds ────────────────────────────────────────
    void feedCC(uint8_t cc, uint8_t value);
    void feedVelocity(uint8_t velocity);
    void feedAftertouch(uint8_t pressure);
    void feedModWheel(uint8_t value);
    void feedPitchBend(int16_t value);   // −8192 … +8191

    // ── Snapshot morph callback ────────────────────────────────────────────
    // Called when a SnapshotMorph route produces a morph position [0,1].
    using MorphCallback = std::function<void(const std::string& destId, float position)>;
    void onMorph(MorphCallback cb);

    // ── Clock-driven processing ───────────────────────────────────────────
    // Call once per tick. Evaluates all enabled routes and dispatches.
    void process(uint64_t tick);

private:
    float resolveSource(const ModRoute& r) const;

    std::vector<ModRoute>    m_routes;
    std::array<uint8_t, 128> m_ccValues{};
    float m_velocity{0.f};     // −1..+1
    float m_aftertouch{0.f};
    float m_modWheel{0.f};
    float m_pitchBend{0.f};

    LfoEngine&       m_lfos;
    SequencerEngine& m_seq;
    MidiRouter&      m_router;
    MorphCallback    m_morphCb;

    mutable std::mutex m_mtx;
};
