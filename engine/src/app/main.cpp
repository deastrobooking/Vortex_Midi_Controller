#include "../core/ClockEngine.h"
#include "../core/MidiRouter.h"
#include "../core/SnapshotEngine.h"
#include "../core/AutomationRecorder.h"
#include "../core/AutomationPlayback.h"
#include "../core/MacroEngine.h"
#include "../core/ScaleEngine.h"
#include "../core/LfoEngine.h"
#include "../core/SequencerEngine.h"
#include "../core/ModMatrix.h"
#include "../core/ArpEngine.h"
#include "../core/ArrangeEngine.h"
#include "../hardware/ModuleManager.h"
#include "../midi/MidiOutput.h"
#include "../midi/MidiInput.h"
#include "../database/Database.h"
#include "../api/WebSocketServer.h"
#include "../api/MessageHandler.h"
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <atomic>
#include <nlohmann/json.hpp>

static std::atomic<bool> g_running{true};
static void handleSignal(int) { g_running = false; }

// ─── JSON helpers for LFO / mod-route round-trips ────────────────────────────

using json = nlohmann::json;

static LfoConfig lfoFromJson(const json& j) {
    LfoConfig cfg;
    cfg.lfoId        = j.value("lfo_id","");
    cfg.name         = j.value("name","LFO");
    cfg.shape        = static_cast<LfoShapeType>(j.value("shape",0));
    cfg.syncToTempo  = j.value("sync_to_tempo",true);
    cfg.rateHz       = j.value("rate_hz",1.f);
    cfg.syncDivision = j.value("sync_division", kTicksPerBar4_4);
    cfg.depth        = j.value("depth",1.f);
    cfg.phaseOffset  = j.value("phase_offset",0.f);
    cfg.bipolar      = j.value("bipolar",true);
    for (auto& pt : j.value("custom_points", json::array()))
        cfg.customPoints.push_back({pt.value("phase",0.f), pt.value("value",0.f)});
    return cfg;
}

