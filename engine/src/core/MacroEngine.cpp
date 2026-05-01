#include "MacroEngine.h"
#include <algorithm>
#include <cmath>

void MacroEngine::addMacro(Macro macro) {
    // Enforce max 8 targets per macro.
    if (macro.targets.size() > 8)
        macro.targets.resize(8);

    m_macros.push_back(std::move(macro));
}

void MacroEngine::removeMacro(const std::string& macroId) {
    m_macros.erase(
        std::remove_if(m_macros.begin(), m_macros.end(),
                       [&macroId](const Macro& m) { return m.macroId == macroId; }),
        m_macros.end());
}

bool MacroEngine::hasMacro(const std::string& sourceControlId) const {
    return std::any_of(m_macros.begin(), m_macros.end(),
                       [&sourceControlId](const Macro& m) {
                           return m.sourceControlId == sourceControlId;
                       });
}

void MacroEngine::process(const ControlEvent& evt, MacroOutputCallback cb) {
    for (const auto& macro : m_macros) {
        if (macro.sourceControlId != evt.controlId) continue;

        for (const auto& mt : macro.targets) {
            double norm = std::clamp(static_cast<double>(evt.value) / 4095.0,
                                     0.0, 1.0);

            if (mt.midi.curve == "exponential")
                norm = norm * norm;
            else if (mt.midi.curve == "logarithmic" && norm > 0)
                norm = std::log(1.0 + norm * (M_E - 1.0));

            if (mt.midi.invert) norm = 1.0 - norm;
            norm = std::clamp(norm * static_cast<double>(mt.scaleFactor), 0.0, 1.0);

            int range = mt.midi.maxValue - mt.midi.minValue;
            int value = static_cast<int>(
                std::round(mt.midi.minValue + norm * range));

            // Emit a synthetic control event for each target.
            ControlEvent derived;
            derived.controlId      = macro.macroId + "_out_" +
                                     std::to_string(mt.midi.ccNumber);
            derived.value          = value;
            derived.timestampTicks = evt.timestampTicks;

            if (cb) cb(derived.controlId, value);
        }
    }
}

std::vector<Macro> MacroEngine::allMacros() const { return m_macros; }
