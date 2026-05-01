#pragma once
#include "Types.h"
#include <functional>
#include <string>
#include <vector>

// A macro maps one source control to multiple MIDI targets with independent
// scaling, curve, and smoothing settings.

struct MacroTarget {
    MidiTarget  midi;
    // Macro-level overrides.
    float       scaleFactor{1.0f};
};

struct Macro {
    std::string          macroId;
    std::string          name;
    std::string          sourceControlId;
    std::vector<MacroTarget> targets;
};

using MacroOutputCallback = std::function<void(const std::string& controlId, int value)>;

class MacroEngine {
public:
    void addMacro(Macro macro);
    void removeMacro(const std::string& macroId);
    bool hasMacro(const std::string& sourceControlId) const;

    // Process a control event. If the control is a macro source, fan it out
    // and fire cb for each derived target.
    void process(const ControlEvent& evt, MacroOutputCallback cb);

    std::vector<Macro> allMacros() const;

private:
    std::vector<Macro> m_macros;
};
