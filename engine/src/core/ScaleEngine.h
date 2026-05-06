#pragma once
#include "Types.h"
#include <string>
#include <vector>

class ScaleEngine {
public:
    // ── Built-in factory ─────────────────────────────────────────────────────
    static Scale makeScale(ScaleMode mode, int root = 0);
    static std::vector<Scale> allBuiltin();   // all 16 non-custom modes, root=C

    // ── Pitch operations (static, no instance needed) ─────────────────────
    // Quantize a MIDI note (0–127) to the nearest in-scale degree.
    // Preference for up when tied.
    static int quantize(int note, const Scale& scale);

    // Transpose by n scale degrees (positive = up, negative = down).
    // Stays within the 0–127 range.
    static int transposeDegrees(int note, int degrees, const Scale& scale);

    // Return true if note class is a member of the scale.
    static bool inScale(int note, const Scale& scale);

    // ── Instance: user/project custom scales ─────────────────────────────
    void addScale(Scale s);
    void removeScale(const std::string& scaleId);
    const Scale* find(const std::string& scaleId) const;
    std::vector<Scale> allScales() const;  // builtin + custom

private:
    static std::array<bool, 12> degreesForMode(ScaleMode mode);

    std::vector<Scale> m_custom;
};
