# Modulation Matrix

**Source files:** `engine/src/core/ModMatrix.h` / `ModMatrix.cpp`

---

## Concept

The modulation matrix connects *sources* (things that produce a signal) to *destinations* (parameters that can be driven). Each connection is a `ModRoute` with an `amount` scalar. Multiple routes to the same destination accumulate additively.

`ModMatrix::process(tick)` is called once per engine tick, after LFO advance and before the sequencer step. It reads all source values, scales them by `amount`, and dispatches the results.

---

## Sources

`source` is serialized as an integer in JSON and in the database.

| Int | Enum | Source | Signal range | `source_id` |
|---|---|---|---|---|
| 0 | `Lfo` | Named LFO instance | –1..+1 (bipolar) or 0..1 | LFO ID string |
| 1 | `Velocity` | Last MIDI note-on velocity | 0..1 | `""` |
| 2 | `Aftertouch` | Channel pressure | 0..1 | `""` |
| 3 | `ModWheel` | CC 1 | 0..1 | `""` |
| 4 | `PitchBend` | Pitch bend | –1..+1 | `""` |
| 5 | `MidiCC` | Arbitrary CC | 0..1 | `"cc74"` format |

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

`dest` is serialized as an integer in JSON and in the database.

| Int | Enum | Destination | `dest_id` | How it's applied |
|---|---|---|---|---|
| 0 | `MidiCC` | Arbitrary MIDI CC | control ID string | Accumulated mod → CC 64 ± 63, sent via MidiRouter |
| 1 | `SeqPitch` | Track pitch | track index string | `sequencer.setPitchMod(trackId, semitones)` |
| 2 | `SeqVelocity` | Track velocity | track index string | `sequencer.setVelocityMod(trackId, delta)` |
| 3 | `SeqGate` | Track gate | track index string | `sequencer.setGateMod(trackId, multiplier)` |
| 4 | `SeqRate` | Track step rate | track index string | `sequencer.setRateMod()` — maps [–1,1] → [0.25×,4×] |
| 5 | `LfoRate` | Another LFO's rate | LFO ID string | `lfos.setRateMod(lfoId, multiplier)` |
| 6 | `LfoDepth` | Another LFO's depth | LFO ID string | `lfos.setDepthMod(lfoId, scale)` |
| 7 | `SnapshotMorph` | Scene cross-fade | `"sceneA:sceneB"` | Fires `onMorph(destId, position)` callback |

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
| `add_mod_route` | route fields (see below) | Creates a new route, persists to DB |
| `update_mod_route` | route fields (full object) | Replaces existing route |
| `remove_mod_route` | `route_id` | Removes and deletes from DB |
| `list_mod_routes` | — | Returns all `ModRoute` objects as JSON array |

Route payload fields: `route_id` (string), `source` (int), `source_id` (string), `dest` (int), `dest_id` (string), `amount` (float −1..1), `offset` (float), `enabled` (bool). See [websocket-api.md](websocket-api.md) for the full example.

---

## UI Integration

`ui/js/mod-matrix.js` renders a 6-source × 8-destination table. Clicking a cell opens a floating popup where the user sets amount, selects the LFO instance (for LFO source), CC number (for MidiCC source/dest), and target track (for Seq destinations). Active routes show a percentage badge in the cell.
