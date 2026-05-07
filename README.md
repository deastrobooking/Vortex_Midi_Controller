# Vortex MIDI Controller

A modular, open-source MIDI workstation built around a Raspberry Pi engine daemon and RP2040-based hardware channel modules. Vortex combines a 64-track polymetric step/melodic sequencer, a 21-shape LFO engine, a flexible modulation matrix, and a browser-based GUI — all communicating in real time over WebSocket.

---

## Feature Summary

| Layer | What it does |
|---|---|
| **64-track Sequencer** | Polymetric step sequencer (up to 64 steps/track), melodic piano-roll mode, per-track swing, scale quantization, mute/solo |
| **Melodic / Piano Roll** | Note grid with scale highlighting; click to paint notes; snap-to-scale quantization |
| **LFO Engine** | 21 built-in shapes (sine, triangle, sawtooth, square, S&H, syncopated 1/8 & 1/16, dotted groove, shuffle, custom draw) |
| **Modulation Matrix** | Route any source (LFO, velocity, aftertouch, mod wheel, pitch bend, MIDI CC) to any destination (MIDI CC, seq pitch/velocity/gate/rate, LFO rate/depth, scene morph) |
| **Arpeggiator** | Per-track: Up / Down / UpDown / Random / AsPlayed modes, 1–4 octave range, latch, gate % |
| **Arrangement Timeline** | Bar-based clip painter for sequencing pattern changes across tracks |
| **Scene Snapshots** | Capture and recall full control states; morph between scenes |
| **Automation** | Per-control automation lanes with loop and interpolation |
| **Macro Engine** | Fan-out one physical control to multiple MIDI targets |
| **Scale Engine** | 16 built-in modes (Major → Chromatic) + custom user-defined scales |
| **MIDI I/O** | ALSA RawMidi output + multi-port input with device discovery and live CC/clock routing |
| **Hardware Modules** | RP2040 channel modules: 16 faders, 16 encoders, 32 buttons, WS2812B LEDs, OLED displays |
| **Browser GUI** | Vanilla JS ES6 app — no build step required, connects over WebSocket |

---

## Repository Layout

```
Vortex_Midi_Controller/
├── engine/               # C++20 daemon (midiworkstationd)
│   ├── src/
│   │   ├── app/          # main.cpp — wires all subsystems
│   │   ├── api/          # WebSocket server + JSON message handler
│   │   ├── core/         # Sequencer, LFO, Mod Matrix, Scale, Arp, Arrange…
│   │   ├── database/     # SQLite via Database.h
│   │   ├── hardware/     # ModuleManager (UART ↔ RP2040)
│   │   └── midi/         # MidiOutput + MidiInput (ALSA RawMidi)
│   ├── tests/            # Google Test suites
│   └── CMakeLists.txt
├── firmware/
│   └── rp2040-channel-module/   # Pico C SDK firmware
├── database/
│   └── schema.sql        # SQLite schema (applied on first run)
├── ui/                   # Browser front-end
│   ├── index.html
│   ├── css/style.css
│   └── js/               # ES6 modules — no bundler needed
└── docs/                 # Developer documentation
```

---

## Requirements

### Engine daemon

| Dependency | Min version | Install |
|---|---|---|
| GCC or Clang | C++20 | `apt install build-essential` |
| CMake | 3.20 | `apt install cmake` |
| ALSA | any | `apt install libasound2-dev` |
| libwebsockets | 4.x | `apt install libwebsockets-dev` |
| SQLite 3 | 3.x | `apt install libsqlite3-dev` |
| nlohmann/json | 3.11 (auto-fetched) | FetchContent, no manual step |
| Google Test | 1.14 (auto-fetched) | FetchContent, only needed for tests |

### Firmware

| Dependency | Notes |
|---|---|
| Raspberry Pi Pico SDK | Set `PICO_SDK_PATH` in environment |
| CMake ≥ 3.20 | |
| arm-none-eabi-gcc | `apt install gcc-arm-none-eabi` |

### UI

No build step. Open `ui/index.html` in a browser or serve from a local HTTP server.

---

## Building

### Engine daemon

```bash
cd engine
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The binary is at `engine/build/midiworkstationd`.

Build with tests:

```bash
cmake -B build -DBUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### Firmware

```bash
cd firmware/rp2040-channel-module
cmake -B build -DPICO_SDK_PATH=$PICO_SDK_PATH
cmake --build build --parallel
```

Flash `build/rp2040_channel_module.uf2` to the Pico via USB mass-storage boot mode.

---

## Running

### 1. Start the engine daemon

```bash
# Default paths
sudo midiworkstationd

# Custom DB and schema paths
midiworkstationd /path/to/vortex.db /path/to/schema.sql
```

The daemon:
- Applies the schema on first run (`database/schema.sql`)
- Listens for WebSocket connections on **port 8080**
- Auto-opens hardware serial ports stored in settings
- Outputs MIDI clock to all open ALSA output ports

### 2. Open the UI

```bash
# Simple HTTP server (Python)
cd ui
python3 -m http.server 3000
```

Then open [http://localhost:3000](http://localhost:3000) in a browser. The UI auto-connects to `ws://localhost:8080`.

For direct hardware use without a server, open `ui/index.html` directly (`file://` works if the engine is on the same host).

### 3. Connect MIDI devices

In the UI — **Utility panel → MIDI I/O → Discover** — then enable the ports you want. Alternatively configure port IDs in the database settings table:

```sql
INSERT OR REPLACE INTO settings (key, value)
VALUES ('midi_input_0_port_id', '1');
```

---

## Configuration

All runtime configuration is stored in the SQLite database under the `settings` table. Common keys:

| Key | Example | Description |
|---|---|---|
| `last_project_id` | `1` | Active project on startup |
| `module_0_port` | `/dev/ttyUSB0` | RP2040 module serial port |
| `module_1_port` | `/dev/ttyUSB1` | Additional module port |
| `midi_input_0_port_id` | `1` | ALSA input port auto-opened on startup |

---

## Development Quick Reference

| Task | Command |
|---|---|
| Rebuild engine | `cmake --build engine/build --parallel` |
| Run all tests | `ctest --test-dir engine/build` |
| Run one test suite | `./engine/build/tests/test_scale_engine` |
| Watch UI changes | Edit `ui/js/*.js` — reload browser |
| Apply schema changes | Bump schema, delete DB file, restart daemon |
| Check WebSocket messages | Open browser DevTools → Network → WS |

---

## Docs

Developer documentation lives in [docs/](docs/):

- [docs/sequencer.md](docs/sequencer.md) — Step & melodic sequencer internals
- [docs/lfo-engine.md](docs/lfo-engine.md) — LFO shapes, evaluation, custom draw
- [docs/mod-matrix.md](docs/mod-matrix.md) — Modulation routing architecture
- [docs/scale-engine.md](docs/scale-engine.md) — Scale modes, quantization, custom scales
- [docs/arp-engine.md](docs/arp-engine.md) — Arpeggiator modes and clock integration
- [docs/arrange-engine.md](docs/arrange-engine.md) — Arrangement timeline and pattern switching
- [docs/hardware-protocol.md](docs/hardware-protocol.md) — RP2040 UART binary protocol
- [docs/websocket-api.md](docs/websocket-api.md) — Full WebSocket message reference
- [docs/database-schema.md](docs/database-schema.md) — SQLite schema and blob formats

---

## License

MIT
