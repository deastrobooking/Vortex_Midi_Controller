# WebSocket API Reference

The engine daemon (`midiworkstationd`) listens on **`ws://localhost:8080`**. All messages are UTF-8 JSON objects with a `type` string field. The UI sends commands; the daemon sends responses and push events.

---

## Connection Lifecycle

On connect the UI sends:

```json
{ "type": "get_project_state" }
{ "type": "get_sequencer_state" }
{ "type": "list_lfos" }
{ "type": "list_mod_routes" }
{ "type": "list_scales" }
{ "type": "list_scenes" }
{ "type": "list_patterns", "project_id": 1 }
```

The daemon also broadcasts `project_state` on every clock beat, and `sequencer_state` whenever a step fires.

---

## Transport & Project

### get_project_state → project_state

```json
// Request
{ "type": "get_project_state" }

// Response (also broadcast every beat)
{
  "type": "project_state",
  "tempo": 120.0,
  "playing": true,
  "current_tick": 1920,
  "current_beat": 2,
  "current_bar": 0,
  "scene_ids": ["scene_1", "scene_2"]
}
```

### transport

```json
{ "type": "transport", "action": "play" }
{ "type": "transport", "action": "stop" }
{ "type": "transport", "action": "continue" }
{ "type": "transport", "action": "tap" }
```

Response: `{ "type": "transport_ack" }` — the UI then re-fetches `project_state`.

### set_tempo → tempo_set

```json
{ "type": "set_tempo", "tempo": 128.5 }

// Broadcast
{ "type": "tempo_set", "tempo": 128.5 }
```

---

## Sequencer

### get_sequencer_state → sequencer_state

```json
// Response — broadcast on every step fire too
{
  "type": "sequencer_state",
  "pattern_id": "pat_1",
  "name": "Intro",
  "tracks": [
    {
      "name": "Kick", "mode": 0, "step_count": 16, "step_len_ticks": 240,
      "swing": 0, "base_note": 36, "channel": 10, "use_scale": false,
      "muted": false, "soloed": false,
      "steps": [
        {"active": true, "note": 36, "velocity": 100, "gate_percent": 75,
         "accent": false, "slide": false, "pitch_offset": 0},
        ...
      ]
    },
    ...
  ],
  "current_steps": [0, 0, 3, 0, ...]
}
```

`current_steps[i]` is the playhead step index for track i.

### set_track

```json
{
  "type": "set_track", "track_id": 0,
  "name": "Kick", "mode": 0, "step_count": 16,
  "step_len_ticks": 240, "swing": 20,
  "base_note": 36, "channel": 10, "use_scale": false
}
```

### set_step

```json
{
  "type": "set_step", "track_id": 0, "step_idx": 0,
  "active": true, "note": 36, "velocity": 100,
  "gate_percent": 75, "accent": false, "slide": false, "pitch_offset": 0
}
```

### clear_track / mute_track / solo_track

```json
{ "type": "clear_track", "track_id": 2 }
{ "type": "mute_track",  "track_id": 2, "muted": true }
{ "type": "solo_track",  "track_id": 2, "solo":  true }
```

### save_pattern / load_pattern / list_patterns

```json
{ "type": "save_pattern", "pattern_id": "pat_intro", "name": "Intro", "project_id": 1 }
// Response: { "type": "pattern_saved", "pattern_id": "pat_intro" }

{ "type": "load_pattern", "pattern_id": "pat_intro" }
// Response: { "type": "pattern_loaded", "pattern_id": "pat_intro" }
// Followed by get_sequencer_state auto-fetch from client.

{ "type": "list_patterns", "project_id": 1 }
// Response: { "type": "patterns_list", "pattern_ids": ["pat_intro", "pat_verse"] }
```

---

## LFO

### add_lfo / update_lfo / remove_lfo

```json
{
  "type": "add_lfo",
  "lfo_id": "lfo_1", "name": "Filter LFO",
  "shape": 0,          // 0=Sine … 20=Custom
  "sync_to_tempo": true,
  "rate_hz": 1.0,
  "sync_division": 3840,   // ticks (3840 = 1 bar @ 960 PPQN)
  "depth": 1.0,
  "phase_offset": 0.0,
  "bipolar": true,
  "custom_points": []
}
// Response: { "type": "lfo_added", "lfo_id": "lfo_1" }

{ "type": "update_lfo", "lfo_id": "lfo_1", "depth": 0.5 }
// Response: { "type": "lfo_updated", "lfo_id": "lfo_1" }

{ "type": "remove_lfo", "lfo_id": "lfo_1" }
// Response: { "type": "lfo_removed", "lfo_id": "lfo_1" }
```

### list_lfos → lfos_list

```json
{ "type": "list_lfos" }
// Response: { "type": "lfos_list", "lfos": [ { ...LfoConfig... }, ... ] }
```

### get_lfo_preview → lfo_preview

```json
{ "type": "get_lfo_preview", "lfo_id": "lfo_1", "resolution": 256 }
// Response: { "type": "lfo_preview", "lfo_id": "lfo_1", "values": [0.5, 0.71, ...] }
```

---

## Modulation Matrix

### add_mod_route / update_mod_route / remove_mod_route

`source` and `dest` are **integer enum values** (not strings). See `ModSource` / `ModDest` in `Types.h`.

