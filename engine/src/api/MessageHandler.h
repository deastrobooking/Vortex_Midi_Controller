#pragma once
#include <functional>
#include <string>

// Forward declarations
class ClockEngine;
class MidiRouter;
class SnapshotEngine;
class AutomationRecorder;
class AutomationPlayback;
class MacroEngine;
class Database;

// Parses incoming WebSocket JSON messages and dispatches to engine subsystems.
// Also formats outgoing JSON responses.

class MessageHandler {
public:
    MessageHandler(ClockEngine&         clock,
                   MidiRouter&          router,
                   SnapshotEngine&      snapshots,
                   AutomationRecorder&  recorder,
                   AutomationPlayback&  playback,
                   MacroEngine&         macros,
                   Database&            db);

    // Returns a JSON response string (may be empty if no reply needed).
    std::string handle(const std::string& json);

    // Build a JSON state broadcast for connected UI clients.
    std::string buildProjectState() const;
    std::string buildControlValues() const;

private:
    std::string handleGetProjectState();
    std::string handleUpdateMapping(const std::string& json);
    std::string handleTriggerScene(const std::string& json);
    std::string handleSetTempo(const std::string& json);
    std::string handleTransport(const std::string& json);
    std::string handleSaveScene(const std::string& json);
    std::string handleStartRecord(const std::string& json);
    std::string handleStopRecord(const std::string& json);
    std::string handleListScenes();
    std::string handleListProjects();

    ClockEngine&         m_clock;
    MidiRouter&          m_router;
    SnapshotEngine&      m_snapshots;
    AutomationRecorder&  m_recorder;
    AutomationPlayback&  m_playback;
    MacroEngine&         m_macros;
    Database&            m_db;
};
