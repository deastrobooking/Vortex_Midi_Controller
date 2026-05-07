# Arpeggiator Engine

**Source files:** `engine/src/core/ArpEngine.h` / `ArpEngine.cpp`

---

## Overview

The arpeggiator is a per-track module that generates a repeating note pattern from a set of simultaneously held notes. It operates at the same 960 PPQN clock as the sequencer. Each of the 64 tracks has its own independent `ArpConfig` and note-hold buffer.

---

## Modes

| Enum | Name | Behaviour |
|---|---|---|
| `Up` | Up | Notes sorted ascending, stepped in order |
| `Down` | Down | Notes sorted descending, stepped in order |
| `UpDown` | Up-Down | Ascending then descending, bouncing at extremes (no repeat at ends) |
| `DownUp` | Down-Up | Descending then ascending, bouncing at extremes |
| `Random` | Random | Each step picks a random note from the held set |
| `AsPlayed` | As Played | Fires in the order notes were originally pressed |

---

## ArpConfig

```cpp
struct ArpConfig {
    bool     enabled;       // false = arp bypassed for this track
    ArpMode  mode;          // Up / Down / UpDown / DownUp / Random / AsPlayed
    uint32_t rateTicks;     // step interval in ticks (240 = 1/4 note @ 960 PPQN)
    uint8_t  octaveRange;   // 1–4: how many octave copies of held notes to include
    bool     latch;         // if true, released notes stay in the arp buffer
    uint8_t  gatePercent;   // 0–100: how long each arp note sounds within its slot
};
```

### Octave range

When `octaveRange > 1`, the note set is duplicated for each octave:

```
held: [C3, E3, G3]   octaveRange=2
arpNotes: [C3, E3, G3, C4, E4, G4]  (sorted per mode)
```

---

## Clock Integration

`ArpEngine::advance(tick, trackId, noteOnCb, noteOffCb)` is called once per tick from the sequencer or directly from `main.cpp`'s clock callback. The arp fires note-on when `tick >= nextTick` and schedules note-off at `nextTick - rateTicks + gateTicks`.

The arp and the step sequencer are independent — enabling the arp on a track does not disable the step sequencer for that track. In practice, you would set the sequencer track to have no active steps, or use a dedicated track for arped parts.

---

## Note Feeding

```cpp
// Called from MIDI input callback when a key is pressed.
arp.noteHeld(trackId, note, velocity);

// Called when a key is released.
arp.noteReleased(trackId, note);

// Clear all held notes (e.g. on transport stop).
arp.clearHeld(trackId);
```

When `latch = true`, `noteReleased` is a no-op — notes accumulate until `clearHeld` is called.

---

## WebSocket Commands

| Message | Key fields | Notes |
|---|---|---|
| `set_arp` | `track_id`, `enabled`, `mode`, `rate_ticks`, `octave_range`, `latch`, `gate_percent` | Applies config immediately |
| `get_arp` | `track_id` | Returns current `ArpConfig` for that track |

Response for `get_arp`:

```json
{
  "type": "arp_config",
  "track_id": 3,
  "enabled": true,
  "mode": 2,
  "rate_ticks": 240,
  "octave_range": 2,
  "latch": false,
  "gate_percent": 50
}
```

---

## Rate Reference

| `rateTicks` | Note value @ 120 BPM |
|---|---|
| 960 | Whole note |
| 480 | Half note |
| 240 | Quarter note |
| 120 | Eighth note |
| 60 | Sixteenth note |
| 40 | Sixteenth note triplet |

---

## Randomness

The `Random` mode uses a `std::mt19937` seeded at construction from `std::random_device`. The seed is not persisted — randomness resets on daemon restart.
