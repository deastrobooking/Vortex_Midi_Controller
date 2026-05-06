#include "MessageHandler.h"
#include "../core/ClockEngine.h"
#include "../core/MidiRouter.h"
#include "../core/SnapshotEngine.h"
#include "../core/AutomationRecorder.h"
#include "../core/AutomationPlayback.h"
#include "../core/MacroEngine.h"
#include "../core/SequencerEngine.h"
#include "../core/LfoEngine.h"
#include "../core/LfoShape.h"
#include "../core/ModMatrix.h"
#include "../core/ScaleEngine.h"
#include "../core/ArpEngine.h"
#include "../core/ArrangeEngine.h"
#include "../midi/MidiInput.h"
#include "../database/Database.h"
#include <nlohmann/json.hpp>
#include <algorithm>

using json = nlohmann::json;

// ─── Construction ────────────────────────────────────────────────────────────

MessageHandler::MessageHandler(ClockEngine&              clock,
                               MidiRouter&               router,
                               SnapshotEngine&           snapshots,
                               AutomationRecorder&       recorder,
                               AutomationPlayback&       playback,
                               MacroEngine&              macros,
                               SequencerEngine&          sequencer,
                               LfoEngine&                lfos,
                               ModMatrix&                modMatrix,
                               ScaleEngine&              scales,
                               Database&                 db,
                               vortex::ArpEngine&        arp,
                               vortex::ArrangeEngine&    arrange)
    : m_clock(clock), m_router(router), m_snapshots(snapshots),
      m_recorder(recorder), m_playback(playback), m_macros(macros),
      m_sequencer(sequencer), m_lfos(lfos), m_modMatrix(modMatrix),
      m_scales(scales), m_db(db), m_arp(arp), m_arrange(arrange)
{}

// ─── Dispatch ────────────────────────────────────────────────────────────────

std::string MessageHandler::handle(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded() || !doc.contains("type")) return {};
    const std::string type = doc["type"];

    // ── Core ───────────────────────────────────────────────────────────────
    if (type == "get_project_state")  return handleGetProjectState();
    if (type == "update_mapping")     return handleUpdateMapping(jsonStr);
    if (type == "trigger_scene")      return handleTriggerScene(jsonStr);
    if (type == "set_tempo")          return handleSetTempo(jsonStr);
    if (type == "transport")          return handleTransport(jsonStr);
    if (type == "save_scene")         return handleSaveScene(jsonStr);
    if (type == "start_record")       return handleStartRecord(jsonStr);
    if (type == "stop_record")        return handleStopRecord(jsonStr);
    if (type == "list_scenes")        return handleListScenes();
    if (type == "list_projects")      return handleListProjects();
    if (type == "add_macro")          return handleAddMacro(jsonStr);
    if (type == "remove_macro")       return handleRemoveMacro(jsonStr);
    if (type == "list_macros")        return handleListMacros();

    // ── Sequencer ─────────────────────────────────────────────────────────
    if (type == "get_sequencer_state") return handleGetSequencerState();
    if (type == "set_track")           return handleSetTrack(jsonStr);
    if (type == "set_step")            return handleSetStep(jsonStr);
    if (type == "clear_track")         return handleClearTrack(jsonStr);
    if (type == "mute_track")          return handleMuteTrack(jsonStr);
    if (type == "solo_track")          return handleSoloTrack(jsonStr);
    if (type == "save_pattern")        return handleSavePattern(jsonStr);
    if (type == "load_pattern")        return handleLoadPattern(jsonStr);
    if (type == "list_patterns")       return handleListPatterns(jsonStr);

    // ── LFO ───────────────────────────────────────────────────────────────
    if (type == "add_lfo")            return handleAddLfo(jsonStr);
    if (type == "update_lfo")         return handleUpdateLfo(jsonStr);
    if (type == "remove_lfo")         return handleRemoveLfo(jsonStr);
    if (type == "list_lfos")          return handleListLfos();
    if (type == "get_lfo_preview")    return handleGetLfoPreview(jsonStr);

    // ── Mod matrix ────────────────────────────────────────────────────────
    if (type == "add_mod_route")      return handleAddModRoute(jsonStr);
    if (type == "update_mod_route")   return handleUpdateModRoute(jsonStr);
    if (type == "remove_mod_route")   return handleRemoveModRoute(jsonStr);
    if (type == "list_mod_routes")    return handleListModRoutes();

    // ── Scales ────────────────────────────────────────────────────────────
    if (type == "list_scales")        return handleListScales();
    if (type == "add_scale")          return handleAddScale(jsonStr);
    if (type == "remove_scale")       return handleRemoveScale(jsonStr);

    // ── MIDI input ────────────────────────────────────────────────────────
    if (type == "discover_midi_inputs") return handleDiscoverMidiInputs();
    if (type == "open_midi_input")      return handleOpenMidiInput(jsonStr);
    if (type == "close_midi_input")     return handleCloseMidiInput(jsonStr);

    // ── Arp ───────────────────────────────────────────────────────────────
    if (type == "set_arp")              return handleSetArp(jsonStr);
    if (type == "get_arp")              return handleGetArp(jsonStr);

    // ── Arrange ───────────────────────────────────────────────────────────
    if (type == "arrange_set_clip")     return handleArrangeSetClip(jsonStr);
    if (type == "arrange_remove_clip")  return handleArrangeRemoveClip(jsonStr);
    if (type == "arrange_get_clips")    return handleArrangeGetClips();

    return json{{"type","error"},{"message","unknown message type"}}.dump();
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

