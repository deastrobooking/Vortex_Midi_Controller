#pragma once
#include "Types.h"
#include "ScaleEngine.h"
#include <vector>
#include <random>
#include <functional>

namespace vortex {

enum class ArpMode : uint8_t {
  Up       = 0,
  Down     = 1,
  UpDown   = 2,
  DownUp   = 3,
  Random   = 4,
  AsPlayed = 5,
};

struct ArpConfig {
  bool      enabled     = false;
  ArpMode   mode        = ArpMode::Up;
  uint32_t  rateTicks   = 240;  // step interval in ticks (240 = 1/4 note @ 960ppqn)
  uint8_t   octaveRange = 1;    // 1–4: add notes in upper octaves
  bool      latch       = false;
  uint8_t   gatePercent = 50;
};

// Per-track arpeggiator — driven by advance() from SequencerEngine each tick.
class ArpEngine {
public:
  explicit ArpEngine(ScaleEngine& scales);

  // Call once per tick for a given track.
  // Fires noteOn/noteOff callbacks with (note, velocity).
  void advance(uint32_t tick, uint8_t trackId,
               std::function<void(uint8_t note, uint8_t vel)> noteOn,
               std::function<void(uint8_t note)>              noteOff);

  // Feed held notes from the host (keyboard / MIDI input).
  void noteHeld(uint8_t trackId, uint8_t note, uint8_t velocity);
  void noteReleased(uint8_t trackId, uint8_t note);
  void clearHeld(uint8_t trackId);

  // Config per track.
  void         setConfig(uint8_t trackId, const ArpConfig& cfg);
  ArpConfig    getConfig(uint8_t trackId) const;

private:
  static constexpr size_t kMaxTracks = 64;

  struct HeldNote { uint8_t note; uint8_t velocity; };

  struct TrackState {
    ArpConfig             config;
    std::vector<HeldNote> held;
    std::vector<HeldNote> latchBuffer;
    std::vector<HeldNote> arpNotes;   // expanded with octaves, sorted per mode
    size_t                arpIndex   = 0;
    int                   direction  = 1;   // +1 or -1 for UpDown
    uint32_t              nextTick   = 0;
    uint8_t               activeNote = 255; // 255 = none
    bool                  noteActive = false;
  };

  void     rebuildArpNotes(TrackState& ts);
  uint8_t  nextNote(TrackState& ts);

  std::array<TrackState, kMaxTracks> m_tracks;
  ScaleEngine&                       m_scales;
  std::mt19937                       m_rng{ std::random_device{}() };
};

} // namespace vortex
