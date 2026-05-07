# LFO Engine

**Source files:**
- `engine/src/core/LfoShape.h` — header-only shape evaluator
- `engine/src/core/LfoEngine.h` / `LfoEngine.cpp` — multi-instance clock-driven engine

---

## Architecture

The LFO system is split into two layers:

1. **LfoShape** — pure, stateless function `evaluate(cfg, phase)`. Works the same in C++ and can be replicated in JavaScript for offline UI thumbnail rendering.
2. **LfoEngine** — manages multiple named LFO instances, advances their phase each clock tick, handles randomness cycling, and exposes `getValue(lfoId)`.

---

## Shapes

| Index | Name | Description |
|---|---|---|
| 0 | Sine | Smooth sinusoidal wave |
| 1 | Triangle | Linear rise and fall |
| 2 | Square50 | 50 % duty-cycle square |
| 3 | SawUp | Rising sawtooth |
| 4 | SawDown | Falling sawtooth |
| 5 | SampleHold | Stepped random (new value each cycle) |
| 6 | SmoothRandom | Interpolated random (cubic between cycle values) |
| 7 | ExpRise | Exponential rise curve |
| 8 | ExpFall | Exponential fall curve |
| 9 | Pulse25 | 25 % duty-cycle pulse |
| 10 | Pulse75 | 75 % duty-cycle pulse |
| 11 | StaircaseUp | 8-step staircase rising |
| 12 | StaircaseDown | 8-step staircase falling |
| 13 | Bounce | Sine-squared (ping-pong feel) |
| 14 | SineSquared | Rectified sine, always positive |
| 15 | Trapezoid | Attack–hold–decay–rest envelope (looping) |
| 16 | Syncopated8th | Gate HIGH on off-beats (slots 1,3,5,7 of 8) |
| 17 | Syncopated16th | Son clave pattern `1001001000101000` across 16 slots |
| 18 | DottedGroove | 3+3+2 pattern across 8 slots |
| 19 | Shuffle16th | Each 8th-note pair split 2:1 (2/3 HIGH, 1/3 LOW) |
| 20 | Custom | User-drawn points, linearly interpolated |

### Rhythmic gate envelope

Rhythmic shapes (16–19) divide each full cycle into N equal slots. Each slot is rendered with an attack-hold-decay envelope:

- **0–7 %** of slot width: linear ramp from 0 → 1
- **7–83 %**: hold at 1
- **83–95 %**: linear ramp from 1 → 0
- **95–100 %**: silence at 0

This gives a punchy, musical gate rather than a hard square edge.

---

## LfoConfig Fields

```cpp
struct LfoConfig {
    std::string      lfoId;
    std::string      name;
    LfoShapeType     shape;
    bool             syncToTempo;    // true = clock-sync, false = free-running Hz
    float            rateHz;         // used when syncToTempo = false
    uint32_t         syncDivision;   // ticks per cycle when synced (default = 3840 = 1 bar)
    float            depth;          // 0.0–1.0 output scale
    float            phaseOffset;    // 0.0–1.0 start phase
    bool             bipolar;        // true → [-1, 1], false → [0, 1]
    std::vector<LfoPoint> customPoints; // used when shape = Custom
};
```

---

## Phase Computation

**Tempo-synced:** `phase = (tick % syncDivision) / float(syncDivision)`

**Free-running:** `phase = fmod(tick / (sampleRate / rateHz), 1.0f)` where `sampleRate = 960` ticks/beat × `tempo/60` beats/sec.

Phase is shifted by `phaseOffset` before evaluation.

---

## Randomness (S&H and SmoothRandom)

Each LFO instance stores `prevRandom` and `nextRandom`. At every cycle boundary:

```cpp
prevRandom = nextRandom;
nextRandom = pseudoRandom(++cycle, seed);
```

`pseudoRandom(cycle, seed)` is a deterministic hash function so the same seed always produces the same sequence — useful for reproducible project playback.

SmoothRandom uses cubic interpolation between `prevRandom` and `nextRandom` across the cycle.

---

## External Modulation

| Setter | Effect |
|---|---|
| `setRateMod(lfoId, multiplier)` | Scales cycle duration (0.25×–4×). Called by ModMatrix. |
| `setDepthMod(lfoId, scale)` | Scales output amplitude. Called by ModMatrix. |

---

## WebSocket Commands

| Message | Key fields | Notes |
|---|---|---|
| `add_lfo` | `lfo_id`, `name`, `shape`, rate fields | Creates and starts a new LFO instance |
| `update_lfo` | `lfo_id`, any LfoConfig fields | Live update — takes effect next tick |
| `remove_lfo` | `lfo_id` | Stops and removes |
| `list_lfos` | — | Returns all LfoConfig objects |
| `get_lfo_preview` | `lfo_id`, `resolution` (default 256) | Returns `float[]` for UI waveform display |

Server response for `get_lfo_preview`:

```json
{
  "type": "lfo_preview",
  "lfo_id": "lfo_1",
  "values": [0.5, 0.71, 0.87, ...]
}
```

---

## UI Integration

`ui/js/lfo-editor.js` renders shape thumbnails entirely in JavaScript using its own `evaluateShape()` — a port of `LfoShape::evaluate()` — to avoid a server round-trip per thumbnail. The large preview canvas calls `get_lfo_preview` over WebSocket only when an LFO is selected or its parameters change.

**Custom shape drawing:** Mouse/touch events add, drag, and delete `LfoPoint` objects on the custom canvas. The resulting point array is sent as `custom_points` in `update_lfo`.
