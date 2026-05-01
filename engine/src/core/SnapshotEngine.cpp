#include "SnapshotEngine.h"
#include <algorithm>
#include <cmath>

SnapshotEngine::SnapshotEngine() = default;

void SnapshotEngine::registerControl(const std::string& controlId, int value) {
    m_values.emplace(controlId, value);
}

void SnapshotEngine::updateValue(const std::string& controlId, int value) {
    m_values[controlId] = value;
}

SceneValues SnapshotEngine::captureScene(const std::string& sceneId,
                                          const std::string& name) const {
    SceneValues sv;
    sv.sceneId = sceneId;
    sv.name    = name;
    sv.controlIds.reserve(m_values.size());
    sv.values.reserve(m_values.size());

    for (const auto& [id, val] : m_values) {
        sv.controlIds.push_back(id);
        sv.values.push_back(val);
    }

    return sv;
}

void SnapshotEngine::addScene(SceneValues scene) {
    m_scenes[scene.sceneId] = std::move(scene);
}

void SnapshotEngine::removeScene(const std::string& sceneId) {
    m_scenes.erase(sceneId);
}

void SnapshotEngine::onValueChange(SceneMorphCallback cb) {
    m_callback = std::move(cb);
}

const std::unordered_map<std::string, int>& SnapshotEngine::currentValues() const {
    return m_values;
}

std::vector<std::string> SnapshotEngine::sceneIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_scenes.size());
    for (const auto& [id, _] : m_scenes) ids.push_back(id);
    return ids;
}

void SnapshotEngine::recallScene(const std::string& sceneId,
                                  SceneRecallMode mode,
                                  uint32_t durationTicks) {
    auto it = m_scenes.find(sceneId);
    if (it == m_scenes.end()) return;

    const auto& target = it->second;

    switch (mode) {
        case SceneRecallMode::Jump:
            applyJump(target);
            break;
        case SceneRecallMode::Fade:
            startFade(target, durationTicks, 0); // tick will be set in next tick()
            m_morph.mode = SceneRecallMode::Fade;
            break;
        case SceneRecallMode::Drop:
            // Will be triggered at the next bar boundary — store state.
            {
                m_morph.targetValues.clear();
                for (size_t i = 0; i < target.controlIds.size(); ++i)
                    m_morph.targetValues[target.controlIds[i]] = target.values[i];
                m_morph.mode   = SceneRecallMode::Drop;
                m_morph.active = true;
            }
            break;
        case SceneRecallMode::Morph:
            startFade(target, durationTicks, 0);
            m_morph.mode = SceneRecallMode::Morph;
            break;
    }
}

void SnapshotEngine::tick(uint64_t currentTick) {
    if (!m_morph.active) return;

    switch (m_morph.mode) {
        case SceneRecallMode::Drop:
            // Trigger on the next bar boundary.
            if (m_morph.triggerTick == 0) {
                m_morph.triggerTick =
                    ((currentTick / kTicksPerBar4_4) + 1) * kTicksPerBar4_4;
            }
            if (currentTick >= m_morph.triggerTick) {
                for (const auto& [id, val] : m_morph.targetValues) {
                    m_values[id] = val;
                    if (m_callback) m_callback(id, val);
                }
                m_morph        = {};
            }
            break;

        case SceneRecallMode::Fade:
        case SceneRecallMode::Morph: {
            if (m_morph.startTick == 0) m_morph.startTick = currentTick;
            uint64_t elapsed = currentTick - m_morph.startTick;

            if (elapsed >= m_morph.durationTicks) {
                // Snap to target.
                for (const auto& [id, val] : m_morph.targetValues) {
                    m_values[id] = val;
                    if (m_callback) m_callback(id, val);
                }
                m_morph = {};
                return;
            }

            double t = static_cast<double>(elapsed) /
                       static_cast<double>(m_morph.durationTicks);

            for (const auto& [id, target] : m_morph.targetValues) {
                auto startIt = m_morph.startValues.find(id);
                int  start   = (startIt != m_morph.startValues.end())
                               ? startIt->second : 0;
                int  mixed   = static_cast<int>(
                    std::round(start + t * (target - start)));
                m_values[id] = mixed;
                if (m_callback) m_callback(id, mixed);
            }
            break;
        }

        default:
            break;
    }
}

void SnapshotEngine::applyJump(const SceneValues& scene) {
    for (size_t i = 0; i < scene.controlIds.size(); ++i) {
        m_values[scene.controlIds[i]] = scene.values[i];
        if (m_callback) m_callback(scene.controlIds[i], scene.values[i]);
    }
}

void SnapshotEngine::startFade(const SceneValues& target,
                                uint32_t durationTicks,
                                uint64_t startTick) {
    m_morph.startValues  = m_values;
    m_morph.targetValues.clear();
    for (size_t i = 0; i < target.controlIds.size(); ++i)
        m_morph.targetValues[target.controlIds[i]] = target.values[i];

    m_morph.startTick     = startTick;
    m_morph.durationTicks = durationTicks;
    m_morph.active        = true;
    m_morph.triggerTick   = 0;
}
