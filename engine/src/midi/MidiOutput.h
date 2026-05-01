#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Singleton wrapper around ALSA RawMidi output ports.
// Thread-safe: all public methods acquire an internal mutex.
class MidiOutput {
public:
    static MidiOutput& instance();

    // Open all available MIDI output ports.
    void openAll();
    void closeAll();

    // Send a 3-byte MIDI CC message.
    void sendCC(int portId, uint8_t channel, uint8_t cc, uint8_t value);

    // Send a Note On.
    void sendNoteOn(int portId, uint8_t channel, uint8_t note, uint8_t velocity);

    // Send a Note Off.
    void sendNoteOff(int portId, uint8_t channel, uint8_t note);

    // Send raw bytes.
    void sendRaw(int portId, const uint8_t* data, size_t len);

    // Send MIDI clock tick (0xF8).
    void sendClock(int portId);

    // Send MIDI Start (0xFA) / Stop (0xFC) / Continue (0xFB).
    void sendStart(int portId);
    void sendStop(int portId);
    void sendContinue(int portId);

    struct PortInfo {
        int         id;
        std::string name;
        bool        open;
    };

    std::vector<PortInfo> listPorts() const;

private:
    MidiOutput();
    ~MidiOutput();

    struct Impl;
    Impl* m_impl{nullptr};
};
