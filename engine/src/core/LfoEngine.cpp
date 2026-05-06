#include "LfoEngine.h"
#include <algorithm>

// ─── Config management ────────────────────────────────────────────────────────

void LfoEngine::addLfo(LfoConfig config) {
    std::unique_lock lock(m_mtx);
    for (auto& s : m_lfos) {
        if (s.config.lfoId == config.lfoId) {
            s.config = std::move(config);
            return;
        }
    }
    LfoState st;
    st.seed = static_cast<uint32_t>(std::hash<std::string>{}(config.lfoId));
    st.prevRandom = LfoShape::detail::pseudoRandom(0u, st.seed);
    st.nextRandom = LfoShape::detail::pseudoRandom(1u, st.seed);
    st.config = std::move(config);
    m_lfos.push_back(std::move(st));
}

void LfoEngine::removeLfo(const std::string& lfoId) {
    std::unique_lock lock(m_mtx);
    m_lfos.erase(std::remove_if(m_lfos.begin(), m_lfos.end(),
        [&](const LfoState& s){ return s.config.lfoId == lfoId; }),
        m_lfos.end());
}

void LfoEngine::updateConfig(const std::string& lfoId, const LfoConfig& config) {
    std::unique_lock lock(m_mtx);
    if (auto* s = findState(lfoId)) s->config = config;
}

LfoConfig LfoEngine::getConfig(const std::string& lfoId) const {
    std::shared_lock lock(m_mtx);
    if (const auto* s = findState(lfoId)) return s->config;
    return {};
}

std::vector<LfoConfig> LfoEngine::allConfigs() const {
    std::shared_lock lock(m_mtx);
    std::vector<LfoConfig> out;
    out.reserve(m_lfos.size());
    for (const auto& s : m_lfos) out.push_back(s.config);
    return out;
}

void LfoEngine::setRateMod(const std::string& lfoId, float multiplier) {
    std::unique_lock lock(m_mtx);
    if (auto* s = findState(lfoId))
        s->rateMod = std::clamp(multiplier, 0.25f, 4.f);
}

void LfoEngine::setDepthMod(const std::string& lfoId, float scale) {
    std::unique_lock lock(m_mtx);
    if (auto* s = findState(lfoId))
        s->depthMod = std::clamp(scale, 0.f, 1.f);
}

// ─── Clock-driven advance ─────────────────────────────────────────────────────

void LfoEngine::advance(uint64_t tick) {
    std::unique_lock lock(m_mtx);

    for (auto& s : m_lfos) {
        const auto& cfg = s.config;

        uint32_t div = cfg.syncDivision;
        if (div == 0) div = kTicksPerBar4_4;

        // Apply rate modulation: scale the effective division inversely.
        auto effectiveDiv = static_cast<uint32_t>(div / s.rateMod);
        if (effectiveDiv < 1) effectiveDiv = 1;

        uint64_t cycle = tick / effectiveDiv;
        float    phase = static_cast<float>(tick % effectiveDiv) / effectiveDiv;
        phase = std::fmod(phase + cfg.phaseOffset, 1.f);

        // On cycle boundary: roll random values.
        if (cycle != s.lastCycle) {
            s.prevRandom = s.nextRandom;
            s.nextRandom = LfoShape::detail::pseudoRandom(
                static_cast<uint32_t>(cycle + 1u), s.seed);
            s.lastCycle = cycle;
        }

        // Build a temporary config reflecting depth modulation.
        LfoConfig evalCfg   = cfg;
        evalCfg.depth       = cfg.depth * s.depthMod;

        s.currentValue = LfoShape::evaluate(evalCfg, phase,
                                             s.prevRandom, s.nextRandom);
    }
}

float LfoEngine::getValue(const std::string& lfoId) const {
    std::shared_lock lock(m_mtx);
    if (const auto* s = findState(lfoId)) return s->currentValue;
    return 0.f;
}

void LfoEngine::reset() {
    std::unique_lock lock(m_mtx);
    for (auto& s : m_lfos) {
        s.lastCycle   = 0;
        s.currentValue = 0.f;
        s.prevRandom  = LfoShape::detail::pseudoRandom(0u, s.seed);
        s.nextRandom  = LfoShape::detail::pseudoRandom(1u, s.seed);
    }
}

// ─── Internal helpers ─────────────────────────────────────────────────────────

LfoEngine::LfoState* LfoEngine::findState(const std::string& lfoId) {
    for (auto& s : m_lfos)
        if (s.config.lfoId == lfoId) return &s;
    return nullptr;
}

const LfoEngine::LfoState* LfoEngine::findState(const std::string& lfoId) const {
    for (const auto& s : m_lfos)
        if (s.config.lfoId == lfoId) return &s;
    return nullptr;
}