int MessageHandler::currentProjectId() const {
    return std::stoi(m_db.getSetting("last_project_id", "1"));
}

// ─── Core commands (retained from original) ───────────────────────────────────

std::string MessageHandler::handleGetProjectState() {
    json res;
    res["type"]         = "project_state";
    res["tempo"]        = m_clock.getTempo();
    res["playing"]      = (m_clock.transportState() == TransportState::Playing);
    res["current_tick"] = m_clock.currentTick();
    res["current_beat"] = m_clock.currentBeat();
    res["current_bar"]  = m_clock.currentBar();
    res["scene_ids"]    = m_snapshots.sceneIds();
    return res.dump();
}

std::string MessageHandler::handleUpdateMapping(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    std::string controlId = doc["control_id"];
    auto& jt = doc["target"];
    MidiTarget t;
    t.portId   = jt.value("port_id", 0);
    t.channel  = jt.value("channel", 1);
    t.ccNumber = jt.value("cc",      0);
    t.minValue = jt.value("min",     0);
    t.maxValue = jt.value("max",   127);
    t.curve    = jt.value("curve", "linear");
    t.invert   = jt.value("invert", false);
    m_router.removeTargets(controlId);
    m_router.setTarget(controlId, t);
    return json{{"type","mapping_updated"},{"control_id",controlId}}.dump();
}

std::string MessageHandler::handleTriggerScene(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    std::string sceneId  = doc["scene_id"];
    std::string modeStr  = doc.value("mode","jump");
    uint32_t durationBars = doc.value("duration_bars",4);
    SceneRecallMode mode = SceneRecallMode::Jump;
    if (modeStr == "fade")  mode = SceneRecallMode::Fade;
    if (modeStr == "drop")  mode = SceneRecallMode::Drop;
    if (modeStr == "morph") mode = SceneRecallMode::Morph;
    m_snapshots.recallScene(sceneId, mode, durationBars * kTicksPerBar4_4);
    return json{{"type","scene_triggered"},{"scene_id",sceneId}}.dump();
}

std::string MessageHandler::handleSetTempo(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    m_clock.setTempo(doc.value("tempo",120.0));
    return json{{"type","tempo_set"},{"tempo",m_clock.getTempo()}}.dump();
}

std::string MessageHandler::handleTransport(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    std::string action = doc.value("action","stop");
    if      (action == "play")     m_clock.play();
    else if (action == "stop")     m_clock.stop();
    else if (action == "continue") m_clock.continueClock();
    else if (action == "tap")      m_clock.tapTempo();
    return json{{"type","transport_ack"},{"action",action}}.dump();
}

std::string MessageHandler::handleSaveScene(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    std::string sceneId = doc.value("scene_id","");
    std::string name    = doc.value("name","Untitled");
    if (sceneId.empty())
        return json{{"type","error"},{"message","scene_id required"}}.dump();
    SceneValues sv = m_snapshots.captureScene(sceneId, name);
    m_snapshots.addScene(sv);
    m_db.saveScene(currentProjectId(), sv);
    return json{{"type","scene_saved"},{"scene_id",sceneId}}.dump();
}

