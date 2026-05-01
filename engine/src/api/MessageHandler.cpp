#include "MessageHandler.h"
#include "../core/ClockEngine.h"
#include "../core/MidiRouter.h"
#include "../core/SnapshotEngine.h"
#include "../core/AutomationRecorder.h"
#include "../core/AutomationPlayback.h"
#include "../core/MacroEngine.h"
#include "../database/Database.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

MessageHandler::MessageHandler(ClockEngine& clock,
                               MidiRouter& router,
                               SnapshotEngine& snapshots,
                               AutomationRecorder& recorder,
                               AutomationPlayback& playback,
                               MacroEngine& macros,
                               Database& db)
    : m_clock(clock), m_router(router), m_snapshots(snapshots),
      m_recorder(recorder), m_playback(playback), m_macros(macros), m_db(db)
{}

std::string MessageHandler::handle(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded() || !doc.contains("type")) return {};

    std::string type = doc["type"];

    if (type == "get_project_state")   return handleGetProjectState();
    if (type == "update_mapping")      return handleUpdateMapping(jsonStr);
    if (type == "trigger_scene")       return handleTriggerScene(jsonStr);
    if (type == "set_tempo")           return handleSetTempo(jsonStr);
    if (type == "transport")           return handleTransport(jsonStr);
    if (type == "save_scene")          return handleSaveScene(jsonStr);
    if (type == "start_record")        return handleStartRecord(jsonStr);
    if (type == "stop_record")         return handleStopRecord(jsonStr);
    if (type == "list_scenes")         return handleListScenes();
    if (type == "list_projects")       return handleListProjects();

    return json{{"type", "error"}, {"message", "unknown message type"}}.dump();
}

std::string MessageHandler::handleGetProjectState() {
    json res;
    res["type"]          = "project_state";
    res["tempo"]         = m_clock.getTempo();
    res["playing"]       = (m_clock.transportState() == TransportState::Playing);
    res["current_tick"]  = m_clock.currentTick();
    res["current_beat"]  = m_clock.currentBeat();
    res["current_bar"]   = m_clock.currentBar();
    res["scene_ids"]     = m_snapshots.sceneIds();
    return res.dump();
}

std::string MessageHandler::handleUpdateMapping(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    std::string controlId = doc["control_id"];
    auto& jt = doc["target"];

    MidiTarget t;
    t.portId   = jt.value("port_id",  0);
    t.channel  = jt.value("channel",  1);
    t.ccNumber = jt.value("cc",       0);
    t.minValue = jt.value("min",      0);
    t.maxValue = jt.value("max",    127);
    t.curve    = jt.value("curve", "linear");
    t.invert   = jt.value("invert", false);

    m_router.removeTargets(controlId);
    m_router.setTarget(controlId, t);

    return json{{"type", "mapping_updated"}, {"control_id", controlId}}.dump();
}

std::string MessageHandler::handleTriggerScene(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    std::string sceneId = doc["scene_id"];
    std::string modeStr = doc.value("mode", "jump");
    uint32_t durationBars = doc.value("duration_bars", 4);

    SceneRecallMode mode = SceneRecallMode::Jump;
    if (modeStr == "fade")  mode = SceneRecallMode::Fade;
    if (modeStr == "drop")  mode = SceneRecallMode::Drop;
    if (modeStr == "morph") mode = SceneRecallMode::Morph;

    uint32_t durationTicks = durationBars * kTicksPerBar4_4;
    m_snapshots.recallScene(sceneId, mode, durationTicks);

    return json{{"type", "scene_triggered"}, {"scene_id", sceneId}}.dump();
}

std::string MessageHandler::handleSetTempo(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    m_clock.setTempo(doc.value("tempo", 120.0));
    return json{{"type", "tempo_set"}, {"tempo", m_clock.getTempo()}}.dump();
}

std::string MessageHandler::handleTransport(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    std::string action = doc.value("action", "stop");
    if (action == "play")     m_clock.play();
    else if (action == "stop") m_clock.stop();
    else if (action == "continue") m_clock.continueClock();
    else if (action == "tap") m_clock.tapTempo();
    return json{{"type", "transport_ack"}, {"action", action}}.dump();
}

std::string MessageHandler::handleSaveScene(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr);
    std::string sceneId = doc.value("scene_id", "");
    std::string name    = doc.value("name", "Untitled");
    int projectId       = doc.value("project_id", 1);

    if (sceneId.empty()) {
        return json{{"type","error"},{"message","scene_id required"}}.dump();
    }

    SceneValues sv = m_snapshots.captureScene(sceneId, name);
    m_snapshots.addScene(sv);
    m_db.saveScene(projectId, sv);

    return json{{"type","scene_saved"},{"scene_id", sceneId}}.dump();
}

std::string MessageHandler::handleStartRecord(const std::string& /*json*/) {
    m_recorder.startRecord(m_clock.currentTick());
    return json{{"type","record_started"}}.dump();
}

std::string MessageHandler::handleStopRecord(const std::string& /*json*/) {
    m_recorder.stopRecord(m_clock.currentTick());
    return json{{"type","record_stopped"}}.dump();
}

std::string MessageHandler::handleListScenes() {
    json res;
    res["type"]   = "scenes_list";
    res["scenes"] = m_snapshots.sceneIds();
    return res.dump();
}

std::string MessageHandler::handleListProjects() {
    auto projects = m_db.listProjects();
    json arr = json::array();
    for (const auto& p : projects) {
        arr.push_back({{"id", p.id}, {"name", p.name}, {"tempo", p.tempo}});
    }
    return json{{"type","projects_list"},{"projects", arr}}.dump();
}

std::string MessageHandler::buildProjectState() const {
    return const_cast<MessageHandler*>(this)->handleGetProjectState();
}

std::string MessageHandler::buildControlValues() const {
    json res;
    res["type"] = "control_values";
    auto& vals  = m_snapshots.currentValues();
    for (const auto& [id, v] : vals) res["values"][id] = v;
    return res.dump();
}
