# Database Schema

**File:** `database/schema.sql`
**Engine:** SQLite 3 (accessed via `engine/src/database/Database.h`)

The schema is applied by the daemon on startup via `Database::applySchema(path)`. It uses `CREATE TABLE IF NOT EXISTS` throughout, so it is safe to apply against an existing database.

---

## Tables

### projects

```sql
CREATE TABLE IF NOT EXISTS projects (
    id      INTEGER PRIMARY KEY AUTOINCREMENT,
    name    TEXT    NOT NULL DEFAULT 'Untitled',
    tempo   REAL    NOT NULL DEFAULT 120.0
);
```

The daemon reads `settings.last_project_id` on startup to select the active project.

---

### project_mappings

```sql
CREATE TABLE IF NOT EXISTS project_mappings (
    project_id  INTEGER NOT NULL,
    json_data   TEXT    NOT NULL,
    PRIMARY KEY (project_id)
);
```

Stores the full `MidiRouter` mapping as a single JSON blob per project. Serialized format is internal to `MidiRouter::saveMappingJson()`.

---

### scenes / scene_values

```sql
CREATE TABLE IF NOT EXISTS scenes (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    project_id  INTEGER NOT NULL,
    name        TEXT    NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS scene_values (
    scene_id    INTEGER NOT NULL,
    control_id  TEXT    NOT NULL,
    value       INTEGER NOT NULL,
    PRIMARY KEY (scene_id, control_id),
    FOREIGN KEY (scene_id) REFERENCES scenes(id) ON DELETE CASCADE
);
```

---

### automation_clips / automation_lanes / automation_points

```sql
CREATE TABLE IF NOT EXISTS automation_clips (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    project_id  INTEGER NOT NULL,
    name        TEXT    NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS automation_lanes (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    clip_id     INTEGER NOT NULL,
    control_id  TEXT    NOT NULL,
    FOREIGN KEY (clip_id) REFERENCES automation_clips(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS automation_points (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    lane_id     INTEGER NOT NULL,
    tick        INTEGER NOT NULL,
    value       INTEGER NOT NULL,
    FOREIGN KEY (lane_id) REFERENCES automation_lanes(id) ON DELETE CASCADE
);
```

---

### seq_patterns

```sql
CREATE TABLE IF NOT EXISTS seq_patterns (
    project_id  INTEGER NOT NULL,
    pattern_id  TEXT    NOT NULL,
    json_data   TEXT    NOT NULL,
    PRIMARY KEY (project_id, pattern_id)
);
```

**JSON blob format:**

```json
{
  "pattern_id": "pat_intro",
  "name": "Intro",
  "tracks": [
    {
      "name": "Kick",
      "mode": 0,
      "step_count": 16,
      "step_len_ticks": 240,
      "swing": 0,
      "base_note": 36,
      "channel": 10,
      "use_scale": false,
      "steps": [
        {
          "active": true,
          "note": 36,
          "velocity": 100,
          "gate_percent": 75,
          "accent": false,
          "slide": false,
          "pitch_offset": 0
        }
      ]
    }
  ]
}
```

Up to 64 tracks per pattern, up to 64 steps per track. Unused tracks/steps are simply omitted.

---

### lfos

```sql
CREATE TABLE IF NOT EXISTS lfos (
    project_id  INTEGER NOT NULL,
    lfo_id      TEXT    NOT NULL,
    json_data   TEXT    NOT NULL,
    PRIMARY KEY (project_id, lfo_id)
);
```

**JSON blob format:**

```json
{
  "lfo_id": "lfo_1",
  "name": "Filter LFO",
  "shape": 0,
  "sync_to_tempo": true,
  "rate_hz": 1.0,
  "sync_division": 3840,
  "depth": 1.0,
  "phase_offset": 0.0,
  "bipolar": true,
  "custom_points": [
    {"phase": 0.0, "value": 0.0},
    {"phase": 0.5, "value": 1.0},
    {"phase": 1.0, "value": 0.0}
  ]
}
```

`shape` values 0–20 correspond to `LfoShapeType` enum. `custom_points` is only used when `shape = 20`.

---

### mod_routes

```sql
CREATE TABLE IF NOT EXISTS mod_routes (
    project_id  INTEGER NOT NULL,
    route_id    TEXT    NOT NULL,
    json_data   TEXT    NOT NULL,
    PRIMARY KEY (project_id, route_id)
);
```

**JSON blob format:**

```json
{
  "route_id": "r1",
  "source": 0,
  "source_id": "lfo_1",
  "dest": 0,
  "dest_id": "filter_cc",
  "amount": 0.8,
  "offset": 0.0,
  "enabled": true
}
```

`source` and `dest` are integer enum values matching `ModSource` / `ModDest` in `Types.h` (see [mod-matrix.md](mod-matrix.md) for the full table).

---

### custom_scales

```sql
CREATE TABLE IF NOT EXISTS custom_scales (
    project_id  INTEGER NOT NULL,
    scale_id    TEXT    NOT NULL,
    json_data   TEXT    NOT NULL,
    PRIMARY KEY (project_id, scale_id)
);
```

**JSON blob format:**

```json
{
  "scale_id": "hirajoshi",
  "name": "Hirajoshi",
  "root": 0,
  "degrees": [true,false,true,false,false,true,false,false,true,false,false,false]
}
```

`degrees` is always a 12-element boolean array indexed by semitone class (0 = C, 11 = B).

---

### settings

```sql
CREATE TABLE IF NOT EXISTS settings (
    key     TEXT PRIMARY KEY,
    value   TEXT NOT NULL DEFAULT ''
);
```

Key-value store for daemon configuration. Common keys:

| Key | Example | Description |
|---|---|---|
| `last_project_id` | `1` | Loaded on startup |
| `module_0_port` | `/dev/ttyUSB0` | RP2040 serial port |
| `midi_input_0_port_id` | `1` | ALSA input port opened on startup |

---

## Upsert Pattern

All `save*` methods use SQLite's `INSERT OR REPLACE` (or `ON CONFLICT DO UPDATE`) so saving is idempotent — calling save twice with the same ID updates the record rather than creating a duplicate.

---

## Migrations

The schema does not have a version table. Additive changes (new tables, new columns with defaults) are safe to apply against existing databases. Breaking changes (renaming columns, dropping tables) require a manual migration script or deleting the database file and starting fresh.
