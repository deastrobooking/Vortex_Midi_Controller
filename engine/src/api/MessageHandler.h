#pragma once
#include <functional>
#include <string>

class ClockEngine;
class MidiRouter;
class SnapshotEngine;
class AutomationRecorder;
class AutomationPlayback;
class MacroEngine;
class SequencerEngine;
class LfoEngine;
class ModMatrix;
class ScaleEngine;
class Database;
namespace vortex { class ArpEngine; class ArrangeEngine; }

// Parses incoming WebSocket JSON messages and dispatches to engine subsystems.
// Also formats outgoing JSON responses / broadcasts.
class MessageHandler {
public:
    MessageHandler(ClockEngine&              clock,
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
                   vortex::ArrangeEngine&    arrange);

    // Returns a JSON response string (may be empty if no reply needed).
    std::string handle(const std::string& json);

    // Build JSON broadcast payloads for the UI.
    std::string buildProjectState() const;
    std::string buildControlValues() const;
    std::string buildSequencerState() const;  // playheads + active steps

private:
    // ── Existing commands ────────────────────────────────────────────────
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
    std::string handleAddMacro(const std::string& json);
    std::string handleRemoveMacro(const std::string& json);
    std::string handleListMacros();

    // ── Sequencer commands ───────────────────────────────────────────────
    std::string handleGetSequencerState();
    std::string handleSetTrack(const std::string& json);
    std::string handleSetStep(const std::string& json);
    std::string handleClearTrack(const std::string& json);
    std::string handleMuteTrack(const std::string& json);
    std::string handleSoloTrack(const std::string& json);
    std::string handleSavePattern(const std::string& json);
    std::string handleLoadPattern(const std::string& json);
    std::string handleListPatterns(const std::string& json);

    // ── LFO commands ─────────────────────────────────────────────────────
    std::string handleAddLfo(const std::string& json);
    std::string handleUpdateLfo(const std::string& json);
    std::string handleRemoveLfo(const std::string& json);
    std::string handleListLfos();
    std::string handleGetLfoPreview(const std::string& json);

    // ── Mod matrix commands ──────────────────────────────────────────────
    std::string handleAddModRoute(const std::string& json);
    std::string handleUpdateModRoute(const std::string& json);
    std::string handleRemoveModRoute(const std::string& json);
    std::string handleListModRoutes();

    // ── Scale commands ────────────────────────────────────────────────────
    std::string handleListScales();
    std::string handleAddScale(const std::string& json);
    std::string handleRemoveScale(const std::string& json);

    // ── MIDI input commands ───────────────────────────────────────────────
    std::string handleDiscoverMidiInputs();
    std::string handleOpenMidiInput(const std::string& json);
    std::string handleCloseMidiInput(const std::string& json);

    // ── Arp commands ──────────────────────────────────────────────────────
    std::string handleSetArp(const std::string& json);
    std::string handleGetArp(const std::string& json);

    // ── Arrange commands ──────────────────────────────────────────────────
    std::string handleArrangeSetClip(const std::string& json);
    std::string handleArrangeRemoveClip(const std::string& json);
    std::string handleArrangeGetClips();

    // ── Helpers ───────────────────────────────────────────────────────────
    int currentProjectId() const;

    ClockEngine&              m_clock;
    MidiRouter&               m_router;
    SnapshotEngine&           m_snapshots;
    AutomationRecorder&       m_recorder;
    AutomationPlayback&       m_playback;
    MacroEngine&              m_macros;
    SequencerEngine&          m_sequencer;
    LfoEngine&                m_lfos;
    ModMatrix&                m_modMatrix;
    ScaleEngine&              m_scales;
    Database&                 m_db;
    vortex::ArpEngine&        m_arp;
    vortex::ArrangeEngine&    m_arrange;
};
