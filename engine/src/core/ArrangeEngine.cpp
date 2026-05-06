#include "ArrangeEngine.h"
#include <algorithm>

namespace vortex {

void ArrangeEngine::advanceBar(uint32_t bar) {
  if (bar == m_lastBar) return;
  m_lastBar = bar;
  if (!m_cb) return;

  // Fire callback for every track whose clip changes at this bar.
  // Group clips by trackId and find the active one.
  std::unordered_map<uint8_t, const ArrangeClip*> active;
  for (const auto& c : m_clips) {
    if (bar >= c.startBar && bar < c.startBar + c.length)
      active[c.trackId] = &c;
  }
  for (auto& [tid, clip] : active)
    m_cb(tid, clip->patternId);
}

void ArrangeEngine::setClip(uint8_t trackId, uint32_t startBar,
                             uint32_t length, const std::string& patternId)
{
  // Remove any existing clip at the same position.
  removeClip(trackId, startBar);
  m_clips.push_back({ trackId, startBar, std::max<uint32_t>(1, length), patternId });
}

void ArrangeEngine::removeClip(uint8_t trackId, uint32_t startBar) {
  m_clips.erase(std::remove_if(m_clips.begin(), m_clips.end(),
    [trackId, startBar](const auto& c){
      return c.trackId == trackId && c.startBar == startBar;
    }), m_clips.end());
}

void ArrangeEngine::clearAll() {
  m_clips.clear();
}

const ArrangeClip* ArrangeEngine::findClip(uint8_t trackId, uint32_t bar) const {
  for (const auto& c : m_clips)
    if (c.trackId == trackId && bar >= c.startBar && bar < c.startBar + c.length)
      return &c;
  return nullptr;
}

} // namespace vortex