| `source` | Enum | `source_id` |
|---|---|---|
| 0 | Lfo | LFO instance ID string |
| 1 | Velocity | `""` |
| 2 | Aftertouch | `""` |
| 3 | ModWheel | `""` |
| 4 | PitchBend | `""` |
| 5 | MidiCC | `"cc74"` (e.g. CC 74) |

| `dest` | Enum | `dest_id` |
|---|---|---|
| 0 | MidiCC | control ID string |
| 1–4 | SeqPitch/Vel/Gate/Rate | track index as string (`"0"`) |
| 5–6 | LfoRate/Depth | LFO instance ID string |
| 7 | SnapshotMorph | `"sceneA:sceneB"` pair |

```json
{
  "type": "add_mod_route",
  "route_id": "r_unique_id",
  "source": 0,
  "source_id": "lfo_1",
  "dest": 0,
  "dest_id": "filter_cc",
  "amount": 0.8,
  "offset": 0.0,
  "enabled": true
}

{ "type": "update_mod_route", "route_id": "r1", "source": 0, "source_id": "lfo_1",
  "dest": 0, "dest_id": "filter_cc", "amount": 0.5, "offset": 0.0, "enabled": true }
{ "type": "remove_mod_route", "route_id": "r1" }
```

### list_mod_routes → mod_routes_list

```json
{ "type": "list_mod_routes" }
// Response: { "type": "mod_routes_list", "routes": [...] }
```

---

## Scales

```json
{ "type": "list_scales" }
// Response: { "type": "scales_list", "scales": [ { "scale_id": "...", "name": "...", "degrees": [true,...] } ] }

{
  "type": "add_scale", "scale_id": "my_scale", "name": "Hirajoshi",
  "root": 0,
  "degrees": [true,false,true,false,false,true,false,false,true,false,false,false]
}
// Response: { "type": "scale_added", "scale_id": "my_scale" }

{ "type": "remove_scale", "scale_id": "my_scale" }
// Response: { "type": "scale_removed", "scale_id": "my_scale" }
```

---

## Scenes

```json
// Capture current control state as a named scene.
// scene_id must be supplied by the caller (unique, URL-safe string).
{ "type": "save_scene", "scene_id": "scene_verse_1748000000000", "name": "Verse" }
// Response: { "type": "scene_saved", "scene_id": "scene_verse_..." }
// Then the daemon broadcasts scenes_list automatically.

// Recall a scene (jumps instantly by default).
{ "type": "trigger_scene", "scene_id": "scene_1", "mode": "jump", "duration_bars": 4 }
// mode: "jump" | "fade" | "drop" | "morph"   (default: "jump")
// Response: { "type": "scene_triggered", "scene_id": "scene_1" }

// Delete a scene.
{ "type": "delete_scene", "scene_id": "scene_1" }
// Response: { "type": "scene_deleted", "scene_id": "scene_1" }
// Then the daemon broadcasts scenes_list automatically.

{ "type": "list_scenes" }
// Response: { "type": "scenes_list", "scenes": ["scene_1", "scene_2"] }
```

---

## MIDI I/O

```json
{ "type": "discover_midi_inputs" }
// Response:
{
  "type": "midi_inputs_list",
  "ports": [
    { "id": 1, "name": "Arturia KeyStep", "hw_addr": "0:1", "open": false }
  ]
}

{ "type": "open_midi_input",  "port_id": 1 }
{ "type": "close_midi_input", "port_id": 1 }
```

---

## Arpeggiator

```json
{
  "type": "set_arp", "track_id": 3,
  "enabled": true, "mode": 2, "rate_ticks": 120,
  "octave_range": 2, "latch": false, "gate_percent": 60
}
// Response: { "type": "arp_set", "track_id": 3 }

{ "type": "get_arp", "track_id": 3 }
// Response: { "type": "arp_config", "track_id": 3, ... }
```

---

## Arrangement

```json
{ "type": "arrange_set_clip",    "track_id": 0, "bar": 4, "length": 8, "pattern_id": "verse" }
{ "type": "arrange_remove_clip", "track_id": 0, "bar": 4 }
{ "type": "arrange_get_clips" }
// Response: { "type": "arrange_clips", "clips": [...] }
```

---

## Push Events (Daemon → UI, unsolicited)

| Event type | When sent |
|---|---|
| `project_state` | Every beat tick; after transport commands |
| `sequencer_state` | Every step fire; after set_track / set_step / load_pattern |
| `control_values` | When a hardware control changes |
| `lfo_preview` | Response to `get_lfo_preview` |
| `lfos_list` | After any lfo_added / lfo_updated / lfo_removed |
| `mod_routes_list` | After any mod_route_added / updated / removed |
| `scenes_list` | After scene_saved or scene_deleted |
| `patterns_list` | After pattern_saved |
| `tempo_set` | When BPM changes |
| `transport_ack` | After transport command processed |
| `arp_config` | Response to `get_arp` |
| `arrange_clips` | Response to `arrange_get_clips` |

## Acknowledge-only Responses (no state update)

These are sent back to the requesting client but do not carry payload data:

`arp_set`, `arrange_clip_set`, `arrange_clip_removed`, `scene_triggered`,
`mapping_updated`, `record_started`, `record_stopped`, `midi_input_opened`,
`midi_input_closed`.