std::string MessageHandler::handleStartRecord(const std::string&) {
    m_recorder.startRecord(m_clock.currentTick());
    return json{{"type","record_started"}}.dump();
}

std::string MessageHandler::handleStopRecord(const std::string&) {
    m_recorder.stopRecord(m_clock.currentTick());
    return json{{"type","record_stopped"}}.dump();
}

std::string MessageHandler::handleListScenes() {
    return json{{"type","scenes_list"},{"scenes",m_snapshots.sceneIds()}}.dump();
}

std::string MessageHandler::handleListProjects() {
    auto projects = m_db.listProjects();
    json arr = json::array();
    for (const auto& p : projects)
        arr.push_back({{"id",p.id},{"name",p.name},{"tempo",p.tempo}});
    return json{{"type","projects_list"},{"projects",arr}}.dump();
}

std::string MessageHandler::handleAddMacro(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    Macro m;
    m.macroId         = doc.value("macro_id","");
    m.name            = doc.value("name","");
    m.sourceControlId = doc.value("source_control_id","");
    for (auto& jt : doc.value("targets", json::array())) {
        MacroTarget mt;
        mt.midi.portId   = jt.value("port_id",0);
        mt.midi.channel  = jt.value("channel",1);
        mt.midi.ccNumber = jt.value("cc",0);
        mt.midi.minValue = jt.value("min",0);
        mt.midi.maxValue = jt.value("max",127);
        mt.scaleFactor   = jt.value("scale",1.f);
        m.targets.push_back(mt);
    }
    m_macros.addMacro(std::move(m));
    return json{{"type","macro_added"}}.dump();
}

std::string MessageHandler::handleRemoveMacro(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    m_macros.removeMacro(doc.value("macro_id",""));
    return json{{"type","macro_removed"}}.dump();
}

std::string MessageHandler::handleListMacros() {
    json arr = json::array();
    for (const auto& m : m_macros.allMacros())
        arr.push_back({{"macro_id",m.macroId},{"name",m.name},
                       {"source",m.sourceControlId}});
    return json{{"type","macros_list"},{"macros",arr}}.dump();
}

// ─── Sequencer commands ───────────────────────────────────────────────────────

std::string MessageHandler::handleGetSequencerState() {
    const auto& pat   = m_sequencer.pattern();
    auto        steps = m_sequencer.currentSteps();
    json res;
    res["type"]        = "sequencer_state";
    res["pattern_id"]  = pat.patternId;
    res["pattern_name"]= pat.name;
    res["current_steps"]= steps;
    json tracks = json::array();
    for (const auto& t : pat.tracks) {
        tracks.push_back({
            {"track_id",    t.trackId},
            {"name",        t.name},
            {"midi_port",   t.midiPort},
            {"midi_channel",t.midiChannel},
            {"base_note",   t.baseNote},
            {"melodic",     t.melodic},
            {"step_count",  t.stepCount},
            {"step_ticks",  t.stepTicks},
            {"muted",       t.muted},
            {"transpose",   t.transpose},
            {"swing",       t.swing},
            {"use_scale",   t.useScale},
            {"scale_id",    t.scaleId}
        });
    }
    res["tracks"] = tracks;
    return res.dump();
}

std::string MessageHandler::handleSetTrack(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    SeqTrack t;
    t.trackId    = static_cast<uint8_t>(doc.value("track_id",0));
    t.name       = doc.value("name", "Track " + std::to_string(t.trackId+1));
    t.midiPort   = static_cast<uint8_t>(doc.value("midi_port",0));
    t.midiChannel= static_cast<uint8_t>(doc.value("midi_channel",1));
    t.baseNote   = static_cast<uint8_t>(doc.value("base_note",60));
    t.melodic    = doc.value("melodic",false);
    t.stepCount  = static_cast<uint8_t>(doc.value("step_count",16));
    t.stepTicks  = doc.value("step_ticks", kTicksPerSixteen);
    t.muted      = doc.value("muted",false);
    t.transpose  = static_cast<int8_t>(doc.value("transpose",0));
    t.swing      = doc.value("swing",0.f);
    t.useScale   = doc.value("use_scale",false);
    t.scaleId    = doc.value("scale_id","");
    t.active     = doc.value("active",true);
    // Initialise steps.
    t.steps.resize(t.stepCount);
    m_sequencer.setTrack(t);
    return json{{"type","track_set"},{"track_id",t.trackId}}.dump();
}

