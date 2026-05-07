# Arrangement Engine

**Source files:** `engine/src/core/ArrangeEngine.h` / `ArrangeEngine.cpp`

---

## Overview

The arrangement engine provides a bar-resolution clip timeline. It holds a flat list of `ArrangeClip` objects and fires a callback whenever the playhead enters a bar that has a clip. This allows pattern-switching per track on bar boundaries — e.g. playing an intro pattern on bars 1–4, then a verse pattern on bars 5–12.

---

## Clip Structure

```cpp
struct ArrangeClip {
    uint8_t     trackId;     // which sequencer track this clip drives
    uint32_t    startBar;    // first bar the clip is active (0-indexed)
    uint32_t    length;      // duration in bars (minimum 1)
    std::string patternId;   // "" = use the live pattern; non-empty = load from DB
};
```

Multiple clips on the same track must not overlap (the API does not enforce this — it is the caller's responsibility).

---

## Clock Integration

`ArrangeEngine::advanceBar(bar)` is called from the clock tick callback in `main.cpp` whenever `clock.currentBar()` changes. The engine compares against its internal `m_lastBar` to avoid firing the callback more than once per bar.

```cpp
// In main.cpp clock callback:
arrange.advanceBar(static_cast<uint32_t>(clock.currentBar()));
```

When a bar boundary is reached, the engine finds all clips whose range `[startBar, startBar + length)` contains the current bar, groups them by `trackId`, and fires `patternChangeCb(trackId, patternId)` for each.

---

## Pattern Change Callback

```cpp
arrange.setPatternChangeCb([&](uint8_t trackId, const std::string& patternId) {
    if (!patternId.empty()) {
        // Load pattern from DB and apply to sequencer.
        std::string raw = db.loadPattern(patternId);
        sequencer.loadPatternForTrack(trackId, raw);
    }
    // "" = keep using whatever pattern is already loaded.
});
```

The callback runs on the audio/clock thread. Keep it lock-free or minimally locking.

---

## API

```cpp
// Add or replace a clip.
arrange.setClip(trackId, startBar, length, patternId);

// Remove a clip starting at a given bar on a given track.
arrange.removeClip(trackId, startBar);

// Remove all clips.
arrange.clearAll();

// Read-only access to the clip list.
const std::vector<ArrangeClip>& clips = arrange.clips();
```

---

## WebSocket Commands

| Message | Key fields | Notes |
|---|---|---|
| `arrange_set_clip` | `track_id`, `bar`, `length`, `pattern_id` | Creates or replaces a clip |
| `arrange_remove_clip` | `track_id`, `bar` | Removes the clip starting at that bar |
| `arrange_get_clips` | — | Returns full clip list |

Response for `arrange_get_clips`:

```json
{
  "type": "arrange_clips",
  "clips": [
    {"track_id": 0, "bar": 0, "length": 4, "pattern_id": "intro"},
    {"track_id": 0, "bar": 4, "length": 8, "pattern_id": "verse"}
  ]
}
```

---

## UI Integration

`ui/js/arrange.js` renders a canvas timeline where:

- Rows = sequencer tracks.
- Columns = bars (up to 64 visible).
- **Left-click drag** paints a new clip and extends its length.
- **Right-click** removes the clip under the cursor.
- The current bar is highlighted as a playhead column.
- Clips display the track name and are coloured by track index.

Clip changes are immediately sent via `arrange_set_clip` / `arrange_remove_clip`. There is no separate "save arrangement" step — clips exist in memory and should be persisted to DB explicitly if needed (not yet implemented; add an `arrangements` table to `schema.sql`).

---

## Persistence (not yet implemented)

The clip list is currently in-memory only and is lost on daemon restart. To persist it, add to `schema.sql`:

```sql
CREATE TABLE IF NOT EXISTS arrange_clips (
    project_id  INTEGER NOT NULL,
    track_id    INTEGER NOT NULL,
    start_bar   INTEGER NOT NULL,
    length      INTEGER NOT NULL DEFAULT 1,
    pattern_id  TEXT    NOT NULL DEFAULT '',
    PRIMARY KEY (project_id, track_id, start_bar)
);
```

Then call `db.saveArrangeClips(projectId, arrange.clips())` in the auto-save loop.
