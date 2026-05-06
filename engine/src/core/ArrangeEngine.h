#pragma once
#include "Types.h"
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

namespace vortex {

struct ArrangeClip {
  uint8_t     trackId;
  uint32_t    startBar;   // inclusive
  uint32_t    length;     // bars
  std::string patternId;  // "" means use the current live pattern
};

// Bar-resolution arrangement engine.
// Call advance(bar) each time the playhead enters a new bar.
// Fires onPatternChange when the clip under the playhead changes.
class ArrangeEngine {
public:
  using PatternChangeCb = std::function<void(uint8_t trackId, const std::string& patternId)>;

  ArrangeEngine() = default;

  // Register callback — called on the audio thread, keep it lock-free.
  void setPatternChangeCb(PatternChangeCb cb) { m_cb = std::move(cb); }

  // Called by the clock engine on bar boundaries.
  void advanceBar(uint32_t bar);

  // Clip CRUD — call from the message handler (any thread).
  void setClip(uint8_t trackId, uint32_t startBar, uint32_t length,
               const std::string& patternId = "");
  void removeClip(uint8_t trackId, uint32_t startBar);
  void clearAll();

  // Accessors.
  const std::vector<ArrangeClip>& clips() const { return m_clips; }

private:
  std::vector<ArrangeClip> m_clips;
  uint32_t                 m_lastBar = UINT32_MAX;
  PatternChangeCb          m_cb;

  const ArrangeClip* findClip(uint8_t trackId, uint32_t bar) const;
};

} // namespace vortex