std::string MessageHandler::handleSetStep(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    uint8_t trackId = static_cast<uint8_t>(doc.value("track_id",0));
    uint8_t stepIdx = static_cast<uint8_t>(doc.value("step_idx",0));
    SeqStep s;
    s.active      = doc.value("active",false);
    s.note        = static_cast<uint8_t>(doc.value("note",60));
    s.velocity    = static_cast<uint8_t>(doc.value("velocity",100));
    s.gatePercent = static_cast<uint8_t>(doc.value("gate_percent",75));
    s.accent      = doc.value("accent",false);
    s.slide       = doc.value("slide",false);
    s.pitchOffset = static_cast<int8_t>(doc.value("pitch_offset",0));
    m_sequencer.setStep(trackId, stepIdx, s);
    return json{{"type","step_set"},{"track_id",trackId},{"step_idx",stepIdx}}.dump();
}

std::string MessageHandler::handleClearTrack(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    uint8_t trackId = static_cast<uint8_t>(doc.value("track_id",0));
    m_sequencer.setStepCount(trackId, 16);
    for (uint8_t i = 0; i < 16; ++i)
        m_sequencer.setStep(trackId, i, SeqStep{});
    return json{{"type","track_cleared"},{"track_id",trackId}}.dump();
}

std::string MessageHandler::handleMuteTrack(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    uint8_t trackId = static_cast<uint8_t>(doc.value("track_id",0));
    bool muted = doc.value("muted",true);
    m_sequencer.muteTrack(trackId, muted);
    return json{{"type","track_muted"},{"track_id",trackId},{"muted",muted}}.dump();
}

std::string MessageHandler::handleSoloTrack(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    uint8_t trackId = static_cast<uint8_t>(doc.value("track_id",0));
    bool solo = doc.value("solo",true);
    m_sequencer.soloTrack(trackId, solo);
    return json{{"type","track_soloed"},{"track_id",trackId},{"solo",solo}}.dump();
}

std::string MessageHandler::handleSavePattern(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    const auto& pat = m_sequencer.pattern();
    std::string pid = pat.patternId.empty()
                        ? doc.value("pattern_id","pattern_1")
                        : pat.patternId;
    // Serialise pattern to JSON.
    json pj;
    pj["pattern_id"]  = pid;
    pj["name"]        = pat.name;
    pj["length_bars"] = pat.lengthBars;
    json tracks = json::array();
    for (const auto& t : pat.tracks) {
        json tj;
        tj["track_id"]    = t.trackId;
        tj["name"]        = t.name;
        tj["midi_port"]   = t.midiPort;
        tj["midi_channel"]= t.midiChannel;
        tj["base_note"]   = t.baseNote;
        tj["melodic"]     = t.melodic;
        tj["step_count"]  = t.stepCount;
        tj["step_ticks"]  = t.stepTicks;
        tj["muted"]       = t.muted;
        tj["transpose"]   = t.transpose;
        tj["swing"]       = t.swing;
        tj["use_scale"]   = t.useScale;
        tj["scale_id"]    = t.scaleId;
        json steps = json::array();
        for (const auto& s : t.steps)
            steps.push_back({{"active",s.active},{"note",s.note},
                             {"velocity",s.velocity},{"gate",s.gatePercent},
                             {"accent",s.accent},{"slide",s.slide},
                             {"pitch_offset",s.pitchOffset}});
        tj["steps"] = steps;
        tracks.push_back(tj);
    }
    pj["tracks"] = tracks;
    m_db.savePattern(currentProjectId(), pid, pj.dump());
    return json{{"type","pattern_saved"},{"pattern_id",pid}}.dump();
}

