#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>
#include <functional>
#include "../core/Types.h"

class Database {
public:
    Database();
    ~Database();

    bool open(const std::string& path);
    void close();

    bool applySchema(const std::string& schemaPath);

    // ─── Projects ──────────────────────────────────────────────────────────
    struct Project {
        int         id{0};
        std::string name;
        double      tempo{120.0};
        std::string lastModified;
    };

    bool             saveProject(Project& proj);  // inserts or updates; sets proj.id
    std::vector<Project> listProjects() const;
    Project          loadProject(int id) const;
    bool             deleteProject(int id);

    // ─── MIDI mappings ─────────────────────────────────────────────────────
    bool saveMappingJson(int projectId, const std::string& json);
    std::string loadMappingJson(int projectId) const;

    // ─── Scenes ────────────────────────────────────────────────────────────
    bool             saveScene(int projectId, const SceneValues& scene);
    std::vector<SceneValues> loadScenes(int projectId) const;
    bool             deleteScene(const std::string& sceneId);

    // ─── Automation ────────────────────────────────────────────────────────
    bool             saveClip(int projectId, const AutomationClip& clip);
    AutomationClip   loadClip(const std::string& clipId) const;
    std::vector<AutomationClip> loadClips(int projectId) const;
    bool             deleteClip(const std::string& clipId);

    // ─── Settings ──────────────────────────────────────────────────────────
    std::string getSetting(const std::string& key,
                           const std::string& defaultValue = "") const;
    void        setSetting(const std::string& key, const std::string& value);

private:
    bool        exec(const std::string& sql);
    int         lastId() const;

    sqlite3*    m_db{nullptr};
};
