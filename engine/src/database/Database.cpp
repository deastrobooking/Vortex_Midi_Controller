#include "Database.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

Database::Database()  = default;
Database::~Database() { close(); }

bool Database::open(const std::string& path) {
    int rc = sqlite3_open(path.c_str(), &m_db);
    if (rc != SQLITE_OK) { m_db = nullptr; return false; }

    // Performance pragmas.
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA synchronous=NORMAL;");
    exec("PRAGMA foreign_keys=ON;");
    return true;
}

void Database::close() {
    if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
}

bool Database::applySchema(const std::string& schemaPath) {
    std::ifstream f(schemaPath);
    if (!f.is_open()) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    return exec(ss.str());
}

bool Database::exec(const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        if (err) sqlite3_free(err);
        return false;
    }
    return true;
}

int Database::lastId() const {
    return static_cast<int>(sqlite3_last_insert_rowid(m_db));
}

// ─── Projects  ───────────────────────────────────────────────────────────────

bool Database::saveProject(Project& proj) {
    sqlite3_stmt* stmt;
    const char* sql = proj.id == 0
        ? "INSERT INTO projects (name, tempo) VALUES (?, ?) RETURNING id;"
        : "UPDATE projects SET name=?, tempo=? WHERE id=? RETURNING id;";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt,  1, proj.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, proj.tempo);
    if (proj.id != 0) sqlite3_bind_int(stmt, 3, proj.id);

    bool ok = (sqlite3_step(stmt) == SQLITE_ROW);
    if (ok && proj.id == 0) proj.id = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<Database::Project> Database::listProjects() const {
    std::vector<Project> result;
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db,
            "SELECT id, name, tempo, last_modified FROM projects ORDER BY id;",
            -1, &stmt, nullptr) != SQLITE_OK)
        return result;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Project p;
        p.id           = sqlite3_column_int(stmt, 0);
        p.name         = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        p.tempo        = sqlite3_column_double(stmt, 2);
        p.lastModified = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        result.push_back(std::move(p));
    }
    sqlite3_finalize(stmt);
    return result;
}

Database::Project Database::loadProject(int id) const {
    Project p;
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db,
            "SELECT id, name, tempo FROM projects WHERE id=?;",
            -1, &stmt, nullptr) != SQLITE_OK)
        return p;

    sqlite3_bind_int(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        p.id    = sqlite3_column_int(stmt, 0);
        p.name  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        p.tempo = sqlite3_column_double(stmt, 2);
    }
    sqlite3_finalize(stmt);
    return p;
}

bool Database::deleteProject(int id) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(m_db, "DELETE FROM projects WHERE id=?;", -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// ─── MIDI mappings ───────────────────────────────────────────────────────────

bool Database::saveMappingJson(int projectId, const std::string& json) {
    sqlite3_stmt* stmt;
    const char* sql = R"(
        INSERT INTO project_mappings (project_id, mapping_json)
        VALUES (?, ?)
        ON CONFLICT(project_id) DO UPDATE SET mapping_json=excluded.mapping_json;
    )";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, projectId);
    sqlite3_bind_text(stmt, 2, json.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::string Database::loadMappingJson(int projectId) const {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(m_db,
        "SELECT mapping_json FROM project_mappings WHERE project_id=?;",
        -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, projectId);
    std::string result;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);
    return result;
}

// ─── Scenes ──────────────────────────────────────────────────────────────────

bool Database::saveScene(int projectId, const SceneValues& sv) {
    exec("BEGIN;");

    sqlite3_stmt* stmt;
    const char* sql = R"(
        INSERT INTO scenes (scene_id, project_id, name)
        VALUES (?, ?, ?)
        ON CONFLICT(scene_id) DO UPDATE SET name=excluded.name;
    )";
    sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, sv.sceneId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  2, projectId);
    sqlite3_bind_text(stmt, 3, sv.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Delete old values then re-insert.
    sqlite3_prepare_v2(m_db,
        "DELETE FROM scene_values WHERE scene_id=?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, sv.sceneId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    for (size_t i = 0; i < sv.controlIds.size(); ++i) {
        sqlite3_prepare_v2(m_db,
            "INSERT INTO scene_values (scene_id, control_id, value) VALUES (?,?,?);",
            -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, sv.sceneId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, sv.controlIds[i].c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt,  3, sv.values[i]);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    return exec("COMMIT;");
}

std::vector<SceneValues> Database::loadScenes(int projectId) const {
    std::vector<SceneValues> result;
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(m_db,
        "SELECT scene_id, name FROM scenes WHERE project_id=?;",
        -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, projectId);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SceneValues sv;
        sv.sceneId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        sv.name    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

        sqlite3_stmt* vs;
        sqlite3_prepare_v2(m_db,
            "SELECT control_id, value FROM scene_values WHERE scene_id=?;",
            -1, &vs, nullptr);
        sqlite3_bind_text(vs, 1, sv.sceneId.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(vs) == SQLITE_ROW) {
            sv.controlIds.push_back(
                reinterpret_cast<const char*>(sqlite3_column_text(vs, 0)));
            sv.values.push_back(sqlite3_column_int(vs, 1));
        }
        sqlite3_finalize(vs);
        result.push_back(std::move(sv));
    }
    sqlite3_finalize(stmt);
    return result;
}

