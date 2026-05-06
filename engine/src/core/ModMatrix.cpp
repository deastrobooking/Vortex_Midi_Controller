#include "ModMatrix.h"
#include "LfoEngine.h"
#include "SequencerEngine.h"
#include "MidiRouter.h"
#include <algorithm>

ModMatrix::ModMatrix(LfoEngine& lfos, SequencerEngine& seq, MidiRouter& router)
    : m_lfos(lfos), m_seq(seq), m_router(router)
{}

// ─── Route management ─────────────────────────────────────────────────────────

void ModMatrix::addRoute(ModRoute route) {
    std::lock_guard lock(m_mtx);
    for (auto& r : m_routes) {
        if (r.routeId == route.routeId) { r = std::move(route); return; }
    }
    m_routes.push_back(std::move(route));
}

void ModMatrix::removeRoute(const std::string& routeId) {
    std::lock_guard lock(m_mtx);
    m_routes.erase(std::remove_if(m_routes.begin(), m_routes.end(),
        [&](const ModRoute& r){ return r.routeId == routeId; }),
        m_routes.end());
}

void ModMatrix::setRouteAmount(const std::string& routeId, float amount) {
    std::lock_guard lock(m_mtx);
    for (auto& r : m_routes)
        if (r.routeId == routeId) { r.amount = std::clamp(amount,-1.f,1.f); return; }
}

void ModMatrix::setRouteEnabled(const std::string& routeId, bool enabled) {
    std::lock_guard lock(m_mtx);
    for (auto& r : m_routes)
        if (r.routeId == routeId) { r.enabled = enabled; return; }
}

void ModMatrix::updateRoute(const ModRoute& route) {
    std::lock_guard lock(m_mtx);
    for (auto& r : m_routes)
        if (r.routeId == route.routeId) { r = route; return; }
    m_routes.push_back(route);
}

std::vector<ModRoute> ModMatrix::allRoutes() const {
    std::lock_guard lock(m_mtx);
    return m_routes;
}

// ─── Incoming MIDI source feeds ───────────────────────────────────────────────

void ModMatrix::feedCC(uint8_t cc, uint8_t value) {
    m_ccValues[cc & 0x7F] = value;
}

void ModMatrix::feedVelocity(uint8_t velocity) {
    m_velocity = (velocity / 63.5f) - 1.f;   // 0–127 → −1..+1
}

void ModMatrix::feedAftertouch(uint8_t pressure) {
    m_aftertouch = pressure / 127.f;
}

void ModMatrix::feedModWheel(uint8_t value) {
    m_modWheel = value / 127.f;
}

void ModMatrix::feedPitchBend(int16_t value) {
    m_pitchBend = std::clamp(value / 8191.f, -1.f, 1.f);
}

void ModMatrix::onMorph(MorphCallback cb) {
    m_morphCb = std::move(cb);
}

// ─── Source resolution ────────────────────────────────────────────────────────

float ModMatrix::resolveSource(const ModRoute& r) const {
    switch (r.source) {
        case ModSource::Lfo:
            return m_lfos.getValue(r.sourceId);
        case ModSource::Velocity:
            return m_velocity;
        case ModSource::Aftertouch:
            return m_aftertouch;
        case ModSource::ModWheel:
            return m_modWheel;
        case ModSource::PitchBend:
            return m_pitchBend;
        case ModSource::MidiCC: {
            // sourceId = "cc<N>"
            int cc = 0;
            try { cc = std::stoi(r.sourceId.substr(2)); } catch (...) {}
            return m_ccValues[cc & 0x7F] / 127.f;
        }
        case ModSource::SeqCvLane:
            // Not yet implemented — placeholder returning 0.
            return 0.f;
        default:
            return 0.f;
    }
}

// ─── Clock-driven processing ──────────────────────────────────────────────────

void ModMatrix::process(uint64_t /*tick*/) {
    std::lock_guard lock(m_mtx);

    // Accumulate additive CC mods before sending: multiple routes may target
    // the same control, and we want a single MIDI message per control per tick.
    std::unordered_map<std::string, float> ccAccum;

    for (const auto& r : m_routes) {
        if (!r.enabled) continue;

        float raw = resolveSource(r);
        float mod = std::clamp(raw * r.amount + r.offset, -1.f, 1.f);

        switch (r.dest) {
        case ModDest::MidiCC:
            ccAccum[r.destId] += mod;
            break;

        case ModDest::SeqPitch: {
            int trackId = 0;
            try { trackId = std::stoi(r.destId); } catch (...) {}
            m_seq.applyPitchMod(static_cast<uint8_t>(trackId), mod * 24.f);
            break;
        }
        case ModDest::SeqVelocity: {
            int trackId = 0;
            try { trackId = std::stoi(r.destId); } catch (...) {}
            m_seq.applyVelocityMod(static_cast<uint8_t>(trackId), mod);
            break;
        }
        case ModDest::SeqGate: {
            int trackId = 0;
            try { trackId = std::stoi(r.destId); } catch (...) {}
            m_seq.applyGateMod(static_cast<uint8_t>(trackId), (mod + 1.f) * 1.f);
            break;
        }
        case ModDest::SeqRate: {
            int trackId = 0;
            try { trackId = std::stoi(r.destId); } catch (...) {}
            // mod in [−1,1] → rate multiplier in [0.25, 4]
            float mult = (mod + 1.f) * 1.875f + 0.25f;
            m_seq.applyRateMod(static_cast<uint8_t>(trackId),
                               std::clamp(mult, 0.25f, 4.f));
            break;
        }
        case ModDest::LfoRate:
            m_lfos.setRateMod(r.destId, (mod + 1.f) * 1.875f + 0.25f);
            break;

        case ModDest::LfoDepth:
            m_lfos.setDepthMod(r.destId, (mod + 1.f) * 0.5f);
            break;

        case ModDest::SnapshotMorph:
            if (m_morphCb) m_morphCb(r.destId, (mod + 1.f) * 0.5f);
            break;

        default:
            break;
        }
    }

    // Dispatch accumulated CC modulations through MidiRouter.
    for (const auto& [controlId, normMod] : ccAccum) {
        float clamped = std::clamp(normMod, -1.f, 1.f);
        // Convert to a 0–127 additive offset centered at 64.
        int offset = static_cast<int>(clamped * 63.f);
        // Route as a ControlEvent; MidiRouter will apply any scaling/curve.
        ControlEvent evt;
        evt.controlId      = controlId;
        evt.value          = std::clamp(64 + offset, 0, 127);
        evt.timestampTicks = 0;
        m_router.route(evt);
    }
}
