#include "MidiRouter.h"
#include "../midi/MidiOutput.h"
#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

MidiRouter::MidiRouter()  = default;
MidiRouter::~MidiRouter() = default;

void MidiRouter::setTarget(const std::string& controlId, MidiTarget target) {
    m_map[controlId].push_back(std::move(target));
}

void MidiRouter::removeTargets(const std::string& controlId) {
    m_map.erase(controlId);
}

std::vector<MidiTarget> MidiRouter::getTargets(const std::string& controlId) const {
    auto it = m_map.find(controlId);
    if (it == m_map.end()) return {};
    return it->second;
}

void MidiRouter::onEvent(ControlEventCallback cb) {
    m_callback = std::move(cb);
}

bool MidiRouter::route(const ControlEvent& evt) {
    auto it = m_map.find(evt.controlId);
    if (it == m_map.end()) return false;

    bool sent = false;

    for (const auto& t : it->second) {
        int scaled = applyScaling(evt.value, t);
        MidiOutput::instance().sendCC(t.portId, t.channel - 1, t.ccNumber, scaled);
        sent = true;
    }

    if (sent && m_callback) m_callback(evt);
    return sent;
}

int MidiRouter::applyScaling(int raw, const MidiTarget& t) const {
    // raw is expected in 0–4095 (12-bit fader) or 0–127 (7-bit CC).
    // Normalise to [0,1], apply curve, then map to [minValue, maxValue].
    double norm = std::clamp(static_cast<double>(raw) / 4095.0, 0.0, 1.0);

    if (t.curve == "exponential") {
        norm = norm * norm;
    } else if (t.curve == "logarithmic") {
        norm = (norm > 0) ? std::log(1.0 + norm * (M_E - 1.0)) : 0.0;
    }
    // else: linear – no change

    if (t.invert) norm = 1.0 - norm;

    int range = t.maxValue - t.minValue;
    return static_cast<int>(std::round(t.minValue + norm * range));
}

// ─── JSON serialisation ──────────────────────────────────────────────────────

void MidiRouter::loadMappingJson(const std::string& jsonStr) {
    auto doc = json::parse(jsonStr, nullptr, false);
    if (doc.is_discarded()) return;

    m_map.clear();

    for (auto& [controlId, jTargets] : doc.items()) {
        for (auto& jt : jTargets) {
            MidiTarget t;
            t.portId    = jt.value("port_id",    0);
            t.channel   = jt.value("channel",    1);
            t.ccNumber  = jt.value("cc",         0);
            t.minValue  = jt.value("min",        0);
            t.maxValue  = jt.value("max",      127);
            t.curve     = jt.value("curve", "linear");
            t.invert    = jt.value("invert",  false);
            t.smoothing = jt.value("smoothing", 0.0f);
            m_map[controlId].push_back(std::move(t));
        }
    }
}

std::string MidiRouter::saveMappingJson() const {
    json doc;

    for (const auto& [controlId, targets] : m_map) {
        json arr = json::array();
        for (const auto& t : targets) {
            arr.push_back({
                {"port_id",   t.portId},
                {"channel",   t.channel},
                {"cc",        t.ccNumber},
                {"min",       t.minValue},
                {"max",       t.maxValue},
                {"curve",     t.curve},
                {"invert",    t.invert},
                {"smoothing", t.smoothing}
            });
        }
        doc[controlId] = arr;
    }

    return doc.dump(2);
}
