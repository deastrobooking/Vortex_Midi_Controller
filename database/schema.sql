-- Vortex MIDI Workstation — SQLite schema
-- Applied once at daemon startup via Database::applySchema().

PRAGMA journal_mode = WAL;
PRAGMA foreign_keys = ON;

-- ─── Projects ─────────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS projects (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    name          TEXT    NOT NULL DEFAULT 'Untitled',
    tempo         REAL    NOT NULL DEFAULT 120.0,
    last_modified TEXT    NOT NULL DEFAULT (datetime('now'))
);

-- ─── MIDI Mappings ────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS project_mappings (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    project_id  INTEGER NOT NULL UNIQUE REFERENCES projects(id) ON DELETE CASCADE,
    mapping_json TEXT   NOT NULL DEFAULT '{}'
);

-- ─── Scenes ───────────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS scenes (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    project_id  INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    scene_id    TEXT    NOT NULL UNIQUE,
    name        TEXT    NOT NULL DEFAULT 'Untitled'
);

CREATE TABLE IF NOT EXISTS scene_values (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    scene_id    TEXT    NOT NULL REFERENCES scenes(scene_id) ON DELETE CASCADE,
    control_id  TEXT    NOT NULL,
    value       INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_scene_values_scene ON scene_values(scene_id);

-- ─── Automation clips ─────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS automation_clips (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    project_id    INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    clip_id       TEXT    NOT NULL UNIQUE,
    length_ticks  INTEGER NOT NULL DEFAULT 15360,
    loop          INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE IF NOT EXISTS automation_lanes (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    clip_id     TEXT    NOT NULL REFERENCES automation_clips(clip_id) ON DELETE CASCADE,
    control_id  TEXT    NOT NULL,
    UNIQUE(clip_id, control_id)
);

CREATE TABLE IF NOT EXISTS automation_points (
    id      INTEGER PRIMARY KEY AUTOINCREMENT,
    lane_id INTEGER NOT NULL REFERENCES automation_lanes(id) ON DELETE CASCADE,
    tick    INTEGER NOT NULL,
    value   INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_auto_points_lane ON automation_points(lane_id, tick);

-- ─── Sequencer patterns ───────────────────────────────────────────────────────
-- Stored as JSON blobs for maximum flexibility.
-- Each pattern contains up to 64 tracks; each track has up to 64 steps.
-- Step data: active, note, velocity, gate%, accent, slide, pitch_offset.

CREATE TABLE IF NOT EXISTS seq_patterns (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    project_id  INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    pattern_id  TEXT    NOT NULL UNIQUE,
    json_data   TEXT    NOT NULL DEFAULT '{}'
);

CREATE INDEX IF NOT EXISTS idx_seq_patterns_proj ON seq_patterns(project_id);

-- ─── LFO configurations ───────────────────────────────────────────────────────
-- 20 built-in shapes + custom (user-drawn point list).
-- shape enum: 0=Sine … 19=Shuffle16th, 20=Custom.

CREATE TABLE IF NOT EXISTS lfos (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    project_id  INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    lfo_id      TEXT    NOT NULL UNIQUE,
    json_data   TEXT    NOT NULL DEFAULT '{}'
);

CREATE INDEX IF NOT EXISTS idx_lfos_proj ON lfos(project_id);

-- ─── Modulation matrix routes ─────────────────────────────────────────────────
-- Sources: 0=Lfo,1=Velocity,2=Aftertouch,3=ModWheel,4=PitchBend,5=MidiCC,6=SeqCvLane
-- Dests:   0=MidiCC,1=SeqPitch,2=SeqVelocity,3=SeqGate,4=SeqRate,5=LfoRate,6=LfoDepth,7=SnapshotMorph

CREATE TABLE IF NOT EXISTS mod_routes (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    project_id  INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    route_id    TEXT    NOT NULL UNIQUE,
    json_data   TEXT    NOT NULL DEFAULT '{}'
);

CREATE INDEX IF NOT EXISTS idx_mod_routes_proj ON mod_routes(project_id);

-- ─── Custom scales ────────────────────────────────────────────────────────────
-- Built-in scales are reconstructed at runtime by ScaleEngine::makeScale().
-- Only user-defined custom scales are persisted here.
-- degrees: JSON array of 12 booleans, one per semitone C … B.

CREATE TABLE IF NOT EXISTS custom_scales (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    project_id  INTEGER NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    scale_id    TEXT    NOT NULL UNIQUE,
    json_data   TEXT    NOT NULL DEFAULT '{}'
);

CREATE INDEX IF NOT EXISTS idx_scales_proj ON custom_scales(project_id);

-- ─── Settings ─────────────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS settings (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL DEFAULT ''
);

-- Seed a default project if the table is empty.
INSERT OR IGNORE INTO projects (id, name, tempo)
VALUES (1, 'Default Project', 120.0);
