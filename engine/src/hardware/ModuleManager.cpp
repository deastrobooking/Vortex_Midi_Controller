#include "ModuleManager.h"
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <algorithm>

static int openSerial(const std::string& path, int baud) {
    int fd = open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return -1;

    termios tty{};
    if (tcgetattr(fd, &tty) != 0) { close(fd); return -1; }

    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);

    tty.c_cflag  = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag  = 0;
    tty.c_oflag  = 0;
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cc[VMIN]  = 11;  // block until one full event packet
    tty.c_cc[VTIME] = 1;

    fcntl(fd, F_SETFL, 0); // switch to blocking after config
    if (tcsetattr(fd, TCSANOW, &tty) != 0) { close(fd); return -1; }

    return fd;
}

ModuleManager::ModuleManager()  = default;
ModuleManager::~ModuleManager() {
    for (auto& m : m_modules) disconnectModule(m.id);
}

bool ModuleManager::connectModule(uint8_t moduleId, const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mtx);

    int fd = openSerial(path, B921600);
    if (fd < 0) return false;

    Module mod;
    mod.id     = moduleId;
    mod.port   = path;
    mod.fd     = fd;
    mod.active = true;
    mod.status = {moduleId, true, 0, 0.0};

    mod.reader = std::thread(&ModuleManager::readerThread, this, moduleId, fd);
    m_modules.push_back(std::move(mod));
    return true;
}

void ModuleManager::disconnectModule(uint8_t moduleId) {
    std::lock_guard<std::mutex> lock(m_mtx);

    for (auto& m : m_modules) {
        if (m.id == moduleId) {
            m.active = false;
            if (m.fd >= 0) { close(m.fd); m.fd = -1; }
            if (m.reader.joinable()) m.reader.join();
            m.status.connected = false;
        }
    }
}

void ModuleManager::onEvent(HardwareEventCallback cb) {
    m_callback = std::move(cb);
}

void ModuleManager::readerThread(uint8_t moduleId, int fd) {
    // Packet size from firmware: 11 bytes (magic + moduleId + type + controlId(2) +
    // value(2) + timestamp(4) + checksum)
    static constexpr size_t kPacketSize = 11;
    uint8_t buf[kPacketSize];

    auto* mod = [&]() -> Module* {
        for (auto& m : m_modules)
            if (m.id == moduleId) return &m;
        return nullptr;
    }();

    while (mod && mod->active) {
        ssize_t n = read(fd, buf, kPacketSize);
        if (n < static_cast<ssize_t>(kPacketSize)) continue;
        if (buf[0] != kPktMagic) continue;

        // Validate XOR checksum.
        uint8_t xorChk = 0;
        for (size_t i = 0; i < kPacketSize - 1; ++i) xorChk ^= buf[i];
        if (xorChk != buf[kPacketSize - 1]) continue;

        HardwareEvent evt{};
        evt.moduleId  = buf[1];
        evt.type      = static_cast<HWEventType>(buf[2]);
        evt.controlId = static_cast<uint16_t>(buf[3] | (buf[4] << 8));
        evt.value     = static_cast<int16_t>(buf[5]  | (buf[6] << 8));
        evt.timestamp = static_cast<uint32_t>(
            buf[7] | (buf[8] << 8) | (buf[9] << 16) | (buf[10] << 24));

        if (evt.type == HWEventType::ModuleHeartbeat) {
            mod->status.lastHeartbeatMs = static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
        }

        if (m_callback) m_callback(evt);
    }
}

void ModuleManager::sendLedColor(uint8_t moduleId, uint16_t ledIdx,
                                  uint8_t r, uint8_t g, uint8_t b) {
    std::lock_guard<std::mutex> lock(m_mtx);
    for (auto& m : m_modules) {
        if (m.id != moduleId || m.fd < 0) continue;
        std::lock_guard<std::mutex> wl(m.writeMtx);

        uint8_t pkt[8];
        pkt[0] = kPktMagic;
        pkt[1] = moduleId;
        pkt[2] = 0x10; // CommandType::SetLed
        pkt[3] = ledIdx & 0xFF;
        pkt[4] = (ledIdx >> 8) & 0xFF;
        pkt[5] = r; pkt[6] = g; pkt[7] = b;
        // Append checksum.
        uint8_t xorChk = 0;
        for (int i = 0; i < 7; ++i) xorChk ^= pkt[i];
        uint8_t full[9];
        memcpy(full, pkt, 8);
        full[8] = xorChk;
        write(m.fd, full, 9);
    }
}

void ModuleManager::sendLabel(uint8_t moduleId, uint8_t displayIdx,
                               const std::string& label) {
    std::lock_guard<std::mutex> lock(m_mtx);
    for (auto& m : m_modules) {
        if (m.id != moduleId || m.fd < 0) continue;
        std::lock_guard<std::mutex> wl(m.writeMtx);

        uint8_t len = static_cast<uint8_t>(std::min(label.size(), size_t(8)));
        std::vector<uint8_t> pkt;
        pkt.push_back(kPktMagic);
        pkt.push_back(moduleId);
        pkt.push_back(0x12); // CommandType::SetDisplayLabel
        pkt.push_back(displayIdx);
        pkt.push_back(len);
        for (uint8_t i = 0; i < len; ++i) pkt.push_back(label[i]);
        uint8_t xorChk = 0;
        for (auto b : pkt) xorChk ^= b;
        pkt.push_back(xorChk);
        write(m.fd, pkt.data(), pkt.size());
    }
}

std::vector<ModuleStatus> ModuleManager::statuses() const {
    std::lock_guard<std::mutex> lock(m_mtx);
    std::vector<ModuleStatus> out;
    for (const auto& m : m_modules) out.push_back(m.status);
    return out;
}
