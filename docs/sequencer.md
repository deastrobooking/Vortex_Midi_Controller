# Sequencer Engine

**Source files:** `engine/src/core/SequencerEngine.h` / `SequencerEngine.cpp`

---

## Overview

The sequencer is a 64-track polymetric engine clocked at 960 PPQN (pulses per quarter note). Each track is fully independent — it can have its own step count, step length, swing, base note, and scale. Two modes exist per track: **step** (drum-style, fires a fixed note) and **melodic** (each step carries its own MIDI note number, shown in the piano-roll view).

---

## Data Structures

```
SeqPattern
└── tracks[64]  : SeqTrack
    ├── name         : string
    ├── mode         : Step | Melodic
    ├── stepCount    : uint8_t (1–64)
    ├── stepLenTicks : uint32_t (default 240 = 1/4 note)
    ├── swing        : uint8_t  (0–100 %)
    ├── baseNote     : uint8_t  (used in Step mode)
    ├── channel      : uint8_t  (MIDI channel 1–16)
    ├── useScale     : bool
    ├── muted        : bool
    ├── soloed       : bool
    └── steps[64]    : SeqStep
        ├── active       : bool
        ├── note         : uint8_t (Melodic mode)
        ├── velocity     : uint8_t
        ├── gatePercent  : uint8_t (0–100 %)
        ├── accent       : bool
        ├── slide        : bool
        └── pitchOffset  : int8_t
```

---

## Clock Integration

`SequencerEngine::advance(prevTick, tick)` is called once per engine tick from `main.cpp` inside the `ClockEngine::onTick` callback.

**Step firing algorithm:**

1. On first call (`nextStepTick == 0`), set `nextStepTick = tick`.
2. While `nextStepTick <= tick`:
   - Fire any pending note-off for the previous step.
   - Compute `stepLen = baseStepLen * rateMod`, then add 33 % for odd-step swing.
   - Look up `steps[currentStep]`. If `active`, send note-on.
   - Schedule note-off at `nextStepTick + stepLen * gateRatio`.
   - Advance `currentStep = (currentStep + 1) % stepCount`.
   - Advance `nextStepTick += stepLen`.

**Slide:** When a step has `slide = true`, the note-off for the previous step is suppressed so notes overlap.

**Mute vs Solo:**
- Muted tracks: playhead still advances, but note-on is suppressed.
- `m_anySolo` flag: when any track is soloed, only soloed tracks fire note-on.

---

## Note Resolution

Final MIDI note = `baseNote + pitchOffset + pitchMod` (Step mode) or `step.note + pitchOffset + pitchMod` (Melodic mode), clamped to 0–127, then optionally scale-quantized via `ScaleEngine::quantize()`.

---

## Modulation Inputs

| Setter | Effect |
|---|---|
| `setPitchMod(trackId, semitones)` | Adds to every fired note on that track |
| `setVelocityMod(trackId, delta)` | Adds to every fired velocity (clamped 1–127) |
| `setGateMod(trackId, multiplier)` | Scales gate ticks (0.25×–4×) |
| `setRateMod(trackId, multiplier)` | Scales step length (0.25×–4×) |

These are called by `ModMatrix::process()` each tick.

---

## WebSocket Commands

| Message type | Key fields | Effect |
|---|---|---|
| `get_sequencer_state` | — | Returns full pattern + current playhead positions |
| `set_track` | `track_id`, track properties | Updates track metadata |
| `set_step` | `track_id`, `step_idx`, step fields | Sets a single step |
| `clear_track` | `track_id` | Deactivates all steps on track |
| `mute_track` | `track_id`, `muted` | Mute/unmute |
| `solo_track` | `track_id`, `solo` | Solo/unsolo |
| `save_pattern` | `pattern_id`, `name` | Serializes full pattern to SQLite |
| `load_pattern` | `pattern_id` | Deserializes and loads |
| `list_patterns` | `project_id` | Returns array of pattern IDs |

---

## Persistence

Patterns are stored as JSON blobs in the `seq_patterns` table:

```json
{
  "pattern_id": "pat_1",
  "name": "Intro",
  "tracks": [
    {
      "name": "Kick",
      "stepCount": 16,
      "steps": [{"active": true, "note": 36, "velocity": 100, "gate_percent": 75}, ...]
    }
  ]
}
```

---

## UI Integration

- **Sequencer tab** — `ui/js/sequencer.js`: canvas grid; click = toggle step; right-click = step detail modal.
- **Piano Roll tab** — `ui/js/piano-roll.js`: melodic note editor; track and scale selectors at top.
- Both panels listen for `sequencer_state` events and re-render immediately.
