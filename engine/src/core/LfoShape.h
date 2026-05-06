#pragma once
#include "Types.h"
#include <cmath>
#include <vector>
#include <algorithm>

// Header-only: pure shape evaluation, no state.
// Used by both LfoEngine (audio thread) and the UI preview endpoint.
namespace LfoShape {

// ─── Internal helpers ────────────────────────────────────────────────────────

namespace detail {

// Simple deterministic value in [−1, 1] from a cycle index + per-LFO seed.
inline float pseudoRandom(uint32_t cycle, uint32_t seed) {
    uint32_t h = (cycle + 1u) * 2654435761u ^ seed * 2246822519u;
    h ^= h >> 13u; h *= 1540483477u; h ^= h >> 15u;
    return static_cast<float>(h & 0xFFFFu) / 32767.5f - 1.f;
}

// Rhythmic gate slot: fast attack → hold → fast decay.
// sub [0,1] is phase within one slot. Returns bipolar value.
inline float rhythmicSlot(float sub, bool active) {
    if (!active) return -0.8f;
    static constexpr float kAttack  = 0.07f;
    static constexpr float kDecay   = 0.12f;
    if (sub < kAttack)          return -0.8f + (1.8f / kAttack) * sub;
    if (sub > (1.f - kDecay))   return 1.f   - (1.8f / kDecay) * (sub - (1.f - kDecay));
    return 1.f;
}

// Custom-points piecewise-linear interpolation.
inline float evalCustom(float p, const std::vector<LfoPoint>& pts) {
    if (pts.empty()) return 0.f;
    if (pts.size() == 1) return pts[0].value;

    // Find surrounding points (assume sorted by phase).
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        if (p <= pts[i + 1].phase) {
            float span = pts[i + 1].phase - pts[i].phase;
            if (span < 1e-6f) return pts[i].value;
            float t = (p - pts[i].phase) / span;
            return pts[i].value + t * (pts[i + 1].value - pts[i].value);
        }
    }
    return pts.back().value;
}

} // namespace detail

// ─── Shape evaluation: phase [0,1) → bipolar [−1, 1] ─────────────────────────
//
// prevRand / nextRand: random values for the current and the next cycle.
// Generate via pseudoRandom(cycleIndex, seed) in LfoEngine.
// Pass 0.f for all non-random shapes.

