#include "ScaleEngine.h"
#include <algorithm>
#include <cassert>

// ─── Degree tables (12 semitones, true = in scale) ───────────────────────────

std::array<bool, 12> ScaleEngine::degreesForMode(ScaleMode mode) {
    std::array<bool, 12> d{};
    switch (mode) {
        case ScaleMode::Major:
            // W W H W W W H  →  C D E F G A B
            d = {1,0,1,0,1,1,0,1,0,1,0,1}; break;
        case ScaleMode::NaturalMinor:
            // W H W W H W W  →  C D Eb F G Ab Bb
            d = {1,0,1,1,0,1,0,1,1,0,1,0}; break;
        case ScaleMode::Dorian:
            d = {1,0,1,1,0,1,0,1,0,1,1,0}; break;
        case ScaleMode::Phrygian:
            d = {1,1,0,1,0,1,0,1,1,0,1,0}; break;
        case ScaleMode::Lydian:
            d = {1,0,1,0,1,0,1,1,0,1,0,1}; break;
        case ScaleMode::Mixolydian:
            d = {1,0,1,0,1,1,0,1,0,1,1,0}; break;
        case ScaleMode::Locrian:
            d = {1,1,0,1,0,1,1,0,1,0,1,0}; break;
        case ScaleMode::HarmonicMinor:
            d = {1,0,1,1,0,1,0,1,1,0,0,1}; break;
        case ScaleMode::MelodicMinor:
            // ascending form
            d = {1,0,1,1,0,1,0,1,0,1,0,1}; break;
        case ScaleMode::PentatonicMajor:
            d = {1,0,1,0,1,0,0,1,0,1,0,0}; break;
        case ScaleMode::PentatonicMinor:
            d = {1,0,0,1,0,1,0,1,0,0,1,0}; break;
        case ScaleMode::Blues:
            // minor pentatonic + b5
            d = {1,0,0,1,0,1,1,1,0,0,1,0}; break;
        case ScaleMode::WholeTone:
            d = {1,0,1,0,1,0,1,0,1,0,1,0}; break;
        case ScaleMode::Diminished:
            // half-whole alternating
            d = {1,1,0,1,1,0,1,1,0,1,1,0}; break;
        case ScaleMode::Augmented:
            d = {1,0,0,1,1,0,0,1,1,0,0,1}; break;
        case ScaleMode::Chromatic:
            d.fill(true); break;
        default:
            d.fill(false); break;
    }
    return d;
}

Scale ScaleEngine::makeScale(ScaleMode mode, int root) {
    static const char* kNames[] = {
        "Major","Natural Minor","Dorian","Phrygian","Lydian","Mixolydian",
        "Locrian","Harmonic Minor","Melodic Minor","Pentatonic Major",
        "Pentatonic Minor","Blues","Whole Tone","Diminished","Augmented",
        "Chromatic","Custom"
    };
    static const char* kRoots[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

    Scale s;
    s.mode   = mode;
    s.root   = root & 11;
    auto deg = degreesForMode(mode);

    // Rotate degrees by root.
    for (int i = 0; i < 12; ++i)
        s.degrees[(i + s.root) % 12] = deg[i];

    int idx  = static_cast<int>(mode);
    s.name   = std::string(kRoots[s.root]) + " " + kNames[idx];
    s.scaleId = std::string(kRoots[s.root]) + "_" + std::to_string(idx);
    return s;
}

std::vector<Scale> ScaleEngine::allBuiltin() {
    std::vector<Scale> out;
    for (int m = 0; m < static_cast<int>(ScaleMode::Custom); ++m)
        out.push_back(makeScale(static_cast<ScaleMode>(m), 0));
    return out;
}

// ─── Pitch operations ────────────────────────────────────────────────────────

bool ScaleEngine::inScale(int note, const Scale& scale) {
    if (note < 0 || note > 127) return false;
    return scale.degrees[note % 12];
}

int ScaleEngine::quantize(int note, const Scale& scale) {
    note = std::clamp(note, 0, 127);
    if (scale.degrees[note % 12]) return note;

    // Search outward: try +1, -1, +2, -2, …
    for (int d = 1; d <= 12; ++d) {
        int up   = note + d;
        int down = note - d;
        if (up <= 127 && scale.degrees[up % 12])   return up;
        if (down >= 0 && scale.degrees[down % 12]) return down;
    }
    return note;  // chromatic fallback (shouldn't reach here for non-empty scale)
}

int ScaleEngine::transposeDegrees(int note, int degrees, const Scale& scale) {
    note = std::clamp(note, 0, 127);
    if (degrees == 0) return note;

    int step = (degrees > 0) ? 1 : -1;
    int remaining = (degrees > 0) ? degrees : -degrees;

    int cur = note;
    while (remaining > 0) {
        cur += step;
        cur = std::clamp(cur, 0, 127);
        if (scale.degrees[cur % 12]) --remaining;
        if (cur == 0 || cur == 127) break;
    }
    return cur;
}

// ─── Instance ────────────────────────────────────────────────────────────────

void ScaleEngine::addScale(Scale s) {
    for (auto& existing : m_custom) {
        if (existing.scaleId == s.scaleId) { existing = std::move(s); return; }
    }
    m_custom.push_back(std::move(s));
}

void ScaleEngine::removeScale(const std::string& scaleId) {
    m_custom.erase(std::remove_if(m_custom.begin(), m_custom.end(),
        [&](const Scale& s){ return s.scaleId == scaleId; }), m_custom.end());
}

const Scale* ScaleEngine::find(const std::string& scaleId) const {
    for (const auto& s : m_custom)
        if (s.scaleId == scaleId) return &s;

    // Also check built-ins by scaleId.
    static auto builtins = allBuiltin();
    for (const auto& s : builtins)
        if (s.scaleId == scaleId) return &s;
    return nullptr;
}

std::vector<Scale> ScaleEngine::allScales() const {
    auto out = allBuiltin();
    out.insert(out.end(), m_custom.begin(), m_custom.end());
    return out;
}
