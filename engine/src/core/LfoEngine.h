#pragma once
#include "Types.h"
#include "LfoShape.h"
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

// Manages multiple LFO instances, advanced once per clock tick.
// Thread-safe: advance() is called from the clock thread;
// getValue() may be called from any thread (e.g. WebSocket for preview).
class LfoEngine {
public:
    // ── Config management ─────────────────────────────────────────────────
    void addLfo(LfoConfig config);
    void removeLfo(const std::string& lfoId);
    void updateConfig(const std::string& lfoId, const LfoConfig& config);
    LfoConfig getConfig(const std::string& lfoId) const;
    std::vector<LfoConfig> allConfigs() const;

    // Apply external modulation to rate or depth (from ModMatrix).
    void setRateMod(const std::string& lfoId, float multiplier);  // 0.25–4×
    void setDepthMod(const std::string& lfoId, float scale);       // 0–1

    // ── Clock-driven advance ──────────────────────────────────────────────
    // Called once per tick from the clock thread.
    void advance(uint64_t tick);

    // Current bipolar output [−1, 1] (or [0, 1] if unipolar).
    float getValue(const std::string& lfoId) const;

    // Reset all LFO phases (e.g. on transport start).
    void reset();

private:
    struct LfoState {
        LfoConfig config;
        float     currentValue{0.f};
        float     prevRandom{0.f};
        float     nextRandom{0.f};
        uint64_t  lastCycle{0};       // cycle index used to detect boundary
        uint32_t  seed{0};            // per-LFO random seed
        float     rateMod{1.f};
        float     depthMod{1.f};
    };

    LfoState* findState(const std::string& lfoId);
    const LfoState* findState(const std::string& lfoId) const;

    std::vector<LfoState>  m_lfos;
    mutable std::shared_mutex m_mtx;
};