bool Database::deleteScene(const std::string& sceneId) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(m_db, "DELETE FROM scenes WHERE scene_id=?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, sceneId.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// ─── Automation clips ─────────────────────────────────────────────────────────

bool Database::saveClip(int projectId, const AutomationClip& clip) {
    exec("BEGIN;");
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(m_db, R"(
        INSERT INTO automation_clips (clip_id, project_id, length_ticks, loop)
        VALUES (?,?,?,?)
        ON CONFLICT(clip_id) DO UPDATE SET length_ticks=excluded.length_ticks, loop=excluded.loop;
    )", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, clip.clipId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  2, projectId);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(clip.lengthTicks));
    sqlite3_bind_int(stmt,  4, clip.loop ? 1 : 0);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    for (const auto& lane : clip.lanes) {
        sqlite3_prepare_v2(m_db, R"(
            INSERT INTO automation_lanes (clip_id, control_id) VALUES (?,?)
            ON CONFLICT DO NOTHING;
        )", -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, clip.clipId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, lane.controlId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        int laneId = lastId();
        sqlite3_finalize(stmt);

        for (const auto& pt : lane.points) {
            sqlite3_prepare_v2(m_db,
                "INSERT INTO automation_points (lane_id, tick, value) VALUES (?,?,?);",
                -1, &stmt, nullptr);
            sqlite3_bind_int(stmt,  1, laneId);
            sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(pt.tick));
            sqlite3_bind_int(stmt,  3, pt.value);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    return exec("COMMIT;");
}

AutomationClip Database::loadClip(const std::string& clipId) const {
    AutomationClip clip;
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(m_db,
        "SELECT clip_id, length_ticks, loop FROM automation_clips WHERE clip_id=?;",
        -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, clipId.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_ROW) { sqlite3_finalize(stmt); return clip; }
    clip.clipId      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    clip.lengthTicks = static_cast<uint64_t>(sqlite3_column_int64(stmt, 1));
    clip.loop        = sqlite3_column_int(stmt, 2) != 0;
    sqlite3_finalize(stmt);

    sqlite3_prepare_v2(m_db,
        "SELECT id, control_id FROM automation_lanes WHERE clip_id=?;",
        -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, clipId.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AutomationLane lane;
        int laneId = sqlite3_column_int(stmt, 0);
        lane.controlId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

        sqlite3_stmt* ps;
        sqlite3_prepare_v2(m_db,
            "SELECT tick, value FROM automation_points WHERE lane_id=? ORDER BY tick;",
            -1, &ps, nullptr);
        sqlite3_bind_int(ps, 1, laneId);
        while (sqlite3_step(ps) == SQLITE_ROW) {
            AutomationPoint pt;
            pt.tick      = static_cast<uint64_t>(sqlite3_column_int64(ps, 0));
            pt.value     = sqlite3_column_int(ps, 1);
            pt.controlId = lane.controlId;
            lane.points.push_back(pt);
        }
        sqlite3_finalize(ps);
        clip.lanes.push_back(std::move(lane));
    }
    sqlite3_finalize(stmt);
    return clip;
}

std::vector<AutomationClip> Database::loadClips(int projectId) const {
    std::vector<AutomationClip> result;
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(m_db,
        "SELECT clip_id FROM automation_clips WHERE project_id=?;",
        -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, projectId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        result.push_back(loadClip(id));
    }
    sqlite3_finalize(stmt);
    return result;
}

bool Database::deleteClip(const std::string& clipId) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(m_db,
        "DELETE FROM automation_clips WHERE clip_id=?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, clipId.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// ─── Settings ─────────────────────────────────────────────────────────────────

std::string Database::getSetting(const std::string& key,
                                  const std::string& def) const {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(m_db,
        "SELECT value FROM settings WHERE key=?;", -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    std::string result = def;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);
    return result;
}

void Database::setSetting(const std::string& key, const std::string& value) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(m_db,
        "INSERT INTO settings (key, value) VALUES (?,?) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value;",
        -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}
