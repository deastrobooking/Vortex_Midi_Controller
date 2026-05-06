#include "ArpEngine.h"
#include <algorithm>
#include <cstring>

namespace vortex {

ArpEngine::ArpEngine(ScaleEngine& scales) : m_scales(scales) {}

// ── Public API ────────────────────────────────────────────────────────────────

void ArpEngine::setConfig(uint8_t trackId, const ArpConfig& cfg) {
  if (trackId >= kMaxTracks) return;
  m_tracks[trackId].config = cfg;
  rebuildArpNotes(m_tracks[trackId]);
}

ArpConfig ArpEngine::getConfig(uint8_t trackId) const {
  if (trackId >= kMaxTracks) return {};
  return m_tracks[trackId].config;
}

void ArpEngine::noteHeld(uint8_t trackId, uint8_t note, uint8_t velocity) {
  if (trackId >= kMaxTracks) return;
  auto& ts = m_tracks[trackId];
  // Avoid duplicates.
  if (std::none_of(ts.held.begin(), ts.held.end(), [note](auto& h){ return h.note == note; }))
    ts.held.push_back({ note, velocity });
  if (!ts.config.latch)
    rebuildArpNotes(ts);
}

void ArpEngine::noteReleased(uint8_t trackId, uint8_t note) {
  if (trackId >= kMaxTracks) return;
  auto& ts = m_tracks[trackId];
  if (ts.config.latch) return;
  ts.held.erase(std::remove_if(ts.held.begin(), ts.held.end(),
    [note](auto& h){ return h.note == note; }), ts.held.end());
  rebuildArpNotes(ts);
}

void ArpEngine::clearHeld(uint8_t trackId) {
  if (trackId >= kMaxTracks) return;
  auto& ts = m_tracks[trackId];
  ts.held.clear();
  rebuildArpNotes(ts);
}

// ── Per-tick advance ──────────────────────────────────────────────────────────

void ArpEngine::advance(uint32_t tick, uint8_t trackId,
                        std::function<void(uint8_t, uint8_t)> noteOn,
                        std::function<void(uint8_t)>          noteOff)
{
  if (trackId >= kMaxTracks) return;
  auto& ts = m_tracks[trackId];
  if (!ts.config.enabled || ts.arpNotes.empty()) return;

  const uint32_t rate = std::max<uint32_t>(1, ts.config.rateTicks);
  const uint32_t gateTicks = std::max<uint32_t>(1,
    static_cast<uint32_t>(rate * ts.config.gatePercent / 100.0f));

  // Initialise on first call.
  if (ts.nextTick == 0) ts.nextTick = tick;

  // Note-off for active note if gate has elapsed.
  if (ts.noteActive && tick >= ts.nextTick - rate + gateTicks) {
    if (noteOff) noteOff(ts.activeNote);
    ts.noteActive = false;
  }

  // Fire next step.
  if (tick >= ts.nextTick) {
    const uint8_t note = nextNote(ts);
    const uint8_t vel  = ts.arpNotes[ts.arpIndex < ts.arpNotes.size() ?
                           ts.arpIndex : 0].velocity;
    if (noteOn) noteOn(note, vel);
    ts.activeNote = note;
    ts.noteActive = true;
    ts.nextTick   = tick + rate;
  }
}

// ── Private helpers ───────────────────────────────────────────────────────────

void ArpEngine::rebuildArpNotes(TrackState& ts) {
  ts.arpNotes.clear();
  ts.arpIndex = 0;
  ts.direction = 1;

  const auto& src = ts.config.latch && !ts.latchBuffer.empty()
                      ? ts.latchBuffer : ts.held;
  if (src.empty()) return;

  // Sort source notes ascending.
  std::vector<HeldNote> sorted = src;
  std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b){ return a.note < b.note; });

  const uint8_t octaves = std::clamp<uint8_t>(ts.config.octaveRange, 1, 4);
  for (uint8_t oct = 0; oct < octaves; ++oct) {
    for (auto& hn : sorted) {
      const uint8_t n = static_cast<uint8_t>(std::min(127, hn.note + oct * 12));
      ts.arpNotes.push_back({ n, hn.velocity });
    }
  }

  // For Down modes, reverse.
  if (ts.config.mode == ArpMode::Down || ts.config.mode == ArpMode::DownUp) {
    std::reverse(ts.arpNotes.begin(), ts.arpNotes.end());
    ts.direction = -1;
  }
}

uint8_t ArpEngine::nextNote(TrackState& ts) {
  if (ts.arpNotes.empty()) return 60;

  const size_t sz = ts.arpNotes.size();
  uint8_t note;

  switch (ts.config.mode) {
    case ArpMode::Random: {
      std::uniform_int_distribution<size_t> dist(0, sz - 1);
      ts.arpIndex = dist(m_rng);
      note = ts.arpNotes[ts.arpIndex].note;
      break;
    }

    case ArpMode::UpDown:
    case ArpMode::DownUp: {
      note = ts.arpNotes[ts.arpIndex].note;
      // Advance with direction bounce.
      if (ts.direction > 0) {
        if (ts.arpIndex + 1 >= sz) {
          ts.direction = -1;
          ts.arpIndex  = sz > 1 ? sz - 2 : 0;
        } else {
          ++ts.arpIndex;
        }
      } else {
        if (ts.arpIndex == 0) {
          ts.direction = 1;
          ts.arpIndex  = sz > 1 ? 1 : 0;
        } else {
          --ts.arpIndex;
        }
      }
      break;
    }

    default: // Up / Down / AsPlayed
      note = ts.arpNotes[ts.arpIndex].note;
      ts.arpIndex = (ts.arpIndex + 1) % sz;
      break;
  }

  return note;
}

} // namespace vortex
