#include "MidiInput.h"
#include <alsa/asoundlib.h>
#include <cstring>

MidiInput& MidiInput::instance() {
    static MidiInput s;
    return s;
}

MidiInput::MidiInput()  = default;
MidiInput::~MidiInput() { closeAll(); }

// ─── Discovery ────────────────────────────────────────────────────────────────

std::vector<MidiInputPort> MidiInput::discover() {
    std::lock_guard lock(m_mtx);
    m_ports.clear();

    int card = -1;
    int nextId = 0;

    while (snd_card_next(&card) == 0 && card >= 0) {
        snd_ctl_t* ctl = nullptr;
        char name[32];
        snprintf(name, sizeof(name), "hw:%d", card);
        if (snd_ctl_open(&ctl, name, 0) < 0) continue;

        snd_rawmidi_info_t* info = nullptr;
        snd_rawmidi_info_alloca(&info);

        int dev = -1;
        while (snd_ctl_rawmidi_next_device(ctl, &dev) == 0 && dev >= 0) {
            snd_rawmidi_info_set_device(info, dev);
            snd_rawmidi_info_set_subdevice(info, 0);
            snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);
            if (snd_ctl_rawmidi_info(ctl, info) < 0) continue;

            char hwAddr[32];
            snprintf(hwAddr, sizeof(hwAddr), "hw:%d,%d", card, dev);

            Port p;
            p.info.id     = nextId++;
            p.info.name   = snd_rawmidi_info_get_name(info);
            p.info.hwAddr = hwAddr;
            p.info.open   = false;
            m_ports.push_back(std::move(p));
        }
        snd_ctl_close(ctl);
    }

    std::vector<MidiInputPort> result;
    for (const auto& p : m_ports) result.push_back(p.info);
    return result;
}

// ─── Open / Close ─────────────────────────────────────────────────────────────

bool MidiInput::open(int portId) {
    std::lock_guard lock(m_mtx);

    for (auto& p : m_ports) {
        if (p.info.id != portId || p.info.open) continue;

        snd_rawmidi_t* handle = nullptr;
        if (snd_rawmidi_open(&handle, nullptr, p.info.hwAddr.c_str(),
                             SND_RAWMIDI_NONBLOCK) < 0) return false;

        p.handle   = handle;
        p.info.open = true;
        p.active   = true;

        int id = portId;
        p.reader = std::thread([this, id]{ readerThread(id); });
        return true;
    }
    return false;
}

void MidiInput::close(int portId) {
    std::lock_guard lock(m_mtx);

    for (auto& p : m_ports) {
        if (p.info.id != portId || !p.info.open) continue;
        p.active = false;
        if (p.reader.joinable()) p.reader.join();
        if (p.handle) {
            snd_rawmidi_close(static_cast<snd_rawmidi_t*>(p.handle));
            p.handle = nullptr;
        }
        p.info.open = false;
    }
}

void MidiInput::closeAll() {
    // Mark all inactive first so threads can exit.
    {
        std::lock_guard lock(m_mtx);
        for (auto& p : m_ports) p.active = false;
    }
    for (auto& p : m_ports)
        if (p.reader.joinable()) p.reader.join();

    std::lock_guard lock(m_mtx);
    for (auto& p : m_ports) {
        if (p.handle) {
            snd_rawmidi_close(static_cast<snd_rawmidi_t*>(p.handle));
            p.handle = nullptr;
        }
        p.info.open = false;
    }
}

void MidiInput::onMessage(MidiInputCallback cb) {
    std::lock_guard lock(m_mtx);
    m_callback = std::move(cb);
}

std::vector<MidiInputPort> MidiInput::openPorts() const {
    std::lock_guard lock(m_mtx);
    std::vector<MidiInputPort> out;
    for (const auto& p : m_ports)
        if (p.info.open) out.push_back(p.info);
    return out;
}

// ─── Reader thread ────────────────────────────────────────────────────────────
// Parses the MIDI byte stream with running-status support.

void MidiInput::readerThread(int portId) {
    snd_rawmidi_t* handle = nullptr;
    {
        std::lock_guard lock(m_mtx);
        for (auto& p : m_ports)
            if (p.info.id == portId) { handle = static_cast<snd_rawmidi_t*>(p.handle); break; }
    }
    if (!handle) return;

    // Switch to blocking mode for the reader thread.
    snd_rawmidi_nonblock(handle, 0);

    uint8_t buf[256];
    uint8_t status = 0;     // running status
    uint8_t data[2]{};
    int     dataExpected = 0;
    int     dataCollected = 0;

    auto bytesForStatus = [](uint8_t s) -> int {
        uint8_t type = s & 0xF0;
        if (s >= 0xF0) {
            // System common / realtime
            switch (s) {
                case 0xF0: return -1;  // SysEx — variable, skip until 0xF7
                case 0xF2: return 2;
                case 0xF1: case 0xF3: return 1;
                default:   return 0;   // 0xF4–0xFF (realtime, 1 byte)
            }
        }
        if (type == 0xC0 || type == 0xD0) return 1;  // Program Change, Aftertouch
        return 2;  // Note On/Off, CC, Pitch Bend, Poly AT
    };

    bool inSysEx = false;

    Port* port = nullptr;
    {
        std::lock_guard lock(m_mtx);
        for (auto& p : m_ports) if (p.info.id == portId) { port = &p; break; }
    }

    while (port && port->active.load()) {
        ssize_t n = snd_rawmidi_read(handle, buf, sizeof(buf));
        if (n <= 0) {
            if (n == -EAGAIN || n == -EINTR) { std::this_thread::yield(); continue; }
            break;  // device disconnected
        }

        MidiInputCallback cb;
        {
            std::lock_guard lock(m_mtx);
            cb = m_callback;
        }

        for (ssize_t i = 0; i < n; ++i) {
            uint8_t b = buf[i];

            // Realtime bytes (0xF8–0xFF) can arrive anywhere in a stream.
            if (b >= 0xF8) {
                if (cb) {
                    MidiInputMessage msg;
                    msg.portId  = portId;
                    msg.status  = b;
                    cb(msg);
                }
                continue;
            }

            if (b == 0xF7) { inSysEx = false; continue; }
            if (inSysEx)   { continue; }

            if (b == 0xF0) { inSysEx = true; status = 0; continue; }

            if (b & 0x80) {
                // New status byte.
                status        = b;
                dataCollected = 0;
                dataExpected  = bytesForStatus(b);
                if (dataExpected == 0 && cb) {
                    MidiInputMessage msg;
                    msg.portId  = portId;
                    msg.status  = status;
                    cb(msg);
                }
                continue;
            }

            // Data byte — use running status.
            if (status == 0 || dataExpected < 0) continue;

            data[dataCollected++] = b;

            if (dataCollected >= dataExpected) {
                if (cb) {
                    MidiInputMessage msg;
                    msg.portId  = portId;
                    msg.status  = status;
                    msg.channel = status & 0x0F;
                    msg.data1   = data[0];
                    msg.data2   = (dataExpected > 1) ? data[1] : 0;
                    cb(msg);
                }
                dataCollected = 0;  // running status: ready for next message
            }
        }
    }
}