std::string MessageHandler::handleLoadPattern(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    std::string pid = doc.value("pattern_id","");
    std::string raw = m_db.loadPattern(pid);
    if (raw.empty())
        return json{{"type","error"},{"message","pattern not found"}}.dump();

    auto pj = json::parse(raw);
    SeqPattern pat;
    pat.patternId  = pj.value("pattern_id","");
    pat.name       = pj.value("name","");
    pat.lengthBars = pj.value("length_bars",1u);
    pat.tracks.resize(kMaxSeqTracks);
    for (uint8_t i = 0; i < kMaxSeqTracks; ++i) pat.tracks[i].trackId = i;

    for (auto& tj : pj.value("tracks", json::array())) {
        uint8_t tid = static_cast<uint8_t>(tj.value("track_id",0));
        auto& t = pat.tracks[tid];
        t.trackId    = tid;
        t.name       = tj.value("name","");
        t.midiPort   = static_cast<uint8_t>(tj.value("midi_port",0));
        t.midiChannel= static_cast<uint8_t>(tj.value("midi_channel",1));
        t.baseNote   = static_cast<uint8_t>(tj.value("base_note",60));
        t.melodic    = tj.value("melodic",false);
        t.stepCount  = static_cast<uint8_t>(tj.value("step_count",16));
        t.stepTicks  = tj.value("step_ticks", kTicksPerSixteen);
        t.muted      = tj.value("muted",false);
        t.transpose  = static_cast<int8_t>(tj.value("transpose",0));
        t.swing      = tj.value("swing",0.f);
        t.useScale   = tj.value("use_scale",false);
        t.scaleId    = tj.value("scale_id","");
        for (auto& sj : tj.value("steps", json::array())) {
            SeqStep s;
            s.active      = sj.value("active",false);
            s.note        = static_cast<uint8_t>(sj.value("note",60));
            s.velocity    = static_cast<uint8_t>(sj.value("velocity",100));
            s.gatePercent = static_cast<uint8_t>(sj.value("gate",75));
            s.accent      = sj.value("accent",false);
            s.slide       = sj.value("slide",false);
            s.pitchOffset = static_cast<int8_t>(sj.value("pitch_offset",0));
            t.steps.push_back(s);
        }
    }
    m_sequencer.loadPattern(std::move(pat));
    return json{{"type","pattern_loaded"},{"pattern_id",pid}}.dump();
}

std::string MessageHandler::handleListPatterns(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    int pid = doc.value("project_id", currentProjectId());
    auto ids = m_db.loadPatternIds(pid);
    return json{{"type","patterns_list"},{"pattern_ids",ids}}.dump();
}

// ─── LFO commands ─────────────────────────────────────────────────────────────

