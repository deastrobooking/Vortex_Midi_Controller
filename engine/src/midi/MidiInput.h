#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct MidiInputPort {
    int         id;
    std::string name;
    std::string hwAddr;   // e.g. "hw:1,0,0"
    bool        open{false};
};

struct MidiInputMessage {
    int      portId{0};
    uint8_t  status{0};   // raw status byte (type | channel), e.g. 0xB0
    uint8_t  channel{0};  // 0–15
    uint8_t  data1{0};
    uint8_t  data2{0};
};

using MidiInputCallback = std::function<void(const MidiInputMessage&)>;

// Singleton ALSA rawmidi input manager.
// Discovers ports, opens selected ports, fires a callback for every
// incoming MIDI message.  Runs a reader thread per open port.
class MidiInput {
public:
    static MidiInput& instance();

    // Enumerate available rawmidi INPUT ports (does not open them).
    std::vector<MidiInputPort> discover();

    // Open a port by id (from discover()).  Returns false on failure.
    bool open(int portId);
    void close(int portId);
    void closeAll();

    // Register callback (replaces any previous one).
    void onMessage(MidiInputCallback cb);

    std::vector<MidiInputPort> openPorts() const;

private:
    MidiInput();
    ~MidiInput();

    void readerThread(int portId);

    struct Port {
        MidiInputPort info;
        void*         handle{nullptr};   // snd_rawmidi_t* — opaque to header
        std::thread   reader;
        std::atomic<bool> active{false};
    };

    std::vector<Port>    m_ports;     // discovered ports
    MidiInputCallback    m_callback;
    mutable std::mutex   m_mtx;
};
