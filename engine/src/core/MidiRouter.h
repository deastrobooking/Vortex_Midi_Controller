#pragma once
#include "Types.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class MidiRouter;
class AutomationPlayback;

using ControlEventCallback = std::function<void(const ControlEvent&)>;

// Maps control IDs to MIDI targets and dispatches events.
class MidiRouter {
public:
    MidiRouter();
    ~MidiRouter();

    // Load a mapping from JSON string.
    void loadMappingJson(const std::string& json);

    // Save current mappings to JSON string.
    std::string saveMappingJson() const;

    // Update or insert a target for a control.
    void setTarget(const std::string& controlId, MidiTarget target);

    // Remove all targets for a control.
    void removeTargets(const std::string& controlId);

    // Get all targets for a control (may be empty).
    std::vector<MidiTarget> getTargets(const std::string& controlId) const;

    // Route a control event to MIDI outputs.
    // Returns true if any MIDI was sent.
    bool route(const ControlEvent& evt);

    // Register a callback fired for every routed event (useful for UI feedback).
    void onEvent(ControlEventCallback cb);

private:
    int applyScaling(int raw, const MidiTarget& t) const;

    std::unordered_map<std::string, std::vector<MidiTarget>> m_map;
    ControlEventCallback m_callback;
};