namespace {
LfoConfig lfoFromJson(const json& j) {
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
json lfoToJson(const LfoConfig& cfg) {
    json j;
    j["lfo_id"]       = cfg.lfoId;
    j["name"]         = cfg.name;
    j["shape"]        = static_cast<int>(cfg.shape);
    j["sync_to_tempo"]= cfg.syncToTempo;
    j["rate_hz"]      = cfg.rateHz;
    j["sync_division"]= cfg.syncDivision;
    j["depth"]        = cfg.depth;
    j["phase_offset"] = cfg.phaseOffset;
    j["bipolar"]      = cfg.bipolar;
    json pts = json::array();
    for (auto& pt : cfg.customPoints)
        pts.push_back({{"phase",pt.phase},{"value",pt.value}});
    j["custom_points"] = pts;
    return j;
}
} // namespace

std::string MessageHandler::handleAddLfo(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    auto cfg = lfoFromJson(doc);
    m_lfos.addLfo(cfg);
    m_db.saveLfo(currentProjectId(), cfg.lfoId, lfoToJson(cfg).dump());
    return json{{"type","lfo_added"},{"lfo_id",cfg.lfoId}}.dump();
}

std::string MessageHandler::handleUpdateLfo(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    auto cfg = lfoFromJson(doc);
    m_lfos.updateConfig(cfg.lfoId, cfg);
    m_db.saveLfo(currentProjectId(), cfg.lfoId, lfoToJson(cfg).dump());
    return json{{"type","lfo_updated"},{"lfo_id",cfg.lfoId}}.dump();
}

std::string MessageHandler::handleRemoveLfo(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    std::string lfoId = doc.value("lfo_id","");
    m_lfos.removeLfo(lfoId);
    m_db.deleteLfo(lfoId);
    return json{{"type","lfo_removed"},{"lfo_id",lfoId}}.dump();
}

std::string MessageHandler::handleListLfos() {
    json arr = json::array();
    for (const auto& cfg : m_lfos.allConfigs())
        arr.push_back(lfoToJson(cfg));
    return json{{"type","lfos_list"},{"lfos",arr}}.dump();
}

std::string MessageHandler::handleGetLfoPreview(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    std::string lfoId = doc.value("lfo_id","");
    int resolution    = doc.value("resolution", 256);
    resolution = std::clamp(resolution, 8, 1024);

    LfoConfig cfg;
    if (!lfoId.empty()) {
        cfg = m_lfos.getConfig(lfoId);
    } else {
        cfg = lfoFromJson(doc.value("config", json{}));
    }

    auto values = LfoShape::preview(cfg, resolution);
    return json{{"type","lfo_preview"},{"lfo_id",lfoId},{"values",values}}.dump();
}

// ─── Mod matrix commands ──────────────────────────────────────────────────────

namespace {
ModRoute routeFromJson(const json& j) {
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
json routeToJson(const ModRoute& r) {
    return {{"route_id",r.routeId},{"source",(int)r.source},{"source_id",r.sourceId},
            {"dest",(int)r.dest},{"dest_id",r.destId},{"amount",r.amount},
            {"offset",r.offset},{"enabled",r.enabled}};
}
} // namespace

std::string MessageHandler::handleAddModRoute(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    auto route = routeFromJson(doc);
    m_modMatrix.addRoute(route);
    m_db.saveModRoute(currentProjectId(), route.routeId, routeToJson(route).dump());
    return json{{"type","mod_route_added"},{"route_id",route.routeId}}.dump();
}

std::string MessageHandler::handleUpdateModRoute(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    auto route = routeFromJson(doc);
    m_modMatrix.updateRoute(route);
    m_db.saveModRoute(currentProjectId(), route.routeId, routeToJson(route).dump());
    return json{{"type","mod_route_updated"},{"route_id",route.routeId}}.dump();
}

std::string MessageHandler::handleRemoveModRoute(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    std::string routeId = doc.value("route_id","");
    m_modMatrix.removeRoute(routeId);
    m_db.deleteModRoute(routeId);
    return json{{"type","mod_route_removed"},{"route_id",routeId}}.dump();
}

std::string MessageHandler::handleListModRoutes() {
    json arr = json::array();
    for (const auto& r : m_modMatrix.allRoutes())
        arr.push_back(routeToJson(r));
    return json{{"type","mod_routes_list"},{"routes",arr}}.dump();
}

// ─── Scale commands ───────────────────────────────────────────────────────────

std::string MessageHandler::handleListScales() {
    json arr = json::array();
    for (const auto& s : m_scales.allScales()) {
        json deg = json::array();
        for (bool b : s.degrees) deg.push_back(b);
        arr.push_back({{"scale_id",s.scaleId},{"name",s.name},
                       {"mode",(int)s.mode},{"root",s.root},{"degrees",deg}});
    }
    return json{{"type","scales_list"},{"scales",arr}}.dump();
}

std::string MessageHandler::handleAddScale(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    Scale s;
    s.scaleId = doc.value("scale_id","");
    s.name    = doc.value("name","Custom");
    s.root    = doc.value("root",0);
    s.mode    = ScaleMode::Custom;
    auto deg  = doc.value("degrees", std::vector<bool>(12,false));
    for (int i = 0; i < 12 && i < (int)deg.size(); ++i) s.degrees[i] = deg[i];
    m_scales.addScale(s);
    json sj = {{"scale_id",s.scaleId},{"name",s.name},{"root",s.root}};
    json dj = json::array();
    for (bool b : s.degrees) dj.push_back(b);
    sj["degrees"] = dj;
    m_db.saveCustomScale(currentProjectId(), s.scaleId, sj.dump());
    return json{{"type","scale_added"},{"scale_id",s.scaleId}}.dump();
}

std::string MessageHandler::handleRemoveScale(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    std::string scaleId = doc.value("scale_id","");
    m_scales.removeScale(scaleId);
    m_db.deleteCustomScale(scaleId);
    return json{{"type","scale_removed"},{"scale_id",scaleId}}.dump();
}

// ─── MIDI input commands ──────────────────────────────────────────────────────

std::string MessageHandler::handleDiscoverMidiInputs() {
    auto ports = MidiInput::instance().discover();
    json arr = json::array();
    for (const auto& p : ports)
        arr.push_back({{"id",p.id},{"name",p.name},{"hw_addr",p.hwAddr},{"open",p.open}});
    return json{{"type","midi_inputs_list"},{"ports",arr}}.dump();
}

std::string MessageHandler::handleOpenMidiInput(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    int portId = doc.value("port_id",0);
    bool ok = MidiInput::instance().open(portId);
    return json{{"type","midi_input_opened"},{"port_id",portId},{"success",ok}}.dump();
}

std::string MessageHandler::handleCloseMidiInput(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    int portId = doc.value("port_id",0);
    MidiInput::instance().close(portId);
    return json{{"type","midi_input_closed"},{"port_id",portId}}.dump();
}

// ─── Broadcast builders ───────────────────────────────────────────────────────

std::string MessageHandler::buildProjectState() const {
    return const_cast<MessageHandler*>(this)->handleGetProjectState();
}

std::string MessageHandler::buildControlValues() const {
    json res;
    res["type"] = "control_values";
    for (const auto& [id, v] : m_snapshots.currentValues())
        res["values"][id] = v;
    return res.dump();
}

std::string MessageHandler::buildSequencerState() const {
    return const_cast<MessageHandler*>(this)->handleGetSequencerState();
}

// ─── Arp commands ─────────────────────────────────────────────────────────────

std::string MessageHandler::handleSetArp(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    uint8_t trackId = doc.value("track_id", 0);

    vortex::ArpConfig cfg;
    cfg.enabled     = doc.value("enabled",      false);
    cfg.mode        = static_cast<vortex::ArpMode>(doc.value("mode", 0));
    cfg.rateTicks   = doc.value("rate_ticks",    240);
    cfg.octaveRange = doc.value("octave_range",  1);
    cfg.latch       = doc.value("latch",         false);
    cfg.gatePercent = doc.value("gate_percent",  50);

    m_arp.setConfig(trackId, cfg);
    return json{{"type","arp_set"},{"track_id",trackId}}.dump();
}

std::string MessageHandler::handleGetArp(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    uint8_t trackId = doc.value("track_id", 0);
    const auto cfg = m_arp.getConfig(trackId);
    return json{
        {"type",         "arp_config"},
        {"track_id",     trackId},
        {"enabled",      cfg.enabled},
        {"mode",         static_cast<int>(cfg.mode)},
        {"rate_ticks",   cfg.rateTicks},
        {"octave_range", cfg.octaveRange},
        {"latch",        cfg.latch},
        {"gate_percent", cfg.gatePercent},
    }.dump();
}

// ─── Arrange commands ─────────────────────────────────────────────────────────

std::string MessageHandler::handleArrangeSetClip(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    uint8_t     trackId   = doc.value("track_id",  0);
    uint32_t    bar       = doc.value("bar",        0u);
    uint32_t    length    = doc.value("length",     1u);
    std::string patternId = doc.value("pattern_id", std::string{});
    m_arrange.setClip(trackId, bar, length, patternId);
    return json{{"type","arrange_clip_set"},{"track_id",trackId},{"bar",bar}}.dump();
}

std::string MessageHandler::handleArrangeRemoveClip(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    uint8_t  trackId = doc.value("track_id", 0);
    uint32_t bar     = doc.value("bar",      0u);
    m_arrange.removeClip(trackId, bar);
    return json{{"type","arrange_clip_removed"},{"track_id",trackId},{"bar",bar}}.dump();
}

std::string MessageHandler::handleArrangeGetClips() {
    json arr = json::array();
    for (const auto& c : m_arrange.clips()) {
        arr.push_back({
            {"track_id",   c.trackId},
            {"bar",        c.startBar},
            {"length",     c.length},
            {"pattern_id", c.patternId},
        });
    }
    return json{{"type","arrange_clips"},{"clips",arr}}.dump();
}
