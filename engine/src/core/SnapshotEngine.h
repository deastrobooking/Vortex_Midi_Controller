#pragma once
#include "Types.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

using SceneMorphCallback = std::function<void(const std::string& controlId, int value)>;

class SnapshotEngine {
public:
    SnapshotEngine();

    // Register a control with its current value.
    void registerControl(const std::string& controlId, int value);

    // Update live value (called on every control event).
    void updateValue(const std::string& controlId, int value);

    // Capture the current state as a new scene.
    SceneValues captureScene(const std::string& sceneId,
                             const std::string& name) const;

    // Load a saved scene into the engine's scene bank.
    void addScene(SceneValues scene);
    void removeScene(const std::string& sceneId);

    // Recall a scene.
    void recallScene(const std::string& sceneId, SceneRecallMode mode,
                     uint32_t durationTicks = 0);

    // Drive morphing: must be called every tick when morphing is active.
    void tick(uint64_t currentTick);

    // Called for each value update during morph / fade.
    void onValueChange(SceneMorphCallback cb);

    const std::unordered_map<std::string, int>& currentValues() const;

    std::vector<std::string> sceneIds() const;

private:
    void applyJump(const SceneValues& scene);
    void startFade(const SceneValues& target, uint32_t durationTicks,
                   uint64_t startTick);

    std::unordered_map<std::string, int>       m_values;
    std::unordered_map<std::string, SceneValues> m_scenes;
    SceneMorphCallback                           m_callback;

    // Morph state
    struct MorphState {
        std::unordered_map<std::string, int> startValues;
        std::unordered_map<std::string, int> targetValues;
        uint64_t startTick{0};
        uint64_t durationTicks{0};
        bool     active{false};
        SceneRecallMode mode{SceneRecallMode::Jump};
        // For Drop mode: the quantized trigger tick.
        uint64_t triggerTick{0};
    };

    MorphState m_morph{};
};
