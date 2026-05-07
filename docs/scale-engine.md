# Scale Engine

**Source files:** `engine/src/core/ScaleEngine.h` / `ScaleEngine.cpp`

---

## Overview

The scale engine provides two services:

1. **Static factory** — `makeScale(mode, root)` constructs a `Scale` object with the correct 12-bool degree table for any of the 16 built-in modes, transposed to any root note.
2. **Instance store** — an `ScaleEngine` object holds a list of project-specific scales (built-in and user-defined) that can be looked up by ID.

---

## Built-in Scale Modes

| Enum | Name | Semitone pattern |
|---|---|---|
| `Major` | Major / Ionian | 2 2 1 2 2 2 1 |
| `NaturalMinor` | Natural Minor / Aeolian | 2 1 2 2 1 2 2 |
| `HarmonicMinor` | Harmonic Minor | 2 1 2 2 1 3 1 |
| `MelodicMinor` | Melodic Minor | 2 1 2 2 2 2 1 |
| `Dorian` | Dorian | 2 1 2 2 2 1 2 |
| `Phrygian` | Phrygian | 1 2 2 2 1 2 2 |
| `Lydian` | Lydian | 2 2 2 1 2 2 1 |
| `Mixolydian` | Mixolydian | 2 2 1 2 2 1 2 |
| `Locrian` | Locrian | 1 2 2 1 2 2 2 |
| `PentatonicMajor` | Major Pentatonic | 2 2 3 2 3 |
| `PentatonicMinor` | Minor Pentatonic | 3 2 2 3 2 |
| `Blues` | Blues | 3 2 1 1 3 2 |
| `WholeTone` | Whole Tone | 2 2 2 2 2 2 |
| `Diminished` | Diminished (half-whole) | 1 2 1 2 1 2 1 2 |
| `Augmented` | Augmented | 3 1 3 1 3 1 |
| `Chromatic` | Chromatic | all 12 semitones |
| `Custom` | User-defined | user-supplied `degrees[12]` |

---

## Scale Structure

```cpp
struct Scale {
    std::string        scaleId;
    std::string        name;
    ScaleMode          mode;
    uint8_t            root;      // 0=C, 1=C#, …, 11=B
    std::array<bool,12> degrees;  // which semitone classes are in-scale
};
```

`degrees` is indexed by `note % 12`, regardless of octave.

---

## makeScale

```cpp
Scale s = ScaleEngine::makeScale(ScaleMode::Dorian, 2 /* D */);
```

Internally, each mode is stored as a C-rooted degree table. `makeScale` rotates the table by `root` positions so that `degrees[root % 12] == true` for the tonic.

---

## quantize

```cpp
uint8_t q = ScaleEngine::quantize(note, scale);
```

Searches outward from `note` for the nearest in-scale semitone. The search alternates down and up:

```
d=1: try note-1, try note+1
d=2: try note-2, try note+2
...
```

Returns the first in-scale match. If the note is already in the scale it is returned unchanged.

---

## Instance API

```cpp
ScaleEngine scales;

// Add (custom or pre-built).
scales.addScale(s);

// Look up by ID — returns nullptr if not found.
const Scale* found = scales.find("my_scale");

// Remove by ID.
scales.removeScale("my_scale");

// Iterate all scales.
for (const Scale& s : scales.allScales()) { … }
```

---

## WebSocket Commands

| Message | Key fields | Notes |
|---|---|---|
| `list_scales` | — | Returns all scales (built-in + custom) as JSON array |
| `add_scale` | `scale_id`, `name`, `root`, `degrees[12]` | Adds custom scale, persists to DB |
| `remove_scale` | `scale_id` | Removes custom scale, deletes from DB |

Response format for `list_scales`:

```json
{
  "type": "scales_list",
  "scales": [
    {
      "scale_id": "my_blues",
      "name": "My Blues",
      "mode": 16,
      "root": 0,
      "degrees": [true,false,false,true,false,true,true,true,false,false,true,false]
    }
  ]
}
```

---

## UI Integration

- **Utility panel → Scale section** — root note select + mode select + 12 interactive degree dots. Click a dot to toggle individual semitones for fully custom scales.
- The scale selector in both the Piano Roll and LFO editor is populated from the `scales_list` event.
- In the piano roll, `degrees[noteClass]` controls row background colour and keyboard key colour.
