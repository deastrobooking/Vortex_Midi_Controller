#include "MidiOutput.h"
#include <alsa/asoundlib.h>
#include <mutex>
#include <vector>
#include <cstring>

struct MidiOutput::Impl {
    struct Port {
        int               id;
        std::string       name;
        snd_rawmidi_t*    handle{nullptr};
        bool              open{false};
    };

    std::vector<Port> ports;
    std::mutex        mtx;
};

MidiOutput& MidiOutput::instance() {
    static MidiOutput s;
    return s;
}

MidiOutput::MidiOutput() : m_impl(new Impl()) {}

MidiOutput::~MidiOutput() {
    closeAll();
    delete m_impl;
}

void MidiOutput::openAll() {
    std::lock_guard<std::mutex> lock(m_impl->mtx);

    // Enumerate ALSA rawmidi output devices.
    int card = -1;
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
            snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_OUTPUT);

            if (snd_ctl_rawmidi_info(ctl, info) < 0) continue;

            char devName[64];
            snprintf(devName, sizeof(devName), "hw:%d,%d", card, dev);

            snd_rawmidi_t* handle = nullptr;
            if (snd_rawmidi_open(nullptr, &handle, devName, SND_RAWMIDI_NONBLOCK) == 0) {
                int portId = static_cast<int>(m_impl->ports.size());
                m_impl->ports.push_back(
                    {portId, std::string(snd_rawmidi_info_get_name(info)), handle, true});
            }
        }

        snd_ctl_close(ctl);
    }
}

void MidiOutput::closeAll() {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    for (auto& p : m_impl->ports) {
        if (p.handle) {
            snd_rawmidi_flush(p.handle);
            snd_rawmidi_close(p.handle);
            p.handle = nullptr;
            p.open   = false;
        }
    }
}

void MidiOutput::sendRaw(int portId, const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    if (portId < 0 || portId >= static_cast<int>(m_impl->ports.size())) return;
    auto& p = m_impl->ports[portId];
    if (!p.open || !p.handle) return;
    snd_rawmidi_write(p.handle, data, len);
}

void MidiOutput::sendCC(int portId, uint8_t channel, uint8_t cc, uint8_t value) {
    uint8_t msg[3] = {
        static_cast<uint8_t>(0xB0 | (channel & 0x0F)),
        static_cast<uint8_t>(cc    & 0x7F),
        static_cast<uint8_t>(value & 0x7F)
    };
    sendRaw(portId, msg, 3);
}

void MidiOutput::sendNoteOn(int portId, uint8_t channel, uint8_t note,
                             uint8_t velocity) {
    uint8_t msg[3] = {
        static_cast<uint8_t>(0x90 | (channel & 0x0F)),
        static_cast<uint8_t>(note     & 0x7F),
        static_cast<uint8_t>(velocity & 0x7F)
    };
    sendRaw(portId, msg, 3);
}

void MidiOutput::sendNoteOff(int portId, uint8_t channel, uint8_t note) {
    uint8_t msg[3] = {
        static_cast<uint8_t>(0x80 | (channel & 0x0F)),
        static_cast<uint8_t>(note & 0x7F),
        0x00
    };
    sendRaw(portId, msg, 3);
}

void MidiOutput::sendClock(int portId) {
    uint8_t msg = 0xF8;
    sendRaw(portId, &msg, 1);
}

void MidiOutput::sendStart(int portId) {
    uint8_t msg = 0xFA;
    sendRaw(portId, &msg, 1);
}

void MidiOutput::sendStop(int portId) {
    uint8_t msg = 0xFC;
    sendRaw(portId, &msg, 1);
}

void MidiOutput::sendContinue(int portId) {
    uint8_t msg = 0xFB;
    sendRaw(portId, &msg, 1);
}

std::vector<MidiOutput::PortInfo> MidiOutput::listPorts() const {
    std::lock_guard<std::mutex> lock(m_impl->mtx);
    std::vector<PortInfo> result;
    result.reserve(m_impl->ports.size());
    for (const auto& p : m_impl->ports)
        result.push_back({p.id, p.name, p.open});
    return result;
}
