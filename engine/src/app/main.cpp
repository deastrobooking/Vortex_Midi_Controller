#include "../core/ClockEngine.h"
#include "../core/MidiRouter.h"
#include "../core/SnapshotEngine.h"
#include "../core/AutomationRecorder.h"
#include "../core/AutomationPlayback.h"
#include "../core/MacroEngine.h"
#include "../hardware/ModuleManager.h"
#include "../midi/MidiOutput.h"
#include "../database/Database.h"
#include "../api/WebSocketServer.h"
#include "../api/MessageHandler.h"
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <atomic>

static std::atomic<bool> g_running{true};

static void handleSignal(int) { g_running = false; }

int main(int argc, char* argv[]) {
    std::signal(SIGINT,  handleSignal);
    std::signal(SIGTERM, handleSignal);

    // ─── Config defaults (override via env / config file later) ─────────────
    std::string dbPath     = "/var/lib/vortex/vortex.db";
    std::string schemaPath = "/usr/share/vortex/schema.sql";
    uint16_t    wsPort     = 8080;
    double      defaultBpm = 120.0;

    // Allow overriding DB path from command line.
    if (argc > 1) dbPath = argv[1];

    std::filesystem::create_directories(
        std::filesystem::path(dbPath).parent_path());

    // ─── Database ─────────────────────────────────────────────────────────
    Database db;
    if (!db.open(dbPath)) {
        std::cerr << "Failed to open database: " << dbPath << "\n";
        return 1;
    }
    db.applySchema(schemaPath);

    // ─── Engine subsystems ────────────────────────────────────────────────
    ClockEngine         clock;
    MidiRouter          router;
    SnapshotEngine      snapshots;
    AutomationRecorder  recorder;
    AutomationPlayback  playback;
    MacroEngine         macros;
    ModuleManager       modules;

    // ─── MIDI output ──────────────────────────────────────────────────────
    MidiOutput::instance().openAll();

    // ─── Load last project ────────────────────────────────────────────────
    int projectId = std::stoi(db.getSetting("last_project_id", "1"));
    auto project  = db.loadProject(projectId);

    if (project.id > 0) {
        clock.setTempo(project.tempo);
        std::string mapJson = db.loadMappingJson(projectId);
        if (!mapJson.empty()) router.loadMappingJson(mapJson);

        for (auto& clip : db.loadClips(projectId))
            playback.loadClip(std::move(clip));

        for (auto& scene : db.loadScenes(projectId))
            snapshots.addScene(std::move(scene));
    } else {
        clock.setTempo(defaultBpm);
    }

    // ─── WebSocket API ────────────────────────────────────────────────────
    WebSocketServer ws;
    MessageHandler  handler(clock, router, snapshots, recorder,
                            playback, macros, db);

    ws.onMessage([&](const std::string& msg) {
        std::string reply = handler.handle(msg);
        if (!reply.empty()) ws.broadcast(reply);
    });

    if (!ws.listen(wsPort)) {
        std::cerr << "Failed to start WebSocket server on port " << wsPort << "\n";
        return 1;
    }

    std::cout << "Vortex MIDI Workstation daemon started.\n";
    std::cout << "  DB:        " << dbPath  << "\n";
    std::cout << "  WebSocket: ws://localhost:" << wsPort << "\n";
    std::cout << "  Project:   " << (project.id > 0 ? project.name : "(none)") << "\n";

    // ─── Hardware modules ────────────────────────────────────────────────
    // Module serial ports can be listed in settings: "module_0_port", etc.
    for (uint8_t i = 0; i < 4; ++i) {
        std::string key  = "module_" + std::to_string(i) + "_port";
        std::string port = db.getSetting(key, "");
        if (!port.empty()) {
            if (modules.connectModule(i, port))
                std::cout << "  Module " << (int)i << ": " << port << "\n";
        }
    }

    // ─── Wire hardware events → router / recorder / snapshot ─────────────
    modules.onEvent([&](const HardwareEvent& hw) {
        // Build a global control ID from module + control.
        std::string controlId =
            "m" + std::to_string(hw.moduleId) +
            "_" + [&]() -> std::string {
                switch (hw.type) {
                    case HWEventType::FaderMove:    return "fader_"   + std::to_string(hw.controlId);
                    case HWEventType::EncoderDelta: return "encoder_" + std::to_string(hw.controlId);
                    case HWEventType::ButtonPress:
                    case HWEventType::ButtonRelease: return "btn_"   + std::to_string(hw.controlId);
                    default: return "ctrl_" + std::to_string(hw.controlId);
                }
            }();

        ControlEvent evt;
        evt.controlId      = controlId;
        evt.value          = hw.value;
        evt.timestampTicks = clock.currentTick();

        // Macros first, then direct routing.
        if (macros.hasMacro(controlId)) {
            macros.process(evt, [&](const std::string& id, int val) {
                ControlEvent derived{id, val, evt.timestampTicks};
                router.route(derived);
                recorder.feed(derived);
                snapshots.updateValue(id, val);
            });
        } else {
            router.route(evt);
        }

        recorder.feed(evt);
        snapshots.updateValue(controlId, hw.value);

        // Push live value updates to UI.
        ws.broadcast(handler.buildControlValues());
    });

    // ─── Wire clock → automation playback ────────────────────────────────
    uint64_t prevTick = 0;
    clock.onTick([&](uint64_t tick) {
        playback.advance(prevTick, tick);
        snapshots.tick(tick);
        prevTick = tick;
    });

    // Route automation playback events to MIDI.
    playback.onEvent([&](const ControlEvent& evt) {
        router.route(evt);
        snapshots.updateValue(evt.controlId, evt.value);
    });

    // Route snapshot morph values to MIDI.
    snapshots.onValueChange([&](const std::string& controlId, int value) {
        ControlEvent evt{controlId, value, clock.currentTick()};
        router.route(evt);
    });

    // Start transport.
    clock.play();

    // ─── Main loop ────────────────────────────────────────────────────────
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Persist mapping every 10 s in background.
        static int saveCounter = 0;
        if (++saveCounter >= 100 && project.id > 0) {
            db.saveMappingJson(projectId, router.saveMappingJson());
            db.setSetting("last_project_id", std::to_string(projectId));
            saveCounter = 0;
        }
    }

    // ─── Graceful shutdown ────────────────────────────────────────────────
    clock.stop();
    ws.stop();
    MidiOutput::instance().closeAll();
    db.saveMappingJson(projectId, router.saveMappingJson());
    db.setSetting("last_project_id", std::to_string(projectId));

    std::cout << "Vortex daemon stopped.\n";
    return 0;
}
