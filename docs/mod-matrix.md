# Modulation Matrix

**Source files:** `engine/src/core/ModMatrix.h` / `ModMatrix.cpp`

---

## Concept

The modulation matrix connects *sources* (things that produce a signal) to *destinations* (parameters that can be driven). Each connection is a `ModRoute` with an `amount` scalar. Multiple routes to the same destination accumulate additively.

`ModMatrix::process(tick)` is called once per engine tick, after LFO advance and before the sequencer step. It reads all source values, scales them by `amount`, and dispatches the results.

---

## Sources

| Enum value | Source | Signal range |
|---|---|---|
| `ModSource::Lfo` | Named LFO instance (`sourceId` = lfoId) | –1 to +1 (bipolar) or 0–1 |
| `ModSource::Velocity` | Last MIDI note-on velocity per channel | 0–1 |
| `ModSource::Aftertouch` | Channel pressure | 0–1 |
| `ModSource::ModWheel` | CC 1 | 0–1 |
| `ModSource::PitchBend` | Pitch bend | –1 to +1 |
| `ModSource::MidiCC` | Arbitrary CC number (`controlId`) | 0–1 |

Source values are fed into the matrix via:

```cpp
modMatrix.feedVelocity(channel, value127);
modMatrix.feedAftertouch(channel, value127);
modMatrix.feedPitchBend(channel, value14bit);
modMatrix.feedCC(channel, ccNumber, value127);
```

These are called from the MIDI input callback in `main.cpp`.

---

## Destinations

| Enum value | Destination | How it's applied |
|---|---|---|
| `ModDest::MidiCC` | Arbitrary MIDI CC | Accumulated mod added to CC 64 baseline, sent via MidiRouter |
| `ModDest::SeqPitch` | Track pitch | `sequencer.setPitchMod(trackId, semitones)` |
| `ModDest::SeqVelocity` | Track velocity | `sequencer.setVelocityMod(trackId, delta)` |
| `ModDest::SeqGate` | Track gate % | `sequencer.setGateMod(trackId, multiplier)` |
| `ModDest::SeqRate` | Track step rate | `sequencer.setRateMod(trackId, multiplier)` — maps [–1, 1] → [0.25×, 4×] |
| `ModDest::LfoRate` | Another LFO's rate | `lfos.setRateMod(lfoId, multiplier)` |
| `ModDest::LfoDepth` | Another LFO's depth | `lfos.setDepthMod(lfoId, scale)` |
| `ModDest::SnapshotMorph` | Scene cross-fade | Fires `onMorph(destId, position)` callback |

---

## ModRoute Structure

```cpp
struct ModRoute {
    std::string routeId;    // unique ID (UUID or user-supplied)
    ModSource   source;
    std::string sourceId;   // lfoId for Lfo source; cc number for MidiCC
    ModDest     dest;
    std::string destId;     // trackId, lfoId, or "sceneA:sceneB" for morph
    float       amount;     // –1.0 to +1.0 scale factor
    float       offset;     // added after scaling (rarely used)
    bool        enabled;
};
```

---

## CC Modulation Detail

For `MidiCC` destinations, the matrix accumulates the normalized mod sum across all active routes targeting the same `controlId`, then sends:

```
outputCC = clamp(64 + sum * 63, 0, 127)
```

This means amount `+1.0` drives the CC to 127, amount `–1.0` drives it to 0, and amount `0` leaves it at 64 (centre).

The CC event is sent through `MidiRouter` so any curve/range/invert mappings defined for that control still apply.

---

## Processing Order Per Tick

```
1. lfos.advance(tick)           — update LFO phases
2. modMatrix.process(tick)      — read sources, dispatch to destinations
3. sequencer.advance(tick)      — step fires use the already-updated mod values
```

This single-tick latency is intentional and negligible at 960 PPQN.

---

## WebSocket Commands

| Message | Key fields | Notes |
|---|---|---|
| `add_mod_route` | route fields | Creates a new route, persists to DB |
| `update_mod_route` | `route_id` + route fields | Replaces existing route |
| `remove_mod_route` | `route_id` | Removes and deletes from DB |
| `list_mod_routes` | — | Returns all `ModRoute` objects as JSON array |

---

## UI Integration

`ui/js/mod-matrix.js` renders a 6-source × 8-destination table. Clicking a cell opens a floating popup where the user sets amount, selects the LFO instance (for LFO source), CC number (for MidiCC source/dest), and target track (for Seq destinations). Active routes show a percentage badge in the cell.