static ModRoute modRouteFromJson(const json& j) {
    ModRoute r;
    r.routeId  = j.value("route_id","");
    r.source   = static_cast<ModSource>(j.value("source",0));
    r.sourceId = j.value("source_id","");
    r.dest     = static_cast<ModDest>(j.value("dest",0));
    r.destId   = j.value("dest_id","");
    r.amount   = j.value("amount",1.f);
    r.offset   = j.value("offset",0.f);
    r.enabled  = j.value("enabled",true);
    return r;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT,  handleSignal);
    std::signal(SIGTERM, handleSignal);

    // ─── Config ──────────────────────────────────────────────────────────
    std::string dbPath     = "/var/lib/vortex/vortex.db";
    std::string schemaPath = "/usr/share/vortex/schema.sql";
    uint16_t    wsPort     = 8080;
    double      defaultBpm = 120.0;

    if (argc > 1) dbPath     = argv[1];
    if (argc > 2) schemaPath = argv[2];

    std::filesystem::create_directories(
        std::filesystem::path(dbPath).parent_path());

    // ─── Database ─────────────────────────────────────────────────────────
    Database db;
    if (!db.open(dbPath)) {
        std::cerr << "Failed to open database: " << dbPath << "\n";
        return 1;
    }
    if (!db.applySchema(schemaPath))
        std::cerr << "Warning: schema not applied (path: " << schemaPath << ")\n";

    // ─── Core engine subsystems ───────────────────────────────────────────
    ClockEngine         clock;
    MidiRouter          router;
    SnapshotEngine      snapshots;
    AutomationRecorder  recorder;
    AutomationPlayback  playback;
    MacroEngine         macros;
    ScaleEngine         scales;
    LfoEngine           lfos;
    SequencerEngine     sequencer(MidiOutput::instance(), scales);
    ModMatrix           modMatrix(lfos, sequencer, router);
    vortex::ArpEngine   arp(scales);
    vortex::ArrangeEngine arrange;

    // ─── MIDI output ──────────────────────────────────────────────────────
    MidiOutput::instance().openAll();

    // ─── Load last project ────────────────────────────────────────────────
    int projectId = std::stoi(db.getSetting("last_project_id", "1"));
    auto project  = db.loadProject(projectId);

    if (project.id > 0) {
        clock.setTempo(project.tempo);

        std::string mapJson = db.loadMappingJson(projectId);
        if (!mapJson.empty()) router.loadMappingJson(mapJson);

        for (auto& clip  : db.loadClips(projectId))   playback.loadClip(std::move(clip));
        for (auto& scene : db.loadScenes(projectId))   snapshots.addScene(std::move(scene));

        // Load LFOs.
        for (auto& [lfoId, jstr] : db.loadLfos(projectId)) {
            auto j = json::parse(jstr, nullptr, false);
            if (!j.is_discarded()) lfos.addLfo(lfoFromJson(j));
        }

        // Load mod routes.
        for (auto& [routeId, jstr] : db.loadModRoutes(projectId)) {
            auto j = json::parse(jstr, nullptr, false);
            if (!j.is_discarded()) modMatrix.addRoute(modRouteFromJson(j));
        }

        // Load custom scales.
        for (auto& [scaleId, jstr] : db.loadCustomScales(projectId)) {
            auto j = json::parse(jstr, nullptr, false);
            if (!j.is_discarded()) {
                Scale s;
                s.scaleId = j.value("scale_id","");
                s.name    = j.value("name","");
                s.root    = j.value("root",0);
                s.mode    = ScaleMode::Custom;
                auto deg  = j.value("degrees", std::vector<bool>(12,false));
                for (int i = 0; i < 12 && i < (int)deg.size(); ++i) s.degrees[i] = deg[i];
                scales.addScale(s);
            }
        }

        // Load first sequencer pattern (if any).
        auto patternIds = db.loadPatternIds(projectId);
        if (!patternIds.empty()) {
            std::string raw = db.loadPattern(patternIds[0]);
            // Minimal deserialisation — full logic lives in MessageHandler::handleLoadPattern.
            auto pj = json::parse(raw, nullptr, false);
            if (!pj.is_discarded()) {
                std::cout << "  Loaded pattern: " << pj.value("name","") << "\n";
            }
        }
    } else {
        clock.setTempo(defaultBpm);
    }

    // ─── WebSocket API ─────────────────────────────────────────────────────
    WebSocketServer ws;
    MessageHandler  handler(clock, router, snapshots, recorder,
                            playback, macros,
                            sequencer, lfos, modMatrix, scales, db,
                            arp, arrange);

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

    // ─── Hardware modules ──────────────────────────────────────────────────
    ModuleManager modules;
    for (uint8_t i = 0; i < 4; ++i) {
        std::string key  = "module_" + std::to_string(i) + "_port";
        std::string port = db.getSetting(key, "");
        if (!port.empty() && modules.connectModule(i, port))
            std::cout << "  Module " << (int)i << ": " << port << "\n";
    }

    // ─── Wire hardware → router / recorder / snapshot ─────────────────────
    modules.onEvent([&](const HardwareEvent& hw) {
        std::string controlId =
            "m" + std::to_string(hw.moduleId) + "_" +
            [&]() -> std::string {
                switch (hw.type) {
                    case HWEventType::FaderMove:     return "fader_"   + std::to_string(hw.controlId);
                    case HWEventType::EncoderDelta:  return "encoder_" + std::to_string(hw.controlId);
                    case HWEventType::ButtonPress:
                    case HWEventType::ButtonRelease: return "btn_"     + std::to_string(hw.controlId);
                    default:                         return "ctrl_"    + std::to_string(hw.controlId);
                }
            }();

        ControlEvent evt{controlId, hw.value, clock.currentTick()};

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
        ws.broadcast(handler.buildControlValues());
    });

    // ─── Wire MIDI input → clock (external sync) + mod matrix ─────────────
    MidiInput::instance().onMessage([&](const MidiInputMessage& msg) {
        uint8_t type = msg.status & 0xF0;
        // Realtime clock messages.
        if (msg.status == 0xF8)  { clock.receiveMidiClock();    return; }
        if (msg.status == 0xFA)  { clock.receiveMidiStart();    return; }
        if (msg.status == 0xFC)  { clock.receiveMidiStop();     return; }
        if (msg.status == 0xFB)  { clock.receiveMidiContinue(); return; }

        // CC → mod matrix source feed.
        if (type == 0xB0) {
            if (msg.data1 == 1)  modMatrix.feedModWheel(msg.data2);
            else                 modMatrix.feedCC(msg.data1, msg.data2);
        }
        // Note-on velocity source.
        if (type == 0x90 && msg.data2 > 0) modMatrix.feedVelocity(msg.data2);
        // Channel pressure (aftertouch).
        if (type == 0xD0)                  modMatrix.feedAftertouch(msg.data1);
        // Pitch bend.
        if (type == 0xE0) {
            int16_t pb = static_cast<int16_t>((msg.data2 << 7) | msg.data1) - 8192;
            modMatrix.feedPitchBend(pb);
        }
    });

    // Open MIDI inputs listed in settings.
    for (int i = 0; i < 4; ++i) {
        std::string key  = "midi_input_" + std::to_string(i) + "_port_id";
        std::string pidStr = db.getSetting(key, "");
        if (!pidStr.empty()) MidiInput::instance().open(std::stoi(pidStr));
    }

    // ─── Wire clock → everything tick-driven ──────────────────────────────
    uint64_t prevTick = 0;

    clock.onTick([&](uint64_t tick) {
        // Automation playback.
        playback.advance(prevTick, tick);
        // Scene morph.
        snapshots.tick(tick);
        // LFO advance.
        lfos.advance(tick);
        // Mod matrix (reads LFO values, dispatches to seq/router).
        modMatrix.process(tick);
        // Sequencer.
        sequencer.advance(prevTick, tick);

        // Arrange: fire bar-boundary callbacks.
        arrange.advanceBar(static_cast<uint32_t>(clock.currentBar()));

        // MIDI clock output (every tick = 960ppqn; MIDI clock = 24ppqn).
        // Send a MIDI clock byte every 40 ticks (960 / 24 = 40).
        if (tick % 40 == 0) {
            for (const auto& p : MidiOutput::instance().listPorts())
                if (p.open) MidiOutput::instance().sendClock(p.id);
        }

        prevTick = tick;
    });

    // Route automation playback events.
    playback.onEvent([&](const ControlEvent& evt) {
        router.route(evt);
        snapshots.updateValue(evt.controlId, evt.value);
    });

    // Route snapshot morph values to MIDI.
    snapshots.onValueChange([&](const std::string& controlId, int value) {
        router.route({controlId, value, clock.currentTick()});
    });

    // Sequencer step feedback → UI broadcast.
    sequencer.onStep([&](uint8_t, uint8_t) {
        ws.broadcast(handler.buildSequencerState());
    });

    // Snapshot morph from mod matrix.
    modMatrix.onMorph([&](const std::string& destId, float position) {
        // destId = "sceneA:sceneB" — morph controller not yet wired to SnapshotEngine.
        (void)destId; (void)position;
    });

    // Start transport.
    clock.play();

    // ─── Main loop ────────────────────────────────────────────────────────
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Auto-save mapping + settings every 10 s.
        static int saveCounter = 0;
        if (++saveCounter >= 100 && project.id > 0) {
            db.saveMappingJson(projectId, router.saveMappingJson());
            db.setSetting("last_project_id", std::to_string(projectId));
            saveCounter = 0;
        }
    }

    // ─── Graceful shutdown ────────────────────────────────────────────────
    clock.stop();
    sequencer.reset();
    ws.stop();
    MidiOutput::instance().closeAll();
    MidiInput::instance().closeAll();

    if (project.id > 0) {
        db.saveMappingJson(projectId, router.saveMappingJson());
        db.setSetting("last_project_id", std::to_string(projectId));
    }

    std::cout << "Vortex daemon stopped.\n";
    return 0;
}