inline float evaluate(const LfoConfig& cfg, float phase,
                      float prevRand = 0.f, float nextRand = 0.f)
{
    const float p = std::clamp(phase, 0.f, 1.f);

    float v = 0.f;
    switch (cfg.shape) {
    // ── Standard waveforms ────────────────────────────────────────────────
    case LfoShapeType::Sine:
        v = sinf(2.f * static_cast<float>(M_PI) * p);
        break;

    case LfoShapeType::Triangle:
        v = (p < 0.5f) ? (4.f * p - 1.f) : (3.f - 4.f * p);
        break;

    case LfoShapeType::Square50:
        v = (p < 0.5f) ? 1.f : -1.f;
        break;

    case LfoShapeType::SawUp:
        v = 2.f * p - 1.f;
        break;

    case LfoShapeType::SawDown:
        v = 1.f - 2.f * p;
        break;

    // ── Random shapes ─────────────────────────────────────────────────────
    case LfoShapeType::SampleHold:
        v = prevRand;   // new value sampled each cycle in LfoEngine
        break;

    case LfoShapeType::SmoothRandom:
        v = prevRand + (nextRand - prevRand) * p;  // linear interp across cycle
        break;

    // ── Exponential shapes ────────────────────────────────────────────────
    case LfoShapeType::ExpRise: {
        float e = (expf(p) - 1.f) / (static_cast<float>(M_E) - 1.f);
        v = 2.f * e - 1.f;
        break;
    }
    case LfoShapeType::ExpFall: {
        float e = (expf(1.f - p) - 1.f) / (static_cast<float>(M_E) - 1.f);
        v = 2.f * e - 1.f;
        break;
    }

    // ── Pulse shapes ──────────────────────────────────────────────────────
    case LfoShapeType::Pulse25:
        v = (p < 0.25f) ? 1.f : -1.f;
        break;

    case LfoShapeType::Pulse75:
        v = (p < 0.75f) ? 1.f : -1.f;
        break;

    // ── Staircase shapes ──────────────────────────────────────────────────
    case LfoShapeType::StaircaseUp: {
        int step = static_cast<int>(p * 8.f);
        v = 2.f * (step / 7.f) - 1.f;
        break;
    }
    case LfoShapeType::StaircaseDown: {
        int step = static_cast<int>(p * 8.f);
        v = 1.f - 2.f * (step / 7.f);
        break;
    }

    // ── Special shapes ────────────────────────────────────────────────────
    case LfoShapeType::Bounce:
        // |sin(π p)| maps to [0,1]; map to [−1,1] as 2|sin|−1
        v = 2.f * fabsf(sinf(static_cast<float>(M_PI) * p)) - 1.f;
        break;

    case LfoShapeType::SineSquared: {
        float s = sinf(static_cast<float>(M_PI) * p);
        v = 2.f * (s * s) - 1.f;   // peaks at 0 and 1, trough at 0.5
        break;
    }

    case LfoShapeType::Trapezoid:
        // 10% ramp-up | 40% hold-high | 10% ramp-down | 40% hold-low
        if      (p < 0.10f) v = -1.f + 20.f * p;
        else if (p < 0.50f) v =  1.f;
        else if (p < 0.60f) v =  1.f - 20.f * (p - 0.50f);
        else                v = -1.f;
        break;

    // ── Rhythmic / syncopated shapes ──────────────────────────────────────
    //    Each uses rhythmicSlot() for a natural attack-hold-decay gate feel.

    case LfoShapeType::Syncopated8th: {
        // 8 slots per cycle; HIGH on odd-indexed slots (off-beats 2,4,6,8).
        int   slot = static_cast<int>(p * 8.f) % 8;
        float sub  = p * 8.f - slot;
        v = detail::rhythmicSlot(sub, (slot % 2) == 1);
        break;
    }

    case LfoShapeType::Syncopated16th: {
        // Son clave: 1 0 0 1 0 0 1 0 | 0 0 1 0 1 0 0 0
        static constexpr bool kPat[16] = {
            1,0,0,1,0,0,1,0, 0,0,1,0,1,0,0,0
        };
        int   slot = static_cast<int>(p * 16.f) % 16;
        float sub  = p * 16.f - slot;
        v = detail::rhythmicSlot(sub, kPat[slot]);
        break;
    }

    case LfoShapeType::DottedGroove: {
        // 3+3+2 across 8 sixteenth slots: accents at 0, 3, 6.
        static constexpr bool kPat[8] = {1,0,0,1,0,0,1,0};
        int   slot = static_cast<int>(p * 8.f) % 8;
        float sub  = p * 8.f - slot;
        v = detail::rhythmicSlot(sub, kPat[slot]);
        break;
    }

    case LfoShapeType::Shuffle16th: {
        // 8 eighth-note pairs per cycle; within each pair the first 16th
        // takes 2/3 of the time (HIGH) and the second takes 1/3 (LOW).
        float eighth = p * 8.f;
        float sub    = eighth - floorf(eighth);  // 0..1 within the eighth
        if (sub < (2.f / 3.f)) {
            float sp = sub / (2.f / 3.f);
            v = detail::rhythmicSlot(sp, true);
        } else {
            v = -0.8f;
        }
        break;
    }

    case LfoShapeType::Custom:
        v = detail::evalCustom(p, cfg.customPoints);
        break;

    default:
        v = 0.f;
        break;
    }

    // Apply phase offset, depth, and unipolar conversion.
    if (!cfg.bipolar) v = (v + 1.f) * 0.5f;   // remap to [0, 1]
    return v * cfg.depth;
}

// ─── Preview: N equally-spaced samples across one cycle ─────────────────────

inline std::vector<float> preview(const LfoConfig& cfg, int n = 256,
                                  uint32_t seed = 0)
{
    std::vector<float> out(n);
    float prevRand = detail::pseudoRandom(0u, seed);
    float nextRand = detail::pseudoRandom(1u, seed);
    for (int i = 0; i < n; ++i)
        out[i] = evaluate(cfg, static_cast<float>(i) / n, prevRand, nextRand);
    return out;
}

} // namespace LfoShape
